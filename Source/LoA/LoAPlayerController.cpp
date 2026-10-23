// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoAPlayerController.h"
#include "UI/ScreenFogWidget.h"
#include "UI/BossDialogueWidget.h"
#include "UI/SlotDragVisualWidget.h"
#include "Components/Image.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/ZoneNoticeWidget.h"
#include "UI/CinematicOverlayWidget.h"
#include "UI/DefeatWidget.h"
#include "UI/ClearWidget.h"
#include "UI/AbandonRaidWidget.h"
#include "LoAGameInstance.h"
#include "Lobby/RepairStationActor.h"
#include "Lobby/ShopNPCActor.h"
#include "Lobby/RefineNPCActor.h"
#include "UI/ShopWidget.h"
#include "UI/RefineWidget.h"
#include "Item/RefineLibrary.h"
#include "Item/RaidReward.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "UI/MinimapWidget.h"
#include "UI/ResourceBarWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Raid/HexArena.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "LoACharacter.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EnhancedInputComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "LoA.h"
#include "Skill/SkillManagerComponent.h"
#include "Raid/EchidnaBoss.h"
#include "Kismet/GameplayStatics.h"
#include "UI/BossHPWidget.h"
#include "UI/CastBarWidget.h"
#include "UI/HUD_ViewModel.h"
#include "UI/SkillTree_ViewModel.h"
#include "Blueprint/UserWidget.h"
#include "View/MVVMView.h"
#include "Item/BattleItemComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemTypes.h"
#include "Stat/StatComponent.h"
#include "UI/InventoryWidget.h"
#include "UI/CharacterInfoWidget.h"
#include "UI/EngravingSetupWidget.h"
#include "UI/GemFuseWidget.h"
#include "Item/GemComponent.h"
#include "Stat/EngravingComponent.h"
#include "UI/CharacterPreviewActor.h"
#include "UI/DamageMeterWidget.h"
#include "Stat/DamageMeterComponent.h"
#include "Item/EquipmentComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

ALoAPlayerController::ALoAPlayerController()
{
	bIsTouch = false;
	bMoveToMouseCursor = false;
	ShortPressThreshold = 0.5f;

	// create the path following comp
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));
	DamageMeter = CreateDefaultSubobject<UDamageMeterComponent>(TEXT("DamageMeter"));

	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;

	PrimaryActorTick.bCanEverTick = true;
}

void ALoAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 클릭이동 게임은 항상 GameAndUI 모드 — 커서 유지, 캡처 시 커서 숨기지 않음
	FInputModeGameAndUI DefaultMode;
	DefaultMode.SetHideCursorDuringCapture(false);
	DefaultMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(DefaultMode);

	HUDViewModel = NewObject<UHUD_ViewModel>(this);
	SkillTreeViewModel = NewObject<USkillTree_ViewModel>(this);

	if (HUDWidgetClass && IsLocalController())
	{
		HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
			BuildBattleItemHUD();
			if (UMVVMView* View = HUDWidget->GetExtension<UMVVMView>())
			{
				bool bOk = View->SetViewModel(FName("HUD_ViewModel"), HUDViewModel);
				UE_LOG(LogTemp, Warning, TEXT("[HUD] SetViewModel: %s"), bOk ? TEXT("성공") : TEXT("실패 - 이름 불일치?"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[HUD] UMVVMView extension 없음 - WBP_HUD에 ViewModel 미등록?"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[HUD] Widget 생성 실패 - HUDWidgetClass 확인"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUD] 위젯 생성 건너뜀 - HUDWidgetClass: %s, IsLocal: %d"),
			HUDWidgetClass ? TEXT("설정됨") : TEXT("미설정"), IsLocalController());
	}

	// 대기 지역 — 소지품이 가득 차 못 받았던 레이드 보상이 있으면 칸이 생길 때마다 지급 (2초마다 확인)
	if (IsLocalController() && !UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass()))
	{
		const ULoAGameInstance* RewardGI = GetGameInstance<ULoAGameInstance>();
		if (RewardGI && RewardGI->PendingRewards.Num() > 0)
		{
			GetWorldTimerManager().SetTimer(PendingRewardTimer, this, &ALoAPlayerController::TryDeliverPendingRewards, 2.f, true, 1.f);
		}
	}

	// OnPossess가 BeginPlay 전에 이미 호출된 경우를 대비해 여기서도 바인딩 시도
	MoveToRepairStationIfReturning(GetPawn());
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		BindCharacterEvents(Char);
		if (Char->SkillManager && SkillTreeViewModel)
		{
			SkillTreeViewModel->Initialize(Char->SkillManager);
		}
	}

	if (IsLocalController())
	{
		CharacterInfoWidget = CreateWidget<UCharacterInfoWidget>(this, UCharacterInfoWidget::StaticClass());
		if (CharacterInfoWidget)
		{
			CharacterInfoWidget->AddToViewport(9);
			CharacterInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		UE_LOG(LogTemp, Log, TEXT("[CharacterInfo] 생성 %s"), CharacterInfoWidget ? TEXT("성공") : TEXT("실패"));

		EngravingSetupWidget = CreateWidget<UEngravingSetupWidget>(this, UEngravingSetupWidget::StaticClass());
		if (EngravingSetupWidget)
		{
			EngravingSetupWidget->AddToViewport(10);
			EngravingSetupWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		UE_LOG(LogTemp, Log, TEXT("[Engraving] 각인 효과 창 생성 %s"), EngravingSetupWidget ? TEXT("성공") : TEXT("실패"));

		GemFuseWidget = CreateWidget<UGemFuseWidget>(this, UGemFuseWidget::StaticClass());
		if (GemFuseWidget)
		{
			GemFuseWidget->AddToViewport(10);
			GemFuseWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		UE_LOG(LogTemp, Log, TEXT("[Gem] 보석 합성 창 생성 %s"), GemFuseWidget ? TEXT("성공") : TEXT("실패"));

		FActorSpawnParameters PreviewParams;
		PreviewParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewParams.ObjectFlags |= RF_Transient;
		CharacterPreviewActor = GetWorld()->SpawnActor<ACharacterPreviewActor>(ACharacterPreviewActor::StaticClass(),
			FVector(0.f, 0.f, ACharacterPreviewActor::PreviewHeight), FRotator::ZeroRotator, PreviewParams);
		if (CharacterInfoWidget)
		{
			CharacterInfoWidget->SetPreviewActor(CharacterPreviewActor);
		}
		UE_LOG(LogTemp, Log, TEXT("[CharacterInfo] 미리보기 액터 %s"), CharacterPreviewActor ? TEXT("생성") : TEXT("생성 실패"));

		DamageMeterWidget = CreateWidget<UDamageMeterWidget>(this, UDamageMeterWidget::StaticClass());
		if (DamageMeterWidget)
		{
			DamageMeterWidget->AddToViewport(9);
			DamageMeterWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (DamageMeter)
		{
			DamageMeter->OnChanged.AddUObject(this, &ALoAPlayerController::HandleDamageMeterChanged);
		}

		InventoryWidget = CreateWidget<UInventoryWidget>(this, UInventoryWidget::StaticClass());
		if (InventoryWidget)
		{
			InventoryWidget->AddToViewport(9);
			InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// SkillTree 위젯: Initialize 후에 SetViewModel → AddToViewport 순서로 생성
	// (AddToViewport가 Event Construct를 트리거하므로 ViewModel이 먼저 준비되어야 함)
	if (SkillTreeWidgetClass && IsLocalController())
	{
		SkillTreeWidget = CreateWidget<UUserWidget>(this, SkillTreeWidgetClass);
		if (SkillTreeWidget)
		{
			if (UMVVMView* View = SkillTreeWidget->GetExtension<UMVVMView>())
			{
				View->SetViewModel(FName("SkillTree_ViewModel"), SkillTreeViewModel);
			}
			SkillTreeWidget->AddToViewport(10);
			SkillTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 보스 HP 바 — 레벨에 배치된 AEchidnaBoss를 찾아서 연결. 보스가 없는 레벨이면 위젯을 아예 만들지 않는다
	if (BossHPWidgetClass && IsLocalController())
	{
		if (AEchidnaBoss* Boss = Cast<AEchidnaBoss>(UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass())))
		{
			BossHPWidget = CreateWidget<UBossHPWidget>(this, BossHPWidgetClass);
			if (BossHPWidget)
			{
				BossHPWidget->AddToViewport();
				TrackedBoss = Boss;
				Boss->OnHPChanged.AddUObject(this, &ALoAPlayerController::OnBossHPChanged);

				// 보스가 이미 BeginPlay를 돈 경우를 위한 초기 1회 갱신.
				// 반대로 보스가 아직이면 보스 BeginPlay의 브로드캐스트가 곧 올바른 값으로 덮어쓴다
				OnBossHPChanged(Boss->GetHP(), Boss->MaxHP);

				UE_LOG(LogTemp, Log, TEXT("[BossHP] 위젯 생성 완료"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[BossHP] CreateWidget 실패 — WBP 컴파일 에러(BindWidget 미해결) 가능성"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[BossHP] 레벨에 AEchidnaBoss가 없어 보스 HP UI를 만들지 않음"));
		}
	}
	else
	{
		// 이 분기가 찍히면 클래스가 아예 안 물린 것 — 로그가 통째로 없어서 원인을 못 가리던 문제를 막는다
		UE_LOG(LogTemp, Warning, TEXT("[BossHP] 생성 건너뜀 — BossHPWidgetClass:%s, IsLocal:%d"),
			BossHPWidgetClass ? TEXT("설정됨") : TEXT("미설정"), IsLocalController());
	}

	// 미니맵 — 모든 맵(아레나가 있으면 타일, 없으면 대기 지역 바닥·구역·NPC). WBP 없이 C++ 클래스로 바로 생성해서 에디터 할당이 필요 없다
	// (위젯 클래스 CDO 할당이 PIE에서 날아가던 문제와 무관). 연기(ZOrder -1)보다 위라 그네·랜잡 중에도 보인다
	if (IsLocalController())
	{
		MinimapWidget = CreateWidget<UMinimapWidget>(this, UMinimapWidget::StaticClass());
		if (MinimapWidget)
		{
			MinimapWidget->AddToViewport(5);
			UE_LOG(LogTemp, Log, TEXT("[Minimap] 위젯 생성 완료"));
		}

		// 좌상단 자원 띠 — 같은 좌상단에 있는 보스 맵 정산 게이지(WBP_BossHP의 SettlementRoot)는 띠 아래로 내린다
		ResourceBarWidget = CreateWidget<UResourceBarWidget>(this, UResourceBarWidget::StaticClass());
		if (ResourceBarWidget)
		{
			ResourceBarWidget->AddToViewport(5);
			if (BossHPWidget)
			{
				if (UWidget* Settlement = BossHPWidget->GetWidgetFromName(TEXT("SettlementRoot")))
				{
					if (UCanvasPanelSlot* SettlementSlot = Cast<UCanvasPanelSlot>(Settlement->Slot))
					{
						FVector2D Pos = SettlementSlot->GetPosition();
						Pos.Y = FMath::Max(Pos.Y, ResourceBarWidget->GetBottomY() + 10.f);
						SettlementSlot->SetPosition(Pos);
					}
				}
			}
		}
	}

	// 캐스팅/차지 진행바 — 만들어두고 숨겨놨다가 Tick이 진행 중일 때만 띄운다
	if (CastBarWidgetClass && IsLocalController())
	{
		CastBarWidget = CreateWidget<UCastBarWidget>(this, CastBarWidgetClass);
		if (CastBarWidget)
		{
			CastBarWidget->AddToViewport();
			CastBarWidget->HideBar();
			UE_LOG(LogTemp, Log, TEXT("[CastBar] 위젯 생성 완료"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CastBar] CreateWidget 실패 — WBP 컴파일 에러(BindWidget 미해결) 가능성"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CastBar] 생성 건너뜀 — CastBarWidgetClass:%s, IsLocal:%d"),
			CastBarWidgetClass ? TEXT("설정됨") : TEXT("미설정"), IsLocalController());
	}

	// 보스 맵 진입 인트로 — HUD 위젯들을 다 만든 뒤라야 숨길 수 있어서 BeginPlay 맨 끝
	if (IsLocalController())
	{
		if (AEchidnaBoss* Boss = Cast<AEchidnaBoss>(UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass())))
		{
			Boss->OnDefeated.AddUObject(this, &ALoAPlayerController::OnBossDefeated);

			// 좌상단 "중단하기" — 인트로보다 먼저 만들어야 인트로가 같이 숨긴다
			AbandonWidget = CreateWidget<UAbandonRaidWidget>(this, UAbandonRaidWidget::StaticClass());
			if (AbandonWidget)
			{
				AbandonWidget->AddToViewport(6);
			}

			if (Boss->bWaitForIntro)
			{
				BeginRaidIntro(Boss);
			}
		}

		// 페이드 아웃하며 넘어왔으면 검은 화면에서 시작해 밝아진다 (인트로가 있으면 인트로가 이미 페이드 인을 건다)
		if (ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>(); GI && GI->bFadeInOnArrive)
		{
			GI->bFadeInOnArrive = false;
			if (!bRaidIntroActive && PlayerCameraManager)
			{
				PlayerCameraManager->StartCameraFade(1.f, 0.f, ArriveFadeInTime, FLinearColor::Black, false, false);
			}
		}
	}
}

void ALoAPlayerController::TravelToLevelWithFade(const TSoftObjectPtr<UWorld>& Level)
{
	if (bLevelTravelPending || Level.IsNull()) return;
	bLevelTravelPending = true;
	PendingTravelLevel = Level;

	if (ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>())
	{
		GI->bFadeInOnArrive = true;
	}

	if (PlayerCameraManager && TravelFadeOutTime > 0.f)
	{
		// 검게 덮은 채로 유지(bHoldWhenFinished) — 레벨이 바뀌면 새 카메라 매니저가 새로 시작한다
		PlayerCameraManager->StartCameraFade(0.f, 1.f, TravelFadeOutTime, FLinearColor::Black, false, true);
		GetWorldTimerManager().SetTimer(TravelFadeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(this, PendingTravelLevel);
		}), TravelFadeOutTime, false);
	}
	else
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, PendingTravelLevel);
	}
}

void ALoAPlayerController::AbandonRaid()
{
	if (bClearActive || bDefeatActive || bRaidIntroActive || bLevelTravelPending) return;

	// 페이드 아웃 동안 맞아 죽거나 움직이지 않게
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		Char->SetInvulnerable(true);
		Char->SetHeldByPattern(true);
	}
	if (AbandonWidget)
	{
		AbandonWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogTemp, Log, TEXT("[Raid] 레이드 중단 — 정비소로 복귀"));
	ReturnToLobby();
}

void ALoAPlayerController::LoAKillBoss()
{
	if (AEchidnaBoss* Boss = Cast<AEchidnaBoss>(UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass())))
	{
		UE_LOG(LogTemp, Log, TEXT("[Cheat] LoAKillBoss — 이번 판은 기록하지 않음"));
		bCheatUsedThisRaid = true;
		Boss->ReceiveDamage(static_cast<float>(Boss->GetHP()) + 1.f);
	}
}

void ALoAPlayerController::LoAKillSelf()
{
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		UE_LOG(LogTemp, Log, TEXT("[Cheat] LoAKillSelf"));
		bCheatUsedThisRaid = true;
		Char->ReceiveDamage(Char->GetMaxHP() * 10.f, ELoADamageKind::Fixed);
	}
}

void ALoAPlayerController::OnPlayerDied()
{
	// 클리어 화면이 이미 떴으면(무적이라 보통은 안 오지만) 패배로 덮지 않는다
	if (bDefeatActive || bClearActive || !IsLocalController()) return;
	bDefeatActive = true;
	bDefeatTravelling = false;
	DefeatElapsed = 0.f;
	DefeatShownSeconds = -1;

	CancelAutoMove();

	if (AbandonWidget)
	{
		AbandonWidget->CloseConfirm();
		AbandonWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	CloseNPCMenu();
	SetInventoryOpen(false);
	SetCharacterInfoOpen(false);
	SetEngravingSetupOpen(false);
	SetGemFuseOpen(false);

	// 스킬창이 열려 있었으면 닫고 입력 모드를 게임으로 되돌린다
	if (SkillTreeWidget && SkillTreeWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		SkillTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
		FInputModeGameAndUI DefaultMode;
		DefaultMode.SetHideCursorDuringCapture(false);
		DefaultMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(DefaultMode);
	}

	DefeatWidget = CreateWidget<UDefeatWidget>(this, UDefeatWidget::StaticClass());
	if (DefeatWidget)
	{
		// 보스 HP·HUD(0~10)·스킬창보다 위
		DefeatWidget->AddToViewport(30);
		DefeatWidget->SetTexts(DeathCauseText);
		DefeatWidget->SetRenderOpacity(0.f);
	}

	UE_LOG(LogTemp, Log, TEXT("[Defeat] 공략 실패 — %.1f초 뒤 정비소로 복귀"), DefeatReturnDelay);
}

void ALoAPlayerController::UpdateDefeat(float DeltaSeconds)
{
	if (!bDefeatActive) return;
	DefeatElapsed += DeltaSeconds;

	// 화면 흑백 — 플레이어 카메라 후처리의 채도를 1 → 0으로. 새 레벨에선 카메라가 새로 만들어지니 되돌릴 필요 없음
	const float GrayAlpha = FMath::Clamp(DefeatElapsed / FMath::Max(0.01f, DefeatGrayFadeTime), 0.f, 1.f);
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		if (UCameraComponent* Camera = Char->GetTopDownCameraComponent())
		{
			const float Saturation = 1.f - GrayAlpha;
			Camera->PostProcessSettings.bOverride_ColorSaturation = true;
			Camera->PostProcessSettings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
			Camera->PostProcessBlendWeight = 1.f;
		}
	}

	if (DefeatWidget)
	{
		DefeatWidget->SetRenderOpacity(FMath::Clamp(DefeatElapsed / 0.5f, 0.f, 1.f));

		const int32 Seconds = FMath::Max(0, FMath::CeilToInt(DefeatReturnDelay - DefeatElapsed));
		if (Seconds != DefeatShownSeconds)
		{
			DefeatShownSeconds = Seconds;
			DefeatWidget->SetCountdown(Seconds);
		}
	}

	if (!bDefeatTravelling && DefeatElapsed >= DefeatReturnDelay)
	{
		bDefeatTravelling = true;
		ReturnToLobby();
	}
}

void ALoAPlayerController::ReturnToLobby()
{
	if (ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>())
	{
		GI->bReturnToRepairStation = true;
	}
	UE_LOG(LogTemp, Log, TEXT("[Raid] %s 로 이동 (정비소 복귀)"), *RaidReturnLevel.ToString());
	TravelToLevelWithFade(RaidReturnLevel);
}

void ALoAPlayerController::OnBossDefeated()
{
	// 이미 죽어서 패배 화면이 떴으면 클리어로 덮지 않는다
	if (bClearActive || bDefeatActive || !IsLocalController()) return;
	bClearActive = true;
	bClearTravelling = false;
	ClearElapsed = 0.f;
	ClearShownSeconds = -1;

	// 보스는 멈췄지만 이미 나간 장판·레이저가 남아 있을 수 있다 — 클리어 후에 죽지 않게 무적
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		Char->SetInvulnerable(true);
	}
	if (AbandonWidget)
	{
		AbandonWidget->CloseConfirm();
		AbandonWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 클리어 시간 — 전투 시작부터 처치까지
	float ClearSeconds = 0.f;
	if (AEchidnaBoss* Boss = Cast<AEchidnaBoss>(UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass())))
	{
		ClearSeconds = Boss->GetClearTime();
	}
	const int32 Total = FMath::Max(0, FMath::FloorToInt(ClearSeconds));
	auto FormatTime = [](int32 Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), Seconds / 3600, (Seconds / 60) % 60, Seconds % 60));
	};

	// 최고 기록 갱신 여부 — GameInstance가 디스크에도 저장
	bool bNewRecord = false;
	float BestSeconds = ClearSeconds;
	ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>();
	if (GI && !bCheatUsedThisRaid)
	{
		bNewRecord = GI->RecordClear(ClearSeconds);
		BestSeconds = GI->BestClearTime;
	}
	const FText ResultLine = bCheatUsedThisRaid
		? FText::Format(NSLOCTEXT("Clear", "ClearTimeCheat", "클리어 시간  {0}    (테스트 명령 사용 — 기록 안 함)"), FormatTime(Total))
		: bNewRecord
		? FText::Format(NSLOCTEXT("Clear", "ClearTimeNew", "클리어 시간  {0}    최고 기록 갱신!"), FormatTime(Total))
		: FText::Format(NSLOCTEXT("Clear", "ClearTime", "클리어 시간  {0}    (최고 기록 {1})"), FormatTime(Total),
			FormatTime(FMath::Max(0, FMath::FloorToInt(BestSeconds))));

	// ── 보상 — 굴리자마자 대기 보상으로 저장(클리어 화면에서 게임을 꺼도 남게) → 소지품에 넣고 다시 저장 ──
	// 첫 클리어 보너스는 테스트 명령을 쓴 판엔 안 준다(최고 기록과 같은 규칙)
	bClearFirstReward = GI && !GI->bFirstClearRewarded && !bCheatUsedThisRaid;
	bClearBonusTaken = false;
	ClearRewards = LoARaidReward::RollEchidna(bClearFirstReward);
	if (GI)
	{
		if (bClearFirstReward) GI->bFirstClearRewarded = true;
		LoARaidReward::Merge(GI->PendingRewards, ClearRewards);
		GI->SaveToDisk();
	}
	TArray<FLoARewardItem> Delivered;
	bClearRewardLeftover = DeliverPendingRewardsNow(Delivered) > 0;

	ClearWidget = CreateWidget<UClearWidget>(this, UClearWidget::StaticClass());
	if (ClearWidget)
	{
		ClearWidget->AddToViewport(30);
		ClearWidget->SetTexts(ClearHeadlineText, ResultLine);
		ClearWidget->SetRenderOpacity(0.f);
		RefreshClearRewardWidget();
	}

	UE_LOG(LogTemp, Log, TEXT("[Clear] 던전 클리어 (%d초) — %.1f초 뒤 정비소로 복귀"), Total, ClearReturnDelay);
}

void ALoAPlayerController::UpdateClear(float DeltaSeconds)
{
	if (!bClearActive) return;
	ClearElapsed += DeltaSeconds;

	if (ClearWidget)
	{
		ClearWidget->SetRenderOpacity(FMath::Clamp(ClearElapsed / 0.5f, 0.f, 1.f));

		const int32 Seconds = FMath::Max(0, FMath::CeilToInt(ClearReturnDelay - ClearElapsed));
		if (Seconds != ClearShownSeconds)
		{
			ClearShownSeconds = Seconds;
			ClearWidget->SetCountdown(Seconds);
		}
	}

	if (!bClearTravelling && ClearElapsed >= ClearReturnDelay)
	{
		bClearTravelling = true;
		ReturnToLobby();
	}
}

int32 ALoAPlayerController::DeliverPendingRewardsNow(TArray<FLoARewardItem>& OutDelivered)
{
	ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>();
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!GI) return 0;
	if (!Char || !Char->Inventory) return GI->PendingRewards.Num();

	const int32 Left = GI->DeliverPendingRewards(Char->Inventory, OutDelivered);
	if (OutDelivered.Num() > 0)
	{
		// 소지품·장비·대기 보상을 한 번에 디스크로 — 소지품에 넣고 대기 보상에서 뺀 상태가 같이 저장돼야 중복·유실이 없다
		if (Char->Equipment) Char->Equipment->SaveNow();
		else GI->SaveToDisk();
	}
	for (const FLoARewardItem& Item : OutDelivered)
	{
		UE_LOG(LogTemp, Log, TEXT("[Reward] 지급 %s x%lld"), *Item.ItemId.ToString(), Item.Count);
	}
	if (Left > 0) UE_LOG(LogTemp, Log, TEXT("[Reward] 소지품이 가득 차 %d줄이 대기 보상으로 남음"), Left);
	return Left;
}

void ALoAPlayerController::TryDeliverPendingRewards()
{
	TArray<FLoARewardItem> Delivered;
	const int32 Left = DeliverPendingRewardsNow(Delivered);
	if (Delivered.Num() > 0)
	{
		ShowTimedNotice(NSLOCTEXT("Reward", "PendingTitle", "보관 중이던 레이드 보상을 받았습니다."),
			Left > 0 ? NSLOCTEXT("Reward", "PendingLeft", "소지품이 가득 차 아직 남은 보상이 있습니다.") : FText::GetEmpty(), 3.f);
	}
	if (Left == 0) GetWorldTimerManager().ClearTimer(PendingRewardTimer);
}

void ALoAPlayerController::RefreshClearRewardWidget()
{
	if (!ClearWidget) return;
	FText Note;
	if (bClearRewardLeftover)
	{
		Note = NSLOCTEXT("Clear", "RewardLeftover", "소지품이 가득 차 일부 보상은 정비소에서 칸이 생기면 지급됩니다.");
	}
	else if (bClearFirstReward)
	{
		Note = FText::Format(NSLOCTEXT("Clear", "RewardFirst", "첫 클리어 보상 — 기본 보상 ×{0}"), FText::AsNumber(LoARaidReward::FirstClearMultiplier));
	}
	ClearWidget->SetRewards(ClearRewards, Note);

	const ALoACharacter* Char = GetPawn<ALoACharacter>();
	const bool bAffordable = Char && Char->Inventory && Char->Inventory->GetGold() >= LoARaidReward::BonusCost;
	ClearWidget->SetBonusState(true, bClearBonusTaken, bAffordable, LoARaidReward::BonusCost);
}

void ALoAPlayerController::RequestClearBonus()
{
	if (!bClearActive || bClearTravelling || bClearBonusTaken) return;
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>();
	if (!Char || !Char->Inventory || !GI) return;

	if (!Char->Inventory->SpendGold(LoARaidReward::BonusCost))
	{
		ShowTimedNotice(NSLOCTEXT("Clear", "BonusTitle", "더보기"), NSLOCTEXT("Clear", "BonusNoGold", "골드가 부족합니다."));
		return;
	}
	bClearBonusTaken = true;
	const TArray<FLoARewardItem> Bonus = LoARaidReward::MakeBonus(ClearRewards);
	LoARaidReward::Merge(GI->PendingRewards, Bonus);
	LoARaidReward::Merge(ClearRewards, Bonus);
	TArray<FLoARewardItem> Delivered;
	bClearRewardLeftover = DeliverPendingRewardsNow(Delivered) > 0;
	if (Delivered.Num() == 0)
	{
		// 전부 대기 보상으로 갔어도 골드 차감은 저장돼야 한다
		if (Char->Equipment) Char->Equipment->SaveNow();
	}
	UE_LOG(LogTemp, Log, TEXT("[Reward] 더보기 — %lld 골드"), LoARaidReward::BonusCost);
	RefreshClearRewardWidget();
}

void ALoAPlayerController::RequestClearLeave()
{
	if (!bClearActive || bClearTravelling) return;
	bClearTravelling = true;
	ReturnToLobby();
}

void ALoAPlayerController::MoveToRepairStationIfReturning(APawn* InPawn)
{
	if (!InPawn) return;

	ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>();
	if (!GI || !GI->bReturnToRepairStation) return;

	// 정비소가 없는 레벨이면 표시를 남겨둔다 (대기 지역에 도착했을 때 처리)
	AActor* Station = UGameplayStatics::GetActorOfClass(this, ARepairStationActor::StaticClass());
	if (!Station) return;

	GI->bReturnToRepairStation = false;

	// 높이는 PlayerStart에 스폰된 그대로 두고 수평 위치만 정비소 중심으로
	FVector Target = Station->GetActorLocation();
	Target.Z = InPawn->GetActorLocation().Z;
	InPawn->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("[Defeat] 정비소로 복귀 — %s"), *Target.ToString());
}

void ALoAPlayerController::BeginRaidIntro(AEchidnaBoss* Boss)
{
	bRaidIntroActive = true;
	bIntroBlendingOut = false;
	RaidIntroElapsed = 0.f;
	IntroBoss = Boss;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	IntroCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SpawnParams);

	// 첫 프레임부터 인트로 카메라 위치로 — 그 뒤 매 틱 UpdateRaidIntro가 움직인다
	UpdateRaidIntro(0.f);

	// 레벨이 막 열린 참이라 검은 화면에서 서서히 밝아지게
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, IntroFadeInTime, FLinearColor::Black, false, false);
	}

	// 연출 중엔 전투 UI를 숨긴다 (원래 Visibility를 기억했다가 되돌림). 캐스팅 바는 원래 숨어 있으니 제외
	IntroHiddenWidgets.Reset();
	IntroHiddenVisibilities.Reset();
	for (UUserWidget* Widget : TArray<UUserWidget*>{ HUDWidget.Get(), BossHPWidget.Get(), MinimapWidget.Get(), ResourceBarWidget.Get(), AbandonWidget.Get() })
	{
		if (!Widget) continue;
		IntroHiddenWidgets.Add(Widget);
		IntroHiddenVisibilities.Add(Widget->GetVisibility());
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	IntroOverlay = CreateWidget<UCinematicOverlayWidget>(this, UCinematicOverlayWidget::StaticClass());
	if (IntroOverlay)
	{
		IntroOverlay->AddToViewport(20);
		IntroOverlay->SetTitle(IntroTitle, IntroSubtitle);
		IntroOverlay->SetBarAmount(0.f);
		IntroOverlay->SetTitleOpacity(0.f);
	}

	UE_LOG(LogTemp, Log, TEXT("[RaidIntro] 시작 — 카메라 %.1f초 + 복귀 %.1f초 뒤 전투 시작"), IntroCameraDuration, IntroBlendOutTime);
}

void ALoAPlayerController::UpdateRaidIntro(float DeltaSeconds)
{
	if (!bRaidIntroActive) return;
	RaidIntroElapsed += DeltaSeconds;

	AEchidnaBoss* Boss = IntroBoss.Get();
	if (!Boss || !IntroCamera)
	{
		EndRaidIntro();
		return;
	}

	// 폰은 BeginPlay보다 늦게 빙의될 수 있어서 매 틱 확인해 묶는다
	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && !Char->IsHeldByPattern())
	{
		Char->SetHeldByPattern(true);
	}

	const float CameraDuration = FMath::Max(0.1f, IntroCameraDuration);

	if (!bIntroBlendingOut)
	{
		const float Alpha = FMath::Clamp(RaidIntroElapsed / CameraDuration, 0.f, 1.f);
		const float Ease = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

		// 보스가 바라보는 방향 기준 오프셋 — 정면에서 얼굴을 보게 된다
		const FVector BossLocation = Boss->GetActorLocation();
		const FQuat BossYaw = FRotator(0.f, Boss->GetActorRotation().Yaw, 0.f).Quaternion();
		const FVector CameraLocation = BossLocation + BossYaw.RotateVector(FMath::Lerp(IntroStartOffset, IntroEndOffset, Ease));
		const FVector LookAt = BossLocation + FVector(0.f, 0.f, IntroLookAtHeight);
		IntroCamera->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());

		// 빙의(OnPossess)가 뷰 타깃을 폰으로 되돌려 놓을 수 있어서 매 틱 다시 잡는다
		if (GetViewTarget() != IntroCamera)
		{
			SetViewTarget(IntroCamera);
		}

		if (Alpha >= 1.f)
		{
			bIntroBlendingOut = true;
			if (APawn* ControlledPawn = GetPawn())
			{
				SetViewTargetWithBlend(ControlledPawn, IntroBlendOutTime, VTBlend_EaseInOut, 2.f);
			}
		}
	}

	if (IntroOverlay)
	{
		// 레터박스: 처음 0.4초 동안 내려오고, 카메라 복귀하는 동안 걷힌다
		const float BarAmount = bIntroBlendingOut
			? 1.f - (RaidIntroElapsed - CameraDuration) / FMath::Max(0.01f, IntroBlendOutTime)
			: RaidIntroElapsed / 0.4f;
		IntroOverlay->SetBarAmount(BarAmount);

		// 보스 이름: 0.5초부터 서서히 떠서 카메라 이동이 끝나기 0.5초 전부터 사라진다
		const float TitleIn = FMath::Clamp((RaidIntroElapsed - 0.5f) / 0.6f, 0.f, 1.f);
		const float TitleOut = FMath::Clamp((CameraDuration - RaidIntroElapsed) / 0.5f, 0.f, 1.f);
		IntroOverlay->SetTitleOpacity(bIntroBlendingOut ? 0.f : TitleIn * TitleOut);
	}

	if (bIntroBlendingOut && RaidIntroElapsed >= CameraDuration + IntroBlendOutTime)
	{
		EndRaidIntro();
	}
}

void ALoAPlayerController::EndRaidIntro()
{
	if (!bRaidIntroActive) return;
	bRaidIntroActive = false;

	APawn* ControlledPawn = GetPawn();
	// 중간에 끊긴 경우(보스 소실 등)엔 블렌드 없이 바로 플레이어 카메라로
	if (!bIntroBlendingOut && ControlledPawn)
	{
		SetViewTarget(ControlledPawn);
	}

	if (ALoACharacter* Char = Cast<ALoACharacter>(ControlledPawn))
	{
		Char->SetHeldByPattern(false);
	}

	for (int32 i = 0; i < IntroHiddenWidgets.Num(); ++i)
	{
		if (IntroHiddenWidgets[i])
		{
			IntroHiddenWidgets[i]->SetVisibility(IntroHiddenVisibilities[i]);
		}
	}
	IntroHiddenWidgets.Reset();
	IntroHiddenVisibilities.Reset();

	if (IntroOverlay)
	{
		IntroOverlay->RemoveFromParent();
		IntroOverlay = nullptr;
	}

	// 블렌드 마지막 프레임에 아직 카메라 매니저가 참조할 수 있어서 바로 지우지 않고 잠깐 뒤에 소멸
	if (IntroCamera)
	{
		IntroCamera->SetLifeSpan(0.5f);
		IntroCamera = nullptr;
	}

	if (AEchidnaBoss* Boss = IntroBoss.Get())
	{
		Boss->StartCombat();
	}
	IntroBoss = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[RaidIntro] 종료 — 전투 시작"));
}

void ALoAPlayerController::OnBossHPChanged(double NewHP, double NewMaxHP)
{
	if (!BossHPWidget || !TrackedBoss.IsValid()) return;

	BossHPWidget->SetBossHP(NewHP, NewMaxHP, TrackedBoss->TotalLines);
}

void ALoAPlayerController::SetScreenFog(bool bEnable, float FadeTime)
{
	SetScreenFogSource(TEXT("Pattern"), bEnable, FadeTime);
}

void ALoAPlayerController::SetScreenFogSource(FName Source, bool bEnable, float FadeTime)
{
	if (bEnable)
	{
		ScreenFogSources.Add(Source);
	}
	else
	{
		ScreenFogSources.Remove(Source);
	}
	bEnable = ScreenFogSources.Num() > 0;

	if (bEnable && !ScreenFogWidget && IsLocalController())
	{
		ScreenFogWidget = CreateWidget<UScreenFogWidget>(this, UScreenFogWidget::StaticClass());
		if (ScreenFogWidget)
		{
			// HUD(ZOrder 0)보다 아래 — 연기 속에서도 보스 HP·스킬 슬롯은 보여야 한다
			ScreenFogWidget->AddToViewport(-1);
			ScreenFogWidget->SetFogOpacity(0.f);
			FogCurrentOpacity = 0.f;
		}
	}

	FogTargetOpacity = bEnable ? 1.f : 0.f;
	FogFadeSpeed = FadeTime > 0.f ? 1.f / FadeTime : 1000.f;
}

void ALoAPlayerController::ShowBossDialogue(const FText& Speaker, const FText& Line, UTexture2D* Portrait)
{
	if (!IsLocalController()) return;

	if (!BossDialogueWidget)
	{
		BossDialogueWidget = CreateWidget<UBossDialogueWidget>(this, UBossDialogueWidget::StaticClass());
		if (!BossDialogueWidget) return;
	}
	// HUD(0)·미니맵(5)보다 위, 알림 띠(8)보다 아래
	if (!BossDialogueWidget->IsInViewport())
	{
		BossDialogueWidget->AddToViewport(7);
	}
	BossDialogueWidget->SetDialogue(Speaker, Line);
	BossDialogueWidget->SetPortrait(Portrait);
	BossDialogueWidget->FadeIn();
}

void ALoAPlayerController::HideBossDialogue()
{
	if (BossDialogueWidget)
	{
		BossDialogueWidget->FadeOut();
	}
}

void ALoAPlayerController::ShowTimedNotice(const FText& Title, const FText& Message, float Duration)
{
	if (!IsLocalController()) return;

	if (!TimedNoticeWidget)
	{
		TimedNoticeWidget = CreateWidget<UZoneNoticeWidget>(this, UZoneNoticeWidget::StaticClass());
		if (!TimedNoticeWidget) return;
	}
	// 스킬트리 창(10)이 열려 있을 때 뜨므로 그보다 위 — 트리는 AddToViewport에서 만들어지니 글자는 그 뒤에 넣는다
	if (!TimedNoticeWidget->IsInViewport())
	{
		TimedNoticeWidget->AddToViewport(11);
	}
	TimedNoticeWidget->SetNotice(Title, Message);
	TimedNoticeWidget->SetCounter(FText::GetEmpty());

	GetWorldTimerManager().SetTimer(TimedNoticeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (TimedNoticeWidget) TimedNoticeWidget->RemoveFromParent();
	}), Duration, false);
}

void ALoAPlayerController::UpdateScreenFog(float DeltaSeconds)
{
	if (!ScreenFogWidget) return;
	if (FMath::IsNearlyEqual(FogCurrentOpacity, FogTargetOpacity)) return;

	FogCurrentOpacity = FMath::FInterpConstantTo(FogCurrentOpacity, FogTargetOpacity, DeltaSeconds, FogFadeSpeed);
	ScreenFogWidget->SetFogOpacity(FogCurrentOpacity);
	ScreenFogWidget->SetVisibility(FogCurrentOpacity > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void ALoAPlayerController::UpdateEnrageTimer()
{
	if (!BossHPWidget || !TrackedBoss.IsValid()) return;

	BossHPWidget->SetEnrageTime(TrackedBoss->GetEnrageRemainingTime(), TrackedBoss->IsEnraged());
	BossHPWidget->SetSettlementGauge(TrackedBoss->SettlementGauge);
	BossHPWidget->SetSettlementPaused(TrackedBoss->IsSettlementPaused());
	// 처치된 뒤엔 남은 그로기가 있어도 숨김
	const bool bShowGroggy = !TrackedBoss->IsDefeated();
	BossHPWidget->SetGroggy(TrackedBoss->GetGroggyRemainingTime(), bShowGroggy ? TrackedBoss->GetGroggyTotalDuration() : 0.f);
}

void ALoAPlayerController::UpdateCastBar()
{
	USkillManagerComponent* SM = GetSkillManager();
	float Elapsed = 0.f;
	float Total = 0.f;
	const bool bCasting = SM && SM->GetActiveCastProgress(Elapsed, Total);

	if (!CastBarWidget) return;

	if (bCasting)
	{
		CastBarWidget->SetProgress(Elapsed, Total);
	}
	else
	{
		CastBarWidget->HideBar();
	}
}

void ALoAPlayerController::ForceMoveTo(const FVector& Destination)
{
	CachedDestination = Destination;
	bAutoMoving = true;
	bHoldMoving = false;
}

bool ALoAPlayerController::GetCursorAimLocation(FVector& OutLocation, bool* bOutHitGround) const
{
	if (bOutHitGround) *bOutHitGround = false;
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		if (bOutHitGround) *bOutHitGround = true;
		OutLocation = Hit.Location;
		return true;
	}

	const APawn* MyPawn = GetPawn();
	FVector RayOrigin, RayDir;
	if (!MyPawn || !DeprojectMousePositionToWorld(RayOrigin, RayDir)) return false;

	// 발밑 높이 평면 — 캡슐 중심에서 절반 높이만큼 아래
	const ACharacter* MyChar = Cast<ACharacter>(MyPawn);
	const float FeetZ = MyPawn->GetActorLocation().Z - (MyChar ? MyChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f);
	if (RayDir.Z < -KINDA_SMALL_NUMBER)
	{
		const float T = (FeetZ - RayOrigin.Z) / RayDir.Z;
		if (T > 0.f)
		{
			OutLocation = RayOrigin + RayDir * T;
			return true;
		}
	}

	// 수평선 위를 가리킴 — 커서 방향의 수평 성분으로
	const FVector Horiz = FVector(RayDir.X, RayDir.Y, 0.f).GetSafeNormal();
	if (Horiz.IsNearlyZero()) return false;
	OutLocation = MyPawn->GetActorLocation() + Horiz * 1000.f;
	return true;
}

bool ALoAPlayerController::GetCursorGroundTarget(const AActor* Caster, float MaxRange, FVector& OutLocation) const
{
	bool bHitGround = false;
	if (!Caster || !GetCursorAimLocation(OutLocation, &bHitGround)) return false;
	if (bHitGround) return true;

	const FVector CasterLoc = Caster->GetActorLocation();
	FVector Horizontal = OutLocation - CasterLoc;
	Horizontal.Z = 0.f;
	if (Horizontal.Size() > MaxRange)
	{
		const float Z = OutLocation.Z;
		OutLocation = CasterLoc + Horizontal.GetSafeNormal() * MaxRange;
		OutLocation.Z = Z;
	}
	return true;
}

void ALoAPlayerController::CancelAutoMove()
{
	bAutoMoving  = false;
	bHoldMoving  = false;
	if (APawn* P = GetPawn())
	{
		if (UCharacterMovementComponent* CMC =
			Cast<UCharacterMovementComponent>(P->GetMovementComponent()))
		{
			CMC->Velocity = FVector(0.f, 0.f, CMC->Velocity.Z);
		}
	}
}

void ALoAPlayerController::StopMovement()
{
	bWasAutoMovingBeforeDash = bAutoMoving;
	bAutoMoving = false;
	bHoldMoving = false;
	bDashSuppressed = true;
	DashSuppressFrames = 2;
	Super::StopMovement();
}

void ALoAPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Controller Tick이 CMC Tick보다 먼저 실행되어야 RequestDirectMove가 같은 프레임에 적용됨
	if (UCharacterMovementComponent* CMC = InPawn ? InPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr)
	{
		CMC->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}

	MoveToRepairStationIfReturning(InPawn);

	if (ALoACharacter* Char = Cast<ALoACharacter>(InPawn))
	{
		if (HUDViewModel)
			BindCharacterEvents(Char);

		if (SkillTreeViewModel && Char->SkillManager)
			SkillTreeViewModel->Initialize(Char->SkillManager);
	}
}

void ALoAPlayerController::BindCharacterEvents(ALoACharacter* InCharacter)
{
	InCharacter->OnHPChanged.RemoveAll(this);
	InCharacter->OnMPChanged.RemoveAll(this);
	InCharacter->OnCharmedChanged.RemoveAll(this);
	InCharacter->OnDied.RemoveAll(this);

	HUDViewModel->SetMaxHP(InCharacter->GetMaxHP());
	HUDViewModel->SetHP(InCharacter->GetHP());
	HUDViewModel->SetMaxMP(InCharacter->GetMaxMP());
	HUDViewModel->SetMP(InCharacter->GetMP());

	InCharacter->OnHPChanged.AddUObject(this, &ALoAPlayerController::OnPlayerHPChanged);
	InCharacter->OnMPChanged.AddUObject(this, &ALoAPlayerController::OnPlayerMPChanged);
	InCharacter->OnCharmedChanged.AddUObject(this, &ALoAPlayerController::OnPlayerCharmedChanged);
	InCharacter->OnDied.AddUObject(this, &ALoAPlayerController::OnPlayerDied);
}

void ALoAPlayerController::OnSkillTreeToggle()
{
	if (!SkillTreeWidget) return;

	// 보스 맵 진입 연출 중·사망 후·상점 중엔 스킬창도 못 연다
	if (bRaidIntroActive || bDefeatActive || bNPCMenuOpen) return;

	// 0.3초 내 중복 호출 무시 (SetWidgetToFocus 시 Enhanced Input 재평가로 인한 이중 발동 방지)
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastSkillTreeToggleTime < 0.3f) return;
	LastSkillTreeToggleTime = Now;

	const bool bOpen = SkillTreeWidget->GetVisibility() == ESlateVisibility::Collapsed;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	if (bOpen)
	{
		if (SkillTreeViewModel) SkillTreeViewModel->Refresh();
		SkillTreeWidget->SetVisibility(ESlateVisibility::Visible);
		InputMode.SetWidgetToFocus(SkillTreeWidget->TakeWidget());
	}
	else
	{
		SkillTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
		// 위젯 포커스 없이 — 게임이 마우스 입력 정상 수신
	}

	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ALoAPlayerController::OnPlayerHPChanged(float NewHP)
{
	if (HUDViewModel)
	{
		// 최대 HP는 장비·스탯으로 바뀐다 — HP 알림마다 같이 넘긴다(예전엔 바인딩할 때 한 번만 넣어 "100151 / 23423"이 됐음)
		if (const ALoACharacter* Char = GetPawn<ALoACharacter>())
		{
			HUDViewModel->SetMaxHP(Char->GetMaxHP());
		}
		HUDViewModel->SetHP(NewHP);
	}
}

void ALoAPlayerController::OnPlayerMPChanged(float NewMP)
{
	if (HUDViewModel)
	{
		HUDViewModel->SetMP(NewMP);
	}
}

void ALoAPlayerController::OnPlayerCharmedChanged(bool bCharmed)
{
	if (bCharmed)
	{
		// 매혹 시작 — 하던 이동/캐스팅을 즉시 정지하고, 그 이후로는 진짜 입력 대신 무작위 행동이 대신 실행됨
		// (OnInputStarted 등 각 입력 핸들러에서 Char->IsCharmed() 체크로 실제 입력은 씹힘)
		bAutoMoving = false;
		bHoldMoving = false;

		if (APawn* ControlledPawn = GetPawn())
		{
			if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
			{
				CMC->Velocity = FVector::ZeroVector;
				CMC->ClearAccumulatedForces();
			}
		}

		if (USkillManagerComponent* SM = GetSkillManager())
		{
			SM->CancelActiveCastSkill();
			SM->CancelPendingRangeMove();
		}

		PerformRandomCharmAction();
		GetWorldTimerManager().SetTimer(CharmConfusionTimerHandle, this, &ALoAPlayerController::PerformRandomCharmAction, CharmActionInterval, true);

		// 매혹 중엔 화면에 핑크 연기 — 패턴 연기와 출처를 나눠서 서로 끄지 않게
		SetScreenFogSource(TEXT("Charm"), true, 0.5f);
	}
	else
	{
		SetScreenFogSource(TEXT("Charm"), false, 0.5f);
		GetWorldTimerManager().ClearTimer(CharmConfusionTimerHandle);
		GetWorldTimerManager().ClearTimer(CharmSkillReleaseTimerHandle);

		if (CharmActiveSkillSlot >= 0)
		{
			if (USkillManagerComponent* SM = GetSkillManager())
			{
				SM->HandleKeyUp(CharmActiveSkillSlot);
			}
			CharmActiveSkillSlot = -1;
		}

		bAutoMoving = false;
	}
}

void ALoAPlayerController::PerformRandomCharmAction()
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->IsCharmed()) return;

	// 스킬을 붙잡고 있는 동안에는 이동 목표를 건드리지 않는다.
	// 여기서 매 틱 목적지를 덮어쓰면 사거리 밖 Cast 스킬의 ForceMoveTo와 싸워서 영원히 사거리에 못 들어간다.
	if (CharmActiveSkillSlot >= 0) return;

	// 무작위 이동 — 실제 클릭 이동과 동일한 방식(bAutoMoving+CachedDestination)을 그대로 재사용
	const float RandAngle = FMath::FRandRange(0.f, 2.f * PI);
	const float RandRadius = FMath::FRandRange(CharmWanderRadius * 0.3f, CharmWanderRadius);
	CachedDestination = Char->GetActorLocation() + FVector(FMath::Cos(RandAngle), FMath::Sin(RandAngle), 0.f) * RandRadius;
	bAutoMoving = true;
	bHoldMoving = false;

	USkillManagerComponent* SM = GetSkillManager();
	if (!SM) return;

	// 쿨타임이 돌아서 실제로 나갈 수 있는 슬롯만 모은 뒤 그 중에서 고른다.
	// 0~7에서 무작정 뽑으면 쿨타임이 10~30초인 현재 구성에서는 대부분 불발되어 매혹이 무해해진다.
	TArray<int32, TInlineAllocator<8>> UsableSlots;
	for (int32 Slot = 0; Slot <= 7; ++Slot)
	{
		if (SM->IsSlotAssigned(Slot) && !SM->IsSlotOnCooldown(Slot))
		{
			UsableSlots.Add(Slot);
		}
	}
	if (UsableSlots.Num() == 0) return;

	CharmActiveSkillSlot = UsableSlots[FMath::RandRange(0, UsableSlots.Num() - 1)];
	SM->HandleKeyDown(CharmActiveSkillSlot);

	// 타입별로 실제 발동에 필요한 만큼 붙잡는다 — 짧게 떼면 캐스팅이 취소만 되고 쿨타임만 날아간다
	const FSkillData SkillData = SM->GetSlotSkillData(CharmActiveSkillSlot);
	float HoldTime = 0.1f;
	switch (SkillData.InputType)
	{
	case ESkillInputType::Cast:   HoldTime = SkillData.CastTime + 0.3f; break;
	case ESkillInputType::Charge: HoldTime = SkillData.ChargeMaxTime + 0.3f; break;
	case ESkillInputType::Hold:   HoldTime = FMath::FRandRange(SkillData.HoldMaxTime * 0.5f, SkillData.HoldMaxTime); break;
	default: break;
	}
	GetWorldTimerManager().SetTimer(CharmSkillReleaseTimerHandle, this, &ALoAPlayerController::ReleaseCharmSkill, HoldTime, false);
}

void ALoAPlayerController::ReleaseCharmSkill()
{
	if (CharmActiveSkillSlot < 0) return;

	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyUp(CharmActiveSkillSlot);
	}
	CharmActiveSkillSlot = -1;
}

void ALoAPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Only set up input on local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);

			if (SkillMappingContext)
			{
				Subsystem->AddMappingContext(SkillMappingContext, 1);
			}
		}

		// Set up action bindings
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// Setup mouse input events
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &ALoAPlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &ALoAPlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &ALoAPlayerController::OnSetDestinationReleased);

			// Setup touch input events
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &ALoAPlayerController::OnTouchTriggered);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &ALoAPlayerController::OnTouchReleased);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &ALoAPlayerController::OnTouchReleased);

			// 스킬 슬롯 바인딩 (루프로 8개 한꺼번에)
			for (int32 i = 0; i < SkillSlotActions.Num(); i++)
			{
				if (!SkillSlotActions[i]) continue;
				EnhancedInputComponent->BindAction(SkillSlotActions[i], ETriggerEvent::Started,   this, &ALoAPlayerController::OnSkillKeyDown, i);
				EnhancedInputComponent->BindAction(SkillSlotActions[i], ETriggerEvent::Triggered, this, &ALoAPlayerController::OnSkillKeyHeld, i);
				EnhancedInputComponent->BindAction(SkillSlotActions[i], ETriggerEvent::Completed, this, &ALoAPlayerController::OnSkillKeyUp,   i);
			}

			// 스킬트리 토글 (K키)
			if (SkillTreeAction)
			{
				EnhancedInputComponent->BindAction(SkillTreeAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnSkillTreeToggle);
			}

			// 대쉬 바인딩
			if (DashAction)
			{
				EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnDashInput);
			}

			// 기본공격 바인딩 (슬롯 인덱스 8)
			if (BasicAttackAction)
			{
				EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started,   this, &ALoAPlayerController::OnSkillKeyDown, USkillManagerComponent::BasicAttackSlotIndex);
				EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Triggered, this, &ALoAPlayerController::OnSkillKeyHeld, USkillManagerComponent::BasicAttackSlotIndex);
				EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Completed, this, &ALoAPlayerController::OnSkillKeyUp,   USkillManagerComponent::BasicAttackSlotIndex);
			}

			// 배틀아이템 1~4 — 입력 액션 에셋 없이 키를 직접 바인딩
			const FKey BattleItemKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
			for (int32 i = 0; i < UE_ARRAY_COUNT(BattleItemKeys); ++i)
			{
				FInputKeyBinding Binding(FInputChord(BattleItemKeys[i]), IE_Pressed);
				Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALoAPlayerController::OnBattleItemKey, i);
				EnhancedInputComponent->KeyBindings.Emplace(MoveTemp(Binding));
			}

			// 소지품 (I)
			{
				FInputKeyBinding Binding(FInputChord(EKeys::I), IE_Pressed);
				Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALoAPlayerController::OnInventoryToggle);
				EnhancedInputComponent->KeyBindings.Emplace(MoveTemp(Binding));
			}

			// 데미지 미터 (L)
			{
				FInputKeyBinding Binding(FInputChord(EKeys::L), IE_Pressed);
				Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALoAPlayerController::OnDamageMeterToggle);
				EnhancedInputComponent->KeyBindings.Emplace(MoveTemp(Binding));
			}

			// 캐릭터 정보 (P)
			{
				FInputKeyBinding Binding(FInputChord(EKeys::P), IE_Pressed);
				Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALoAPlayerController::OnCharacterInfoToggle);
				EnhancedInputComponent->KeyBindings.Emplace(MoveTemp(Binding));
			}

			// NPC 대화 (G)
			{
				FInputKeyBinding Binding(FInputChord(EKeys::G), IE_Pressed);
				Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALoAPlayerController::OnInteractKey);
				EnhancedInputComponent->KeyBindings.Emplace(MoveTemp(Binding));
			}

		}
		else
		{
			UE_LOG(LogLoA, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
		}
	}
}

void ALoAPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// HUD 슬롯 드래그 — 조작 불가 상태여도 놓는 처리는 돼야 하므로 맨 앞
	UpdateSlotDrag();

	// 폰 체크보다 앞 — 인트로는 폰이 빙의되기 전부터 카메라를 움직여야 한다
	UpdateRaidIntro(DeltaSeconds);
	UpdateDefeat(DeltaSeconds);
	UpdateClear(DeltaSeconds);

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent());
	if (!CMC) return;

	// 행동불능 체크보다 먼저 — 캐스팅 도중 기절/넉다운으로 스킬이 끊겼을 때도 바가 남지 않고 사라져야 한다
	UpdateCastBar();
	UpdateBattleItemHUD();
	UpdateNPCInteraction();
	if (InventoryWidget && IsInventoryOpen())
	{
		const ALoACharacter* InvChar = Cast<ALoACharacter>(ControlledPawn);
		InventoryWidget->SetInventory(InvChar ? InvChar->Inventory.Get() : nullptr);
	}
	if (CharacterInfoWidget && IsCharacterInfoOpen())
	{
		const ALoACharacter* InfoChar = Cast<ALoACharacter>(ControlledPawn);
		CharacterInfoWidget->SetStatComponent(InfoChar ? InfoChar->StatComponent.Get() : nullptr);
		CharacterInfoWidget->SetBuildSources(InfoChar ? InfoChar->Gems.Get() : nullptr, InfoChar ? InfoChar->Engravings.Get() : nullptr,
			InfoChar ? InfoChar->Inventory.Get() : nullptr, InfoChar ? InfoChar->SkillManager.Get() : nullptr);
	}
	UpdateEnrageTimer();
	UpdateScreenFog(DeltaSeconds);

	if (ALoACharacter* Char = Cast<ALoACharacter>(ControlledPawn); Char && Char->IsActionLocked())
	{
		return;
	}

	// 매혹 중에는 실제 키 입력이 전부 차단되므로, 붙잡고 있는 슬롯의 Held를 여기서 대신 흘려준다.
	// 사거리 밖 Cast의 진입 판정과 Hold 스킬의 지속 누적이 이 호출에 의존한다.
	if (CharmActiveSkillSlot >= 0)
	{
		if (USkillManagerComponent* SM = GetSkillManager())
		{
			SM->HandleKeyHeld(CharmActiveSkillSlot, DeltaSeconds);
		}
	}

	if (bDashSuppressed)
	{
		if (DashSuppressFrames > 0)
		{
			--DashSuppressFrames;
			return;
		}
		if (CMC->Velocity.Size2D() <= CMC->MaxWalkSpeed + 1.0f)
		{
			bDashSuppressed = false;
			if (bWasAutoMovingBeforeDash && !bHoldMoving)
			{
				bAutoMoving = true;
				bWasAutoMovingBeforeDash = false;
			}
		}
		else
		{
			return;
		}
	}

	if (bAutoMoving)
	{
		FVector ToDestination = CachedDestination - ControlledPawn->GetActorLocation();
		ToDestination.Z = 0.f;

		const float FrameMoveDist = CMC->MaxWalkSpeed * DeltaSeconds + 1.0f;

		if (ToDestination.SizeSquared() <= FMath::Square(FrameMoveDist))
		{
			bAutoMoving = false;
			FVector SnapLocation(CachedDestination.X, CachedDestination.Y, ControlledPawn->GetActorLocation().Z);
			ControlledPawn->SetActorLocation(SnapLocation, false, nullptr, ETeleportType::TeleportPhysics);
			CMC->Velocity = FVector::ZeroVector;
			CMC->ClearAccumulatedForces();
		}
		else
		{
			const FVector Dir = ToDestination.GetSafeNormal();
			const float Speed = CMC->MaxWalkSpeed;
			CMC->Velocity = FVector(Dir.X * Speed, Dir.Y * Speed, CMC->Velocity.Z);
			CMC->RequestDirectMove(Dir * Speed, true);
			ControlledPawn->AddMovementInput(Dir, 1.0f, true);
		}
	}
	else if (bHoldMoving)
	{
		FVector ToDestination = CachedDestination - ControlledPawn->GetActorLocation();
		ToDestination.Z = 0.f;

		if (!ToDestination.IsNearlyZero())
		{
			const FVector Dir = ToDestination.GetSafeNormal();
			const float Speed = CMC->MaxWalkSpeed;
			CMC->Velocity = FVector(Dir.X * Speed, Dir.Y * Speed, CMC->Velocity.Z);
			CMC->RequestDirectMove(Dir * Speed, true);
			ControlledPawn->AddMovementInput(Dir, 1.0f, true);
		}
		else
		{
			CMC->Velocity = FVector(0.f, 0.f, CMC->Velocity.Z);
		}
	}
}

void ALoAPlayerController::OnInputStarted()
{
	// 거리 안의 NPC를 우클릭(= 이동 버튼) — 걷지 않고 대화(상점)
	if (!bNPCMenuOpen)
	{
		AInteractableNPCActor* NPC = HoveredNPC.Get();
		if (NPC && NPC == InRangeNPC.Get())
		{
			OpenNPCMenu(NPC);
			if (bNPCMenuOpen)
			{
				bMovePressSuppressed = true;
				return;
			}
		}
	}

	// HUD 슬롯·소지품 창 위 클릭은 이동으로 쓰지 않는다 (드래그 스왑 시작) — 누르고 있는 동안·뗄 때도
	bMovePressSuppressed = ConsumeHUDSlotPress();
	if (bMovePressSuppressed)
	{
		return;
	}

	// 매혹 중엔 IsActionLocked()에 안 걸려도(진짜 조종불능이 아니라 "무작위 대신 행동"이라 Tick의 이동 처리는
	// 그대로 둬야 함) 실제 플레이어 클릭 입력만은 씹혀야 함 — 그래서 여기 입력 핸들러들에서만 별도 체크
	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && (Char->IsActionLocked() || Char->IsCharmed()))
	{
		return;
	}

	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->CancelActiveCastSkill();
		SM->CancelPendingRangeMove();  // 자동이동 대기 스킬도 취소 (쿨타임 없음)
	}

	bAutoMoving = false;
	bHoldMoving = false;
	bDashSuppressed = false;
	bWasAutoMovingBeforeDash = false;
	bMoveHaltedByAttack = false;  // 새 이동 클릭 — 기본공격·스킬로 멈췄던 것 해제

	if (APawn* ControlledPawn = GetPawn())
	{
		if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
		{
			CMC->Velocity = FVector::ZeroVector;
			CMC->ClearAccumulatedForces();
		}
	}

	UpdateCachedDestination();
}

void ALoAPlayerController::OnSetDestinationTriggered()
{
	if (bSlotPressActive || bMovePressSuppressed)
	{
		return;
	}

	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && (Char->IsActionLocked() || Char->IsCharmed()))
	{
		return;
	}

	// 기본공격으로 멈췄으면 버튼을 계속 누르고 있어도 새 클릭 전까진 걷지 않는다
	if (bMoveHaltedByAttack) return;

	bAutoMoving = false;
	bHoldMoving = true;

	FollowTime += GetWorld()->GetDeltaSeconds();

	// 커서 위치 업데이트만 - 실제 이동은 Tick에서 처리
	UpdateCachedDestination();
}

void ALoAPlayerController::OnSetDestinationReleased()
{
	bHoldMoving = false;

	// UI 위에서 시작된 클릭 — 짧게 눌렀다 떼도 자동이동을 시작하지 않는다
	if (bMovePressSuppressed)
	{
		bMovePressSuppressed = false;
		FollowTime = 0.f;
		return;
	}

	// 짧게 눌렀다 떼도 기본공격으로 멈춘 뒤라면 자동이동을 시작하지 않는다
	if (bMoveHaltedByAttack)
	{
		FollowTime = 0.f;
		return;
	}

	if (FollowTime <= ShortPressThreshold)
	{
		bAutoMoving = true;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}
	else
	{
		// 꾹 누르다 뗐을 때 즉시 정지
		if (APawn* P = GetPawn())
		{
			if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(P->GetMovementComponent()))
			{
				CMC->Velocity = FVector::ZeroVector;
				CMC->ClearAccumulatedForces();
			}
		}
	}

	FollowTime = 0.f;
}

// Triggered every frame when the input is held down
void ALoAPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void ALoAPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void ALoAPlayerController::UpdateCachedDestination()
{
	if (bIsTouch)
	{
		FHitResult Hit;
		if (GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit))
		{
			CachedDestination = Hit.Location;
		}
		return;
	}

	// 커서 레이를 캐릭터 높이의 수평면에 교차 - 캡슐/메시 충돌 완전 무시
	float MouseX, MouseY;
	if (!GetMousePosition(MouseX, MouseY)) return;

	FVector RayOrigin, RayDir;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, RayOrigin, RayDir)) return;

	if (FMath::Abs(RayDir.Z) > KINDA_SMALL_NUMBER)
	{
		const float GroundZ = GetPawn() ? GetPawn()->GetActorLocation().Z : 0.f;
		const float T = (GroundZ - RayOrigin.Z) / RayDir.Z;
		if (T > 0.f)
		{
			CachedDestination = RayOrigin + RayDir * T;
		}
	}
}

void ALoAPlayerController::OnDashInput()
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char) return;

	// 넉다운 중엔 스페이스바가 대시가 아니라 즉시 기상 — InstantGetUpCooldown이 다 찼을 때만 성공
	if (Char->IsKnockedDown())
	{
		Char->TryInstantGetUp();
		return;
	}

	// 경직·매혹 중엔 대시도 막힘 (즉시 기상 같은 대체 동작 없이 그냥 입력 무시)
	// 경직·기절·끌려감·붙잡힘·사망 중엔 대시도 막힘 (넉다운은 위에서 즉시 기상으로 처리), 매혹 중엔 플레이어가 조종 못 함
	if (Char->IsActionLocked() || Char->IsCharmed()) return;

	if (!Char->SkillManager) return;
	if (Char->SkillManager->IsSlotOnCooldown(USkillManagerComponent::DashSlotIndex)) return;

	FHitResult HitResult;
	FVector TargetLocation;
	if (GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		TargetLocation = HitResult.Location;
	}
	else
	{
		TargetLocation = Char->GetActorLocation() + Char->GetActorForwardVector() * 1000.f;
	}

	Char->ExecuteDash(TargetLocation);
	Char->SkillManager->TriggerCooldown(USkillManagerComponent::DashSlotIndex);
	StopMovement();
}

void ALoAPlayerController::OnSkillKeyDown(int32 SlotIndex)
{
	// 기본공격(마우스)이 HUD 슬롯 위에서 눌렸으면 공격하지 않는다 — 슬롯 클릭·드래그 스왑으로 처리
	if (SlotIndex == USkillManagerComponent::BasicAttackSlotIndex)
	{
		bBasicAttackSuppressed = ConsumeHUDSlotPress();
		if (bBasicAttackSuppressed)
		{
			return;
		}
	}

	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && (Char->IsActionLocked() || Char->IsCharmed()))
	{
		return;
	}

	// 스킬·기본공격: 누르는 즉시 멈추고, 다음 이동 클릭 전까지 그 자리에 서 있는다.
	// HandleKeyDown보다 먼저 해야 사거리 밖 스킬의 ForceMoveTo(사거리 자동이동)가 덮어쓰이지 않는다.
	// 기본공격은 큐·쿨타임이라 바로 안 나가도 멈추지만, 스킬은 실제로 나갈 수 있을 때만 멈춘다 —
	// 쿨타임 중이거나 빈 슬롯을 눌렀다고 걷던 캐릭터가 서버리면 안 되므로
	USkillManagerComponent* SkillManager = GetSkillManager();
	const bool bBasicAttack = SlotIndex == USkillManagerComponent::BasicAttackSlotIndex;
	const bool bUsableSkill = SkillManager && SkillManager->IsSlotAssigned(SlotIndex) && !SkillManager->IsSlotOnCooldown(SlotIndex);
	if (bBasicAttack || bUsableSkill)
	{
		CancelAutoMove();
		bWasAutoMovingBeforeDash = false;  // 대시 직후라도 대시 끝나고 이동이 되살아나지 않게
		bMoveHaltedByAttack = true;
	}

	if (SkillManager)
	{
		SkillManager->HandleKeyDown(SlotIndex);
	}
}

void ALoAPlayerController::OnSkillKeyHeld(int32 SlotIndex)
{
	if (SlotIndex == USkillManagerComponent::BasicAttackSlotIndex && bBasicAttackSuppressed)
	{
		return;
	}

	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && (Char->IsActionLocked() || Char->IsCharmed()))
	{
		return;
	}

	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyHeld(SlotIndex, GetWorld()->GetDeltaSeconds());
	}
}

void ALoAPlayerController::OnSkillKeyUp(int32 SlotIndex)
{
	if (SlotIndex == USkillManagerComponent::BasicAttackSlotIndex && bBasicAttackSuppressed)
	{
		bBasicAttackSuppressed = false;
		return;
	}

	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyUp(SlotIndex);
	}
}

USkillManagerComponent* ALoAPlayerController::GetSkillManager() const
{
	if (ALoACharacter* Char = Cast<ALoACharacter>(GetPawn()))
	{
		return Char->SkillManager;
	}
	return nullptr;
}

// ─── HUD 슬롯 클릭·드래그 스왑 ──────────────────────────────────────────────

namespace
{
	// 그룹별 슬롯 위젯 이름 (WBP_HUD) — 스킬은 순번이 곧 스킬 슬롯 번호(0=Q ... 7=F)
	const TCHAR* const HUDSkillBorders[] = { TEXT("Border_Q"), TEXT("Border_W"), TEXT("Border_E"), TEXT("Border_R"),
		TEXT("Border_A"), TEXT("Border_S"), TEXT("Border_D"), TEXT("Border_F") };
	const TCHAR* const HUDSkillImages[] = { TEXT("Img_Q"), TEXT("Img_W"), TEXT("Img_E"), TEXT("Img_R"),
		TEXT("Img_A"), TEXT("Img_S"), TEXT("Img_D"), TEXT("Img_F") };
	const TCHAR* const HUDItemBorders[] = { TEXT("Border_F1"), TEXT("Border_5"), TEXT("Border_6"), TEXT("Border_7"), TEXT("Border_8"), TEXT("Border_9") };
	const TCHAR* const HUDItemImages[] = { TEXT("Img_F1"), TEXT("Img_5"), TEXT("Img_6"), TEXT("Img_7"), TEXT("Img_8"), TEXT("Img_9") };
	const TCHAR* const HUDBattleBorders[] = { TEXT("Border_1"), TEXT("Border_2"), TEXT("Border_3"), TEXT("Border_4") };
	const TCHAR* const HUDBattleImages[] = { TEXT("Img_1"), TEXT("Img_2"), TEXT("Img_3"), TEXT("Img_4") };
	const TCHAR* const HUDOtherBorders[] = { TEXT("Border_Dash"), TEXT("Border_GetUp") };

	constexpr int32 HUDGroupSkill = 0;
	constexpr int32 HUDGroupItem = 1;
	constexpr int32 HUDGroupBattle = 2;
	constexpr int32 HUDGroupOther = 3;

	// 드래그로 인정하는 최소 이동 (화면 픽셀) — 그보다 짧으면 그냥 클릭
	constexpr float HUDSlotDragThreshold = 6.f;

	const TCHAR* GetHUDSlotImageName(int32 Group, int32 Index)
	{
		switch (Group)
		{
		case HUDGroupSkill:  return Index >= 0 && Index < UE_ARRAY_COUNT(HUDSkillImages) ? HUDSkillImages[Index] : nullptr;
		case HUDGroupItem:   return Index >= 0 && Index < UE_ARRAY_COUNT(HUDItemImages) ? HUDItemImages[Index] : nullptr;
		case HUDGroupBattle: return Index >= 0 && Index < UE_ARRAY_COUNT(HUDBattleImages) ? HUDBattleImages[Index] : nullptr;
		default:             return nullptr;
		}
	}
}

bool ALoAPlayerController::FindHUDSlotUnderCursor(int32& OutGroup, int32& OutIndex) const
{
	if (!HUDWidget || !HUDWidget->IsInViewport() || !FSlateApplication::IsInitialized()) return false;

	// Geometry는 데스크톱 절대 좌표 — 커서도 같은 공간으로 비교
	const FVector2D Cursor = FSlateApplication::Get().GetCursorPos();

	auto TestGroup = [&](const TCHAR* const* Names, int32 Count, int32 Group) -> bool
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const UWidget* Widget = HUDWidget->GetWidgetFromName(FName(Names[i]));
			if (!Widget || !Widget->IsVisible()) continue;
			if (Widget->GetCachedGeometry().IsUnderLocation(Cursor))
			{
				OutGroup = Group;
				OutIndex = i;
				return true;
			}
		}
		return false;
	};

	return TestGroup(HUDSkillBorders, UE_ARRAY_COUNT(HUDSkillBorders), HUDGroupSkill)
		|| TestGroup(HUDItemBorders, UE_ARRAY_COUNT(HUDItemBorders), HUDGroupItem)
		|| TestGroup(HUDBattleBorders, UE_ARRAY_COUNT(HUDBattleBorders), HUDGroupBattle)
		|| TestGroup(HUDOtherBorders, UE_ARRAY_COUNT(HUDOtherBorders), HUDGroupOther);
}

bool ALoAPlayerController::ConsumeHUDSlotPress()
{
	// 이미 같은 클릭을 처리 중 (이동·기본공격이 같은 버튼일 때 두 번 불림)
	if (bSlotPressActive) return true;

	// 상점 중 — 캐릭터는 붙잡혀 있고, 어떤 클릭도 이동·기본공격으로 쓰지 않는다
	if (bNPCMenuOpen) return true;

	// 소지품 창 위 — 보통은 UMG가 먼저 먹지만 혹시 새어 들어와도 이동·기본공격으로 쓰지 않는다
	if (InventoryWidget && InventoryWidget->IsCursorOverPanel()) return true;
	if (CharacterInfoWidget && CharacterInfoWidget->IsCursorOverPanel()) return true;
	if (EngravingSetupWidget && EngravingSetupWidget->IsCursorOverPanel()) return true;
	if (GemFuseWidget && GemFuseWidget->IsCursorOverPanel()) return true;
	if (DamageMeterWidget && DamageMeterWidget->IsCursorOverPanel()) return true;

	int32 Group = -1, Index = -1;
	if (!FindHUDSlotUnderCursor(Group, Index)) return false;

	// 드래그는 왼쪽 버튼으로만. 다른 버튼은 클릭만 먹고 끝
	if (IsInputKeyDown(EKeys::LeftMouseButton) && Group != HUDGroupOther)
	{
		bSlotPressActive = true;
		bSlotDragging = false;
		SlotPressGroup = Group;
		SlotPressIndex = Index;
		float X = 0.f, Y = 0.f;
		GetMousePosition(X, Y);
		SlotPressMouse = FVector2D(X, Y);
	}
	return true;
}

bool ALoAPlayerController::GetHUDSlotIconBrush(int32 Group, int32 Index, FSlateBrush& OutBrush) const
{
	const TCHAR* ImageName = GetHUDSlotImageName(Group, Index);
	const UImage* Image = (HUDWidget && ImageName) ? Cast<UImage>(HUDWidget->GetWidgetFromName(FName(ImageName))) : nullptr;
	if (!Image) return false;
	OutBrush = Image->GetBrush();
	return OutBrush.GetResourceObject() != nullptr;
}

void ALoAPlayerController::UpdateSlotDrag()
{
	if (!bSlotPressActive) return;

	float X = 0.f, Y = 0.f;
	const bool bHasMouse = GetMousePosition(X, Y);
	const FVector2D Mouse(X, Y);

	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		FinishSlotDrag();
		return;
	}

	if (!bSlotDragging && bHasMouse && FVector2D::Distance(Mouse, SlotPressMouse) >= HUDSlotDragThreshold)
	{
		// 빈 슬롯은 끌 게 없다
		FSlateBrush Brush;
		if (!GetHUDSlotIconBrush(SlotPressGroup, SlotPressIndex, Brush))
		{
			return;
		}

		// 쿨타임 중인 스킬은 집어 들 수도 없다
		if (SlotPressGroup == HUDGroupSkill)
		{
			const USkillManagerComponent* SM = GetSkillManager();
			if (SM && SM->IsSlotOnCooldown(SlotPressIndex))
			{
				return;
			}
		}

		bSlotDragging = true;
		if (!SlotDragVisual)
		{
			SlotDragVisual = CreateWidget<USlotDragVisualWidget>(this, USlotDragVisualWidget::StaticClass());
		}
		if (SlotDragVisual)
		{
			SlotDragVisual->SetIconBrush(Brush);
			if (!SlotDragVisual->IsInViewport())
			{
				// 스킬트리(10)·알림(11)보다 위
				SlotDragVisual->AddToViewport(20);
			}
			SlotDragVisual->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		}
	}

	if (bSlotDragging && SlotDragVisual && bHasMouse)
	{
		SlotDragVisual->SetPositionInViewport(Mouse, true);
	}
}

void ALoAPlayerController::FinishSlotDrag()
{
	if (bSlotDragging && SlotPressGroup == HUDGroupBattle && InventoryWidget && InventoryWidget->IsCursorOverPanel())
	{
		if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && Char->BattleItems)
		{
			FText Reason;
			if (!Char->BattleItems->Unequip(SlotPressIndex, Reason) && !Reason.IsEmpty())
			{
				ShowTimedNotice(Reason, FText::GetEmpty(), 1.5f);
			}
		}
	}
	else if (bSlotDragging)
	{
		int32 Group = -1, Index = -1;
		// 같은 종류 슬롯 위에서 놓았을 때만 스왑 — 스킬은 스킬끼리, 아이템은 아이템끼리, 배틀아이템은 배틀아이템끼리
		if (FindHUDSlotUnderCursor(Group, Index) && Group == SlotPressGroup && Index != SlotPressIndex)
		{
			SwapHUDSlots(Group, SlotPressIndex, Index);
		}
	}

	if (SlotDragVisual)
	{
		SlotDragVisual->RemoveFromParent();
	}
	bSlotPressActive = false;
	bSlotDragging = false;
	SlotPressGroup = -1;
	SlotPressIndex = -1;
}

void ALoAPlayerController::SwapHUDSlots(int32 Group, int32 IndexA, int32 IndexB)
{
	if (Group == HUDGroupSkill)
	{
		// 스킬은 실제 슬롯(인스턴스·쿨타임)을 맞바꾸고, 아이콘은 OnSkillSlotChanged로 HUD가 갱신
		if (USkillManagerComponent* SM = GetSkillManager())
		{
			const bool bOnCooldown = SM->IsSlotOnCooldown(IndexA) || SM->IsSlotOnCooldown(IndexB);
			if (!SM->SwapSkillSlots(IndexA, IndexB))
			{
				ShowTimedNotice(FText::FromString(bOnCooldown
					? TEXT("재사용 대기 중인 스킬은 옮길 수 없습니다.")
					: TEXT("스킬 사용 중에는 슬롯을 바꿀 수 없습니다.")), FText::GetEmpty(), 1.5f);
			}
		}
		return;
	}

	// 배틀아이템은 실제 칸(아이템·개수·쿨타임)을 맞바꾸고, 아이콘은 UpdateBattleItemHUD가 매 Tick 다시 그린다
	if (Group == HUDGroupBattle)
	{
		if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && Char->BattleItems)
		{
			Char->BattleItems->SwapSlots(IndexA, IndexB);
			return;
		}
	}

	// 아이템은 아직 아이템 시스템이 없어 아이콘(브러시)만 맞바꾼다
	const TCHAR* NameA = GetHUDSlotImageName(Group, IndexA);
	const TCHAR* NameB = GetHUDSlotImageName(Group, IndexB);
	UImage* ImageA = (HUDWidget && NameA) ? Cast<UImage>(HUDWidget->GetWidgetFromName(FName(NameA))) : nullptr;
	UImage* ImageB = (HUDWidget && NameB) ? Cast<UImage>(HUDWidget->GetWidgetFromName(FName(NameB))) : nullptr;
	if (!ImageA || !ImageB) return;

	const FSlateBrush BrushA = ImageA->GetBrush();
	const FSlateBrush BrushB = ImageB->GetBrush();
	const FLinearColor TintA = ImageA->GetColorAndOpacity();
	const FLinearColor TintB = ImageB->GetColorAndOpacity();
	ImageA->SetBrush(BrushB);
	ImageB->SetBrush(BrushA);
	ImageA->SetColorAndOpacity(TintB);
	ImageB->SetColorAndOpacity(TintA);
}

// ─── 배틀아이템 (1~4) ───────────────────────────────────────────────────────

void ALoAPlayerController::OnBattleItemKey(int32 SlotIndex)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->BattleItems) return;

	// 조종 불가(매혹)·행동불능(CC·사망·붙잡힘) 중엔 못 씀 — 컴포넌트도 같은 검사를 한다
	if (Char->IsCharmed() || Char->IsActionLocked()) return;

	Char->BattleItems->UseItem(SlotIndex);
}

void ALoAPlayerController::BuildBattleItemHUD()
{
	BattleItemIcons.Reset();
	BattleItemCooldownImages.Reset();
	BattleItemCooldownMIDs.Reset();
	BattleItemCooldownTexts.Reset();
	BattleItemCountTexts.Reset();

	if (!HUDWidget || !HUDWidget->WidgetTree) return;
	UWidgetTree* Tree = HUDWidget->WidgetTree;
	UMaterialInterface* CooldownMaterial = BattleItemCooldownMaterial.LoadSynchronous();

	auto MakeText = [Tree](const FString& Name, int32 Size) -> UTextBlock*
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*Name));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Text->SetVisibility(ESlateVisibility::Collapsed);
		return Text;
	};

	for (int32 i = 0; i < UE_ARRAY_COUNT(HUDBattleBorders); ++i)
	{
		UBorder* Border = Cast<UBorder>(HUDWidget->GetWidgetFromName(FName(HUDBattleBorders[i])));
		UImage* Icon = Cast<UImage>(HUDWidget->GetWidgetFromName(FName(HUDBattleImages[i])));
		if (!Border || !Icon)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BattleItem] HUD %s/%s 없음 — 칸 %d 표시 생략"), HUDBattleBorders[i], HUDBattleImages[i], i + 1);
			BattleItemIcons.Add(nullptr);
			BattleItemCooldownImages.Add(nullptr);
			BattleItemCooldownMIDs.Add(nullptr);
			BattleItemCooldownTexts.Add(nullptr);
			BattleItemCountTexts.Add(nullptr);
			continue;
		}

		// Border → Overlay → [아이콘, 원형 쿨타임, 쿨타임 초(가운데), 개수(우하단)] — 뒤에 넣은 게 위에 그려진다
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*FString::Printf(TEXT("BattleItemOverlay_%d"), i + 1)));
		Border->SetContent(Overlay);

		if (UOverlaySlot* S = Overlay->AddChildToOverlay(Icon))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UImage* Cooldown = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("BattleItemCooldown_%d"), i + 1)));
		UMaterialInstanceDynamic* MID = CooldownMaterial ? UMaterialInstanceDynamic::Create(CooldownMaterial, this) : nullptr;
		if (MID)
		{
			Cooldown->SetBrushFromMaterial(MID);
		}
		else
		{
			Cooldown->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		}
		Cooldown->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(Cooldown))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* CooldownText = MakeText(FString::Printf(TEXT("BattleItemCooldownTxt_%d"), i + 1), 14);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(CooldownText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* CountText = MakeText(FString::Printf(TEXT("BattleItemCountTxt_%d"), i + 1), 11);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(CountText))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(0.f, 0.f, 3.f, 1.f));
		}

		BattleItemIcons.Add(Icon);
		BattleItemCooldownImages.Add(Cooldown);
		BattleItemCooldownMIDs.Add(MID);
		BattleItemCooldownTexts.Add(CooldownText);
		BattleItemCountTexts.Add(CountText);
	}
}

void ALoAPlayerController::UpdateBattleItemHUD()
{
	if (BattleItemIcons.Num() == 0) return;

	const ALoACharacter* Char = GetPawn<ALoACharacter>();
	const UBattleItemComponent* Items = Char ? Char->BattleItems.Get() : nullptr;
	if (!Items) return;

	for (int32 i = 0; i < BattleItemIcons.Num(); ++i)
	{
		UImage* Icon = BattleItemIcons[i];
		UImage* Cooldown = BattleItemCooldownImages[i];
		UTextBlock* CooldownText = BattleItemCooldownTexts[i];
		UTextBlock* CountText = BattleItemCountTexts[i];
		if (!Icon || !Cooldown || !CooldownText || !CountText) continue;

		const FBattleItemSlot* Slot = Items->GetSlot(i);
		const FItemDefinition* Def = Slot ? LoAItems::Find(Slot->ItemId) : nullptr;
		if (!Def)
		{
			// 빈 칸 — 리소스도 비워야 드래그 시작(GetHUDSlotIconBrush)이 빈 칸으로 본다
			if (Icon->GetBrush().GetResourceObject())
			{
				Icon->SetBrushResourceObject(nullptr);
			}
			Icon->SetVisibility(ESlateVisibility::Hidden);
			Cooldown->SetVisibility(ESlateVisibility::Collapsed);
			CooldownText->SetVisibility(ESlateVisibility::Collapsed);
			CountText->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		UTexture2D* Texture = Def->Icon.LoadSynchronous();
		if (Texture && Icon->GetBrush().GetResourceObject() != Texture)
		{
			Icon->SetBrushResourceObject(Texture);
		}
		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 다 쓰면 칸에서 빼지 않고 빨간 반투명으로
		const bool bDepleted = Slot->Count <= 0;
		Icon->SetColorAndOpacity(bDepleted ? BattleItemDepletedTint : FLinearColor::White);

		CountText->SetText(FText::AsNumber(Slot->Count));
		CountText->SetVisibility(ESlateVisibility::HitTestInvisible);

		const float Ratio = bDepleted ? 0.f : Items->GetCooldownRatio(i);
		if (Ratio > 0.f)
		{
			if (UMaterialInstanceDynamic* MID = BattleItemCooldownMIDs[i])
			{
				MID->SetScalarParameterValue(TEXT("CooldownRatio"), Ratio);
			}
			Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
			CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Items->GetCooldownRemaining(i))));
			CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Cooldown->SetVisibility(ESlateVisibility::Collapsed);
			CooldownText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// ─── 소지품 (I) ─────────────────────────────────────────────────────────────

bool ALoAPlayerController::IsInventoryOpen() const
{
	return InventoryWidget && InventoryWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

void ALoAPlayerController::OnInventoryToggle()
{
	// 보스 맵 진입 연출 중·사망 후엔 못 연다
	if (bRaidIntroActive || bDefeatActive) return;
	SetInventoryOpen(!IsInventoryOpen());
}

void ALoAPlayerController::SetInventoryOpen(bool bOpen)
{
	if (!InventoryWidget) return;
	if (bOpen == IsInventoryOpen()) return;

	if (bOpen)
	{
		const ALoACharacter* Char = GetPawn<ALoACharacter>();
		InventoryWidget->SetInventory(Char ? Char->Inventory.Get() : nullptr);
		InventoryWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
		SetGemFuseOpen(false);  // 합성 창은 소지품 창에 딸린 창
	}
}

// ─── 캐릭터 정보 (P) ─────────────────────────────────────────────────────────

bool ALoAPlayerController::IsCharacterInfoOpen() const
{
	return CharacterInfoWidget && CharacterInfoWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

void ALoAPlayerController::OnCharacterInfoToggle()
{
	// 보스 맵 진입 연출 중·사망 후·상점 대화 중엔 못 연다
	if (bRaidIntroActive || bDefeatActive || bNPCMenuOpen) return;
	SetCharacterInfoOpen(!IsCharacterInfoOpen());
}

void ALoAPlayerController::SetCharacterInfoOpen(bool bOpen)
{
	if (!CharacterInfoWidget) return;
	if (bOpen == IsCharacterInfoOpen()) return;

	if (bOpen)
	{
		const ALoACharacter* Char = GetPawn<ALoACharacter>();
		CharacterInfoWidget->SetStatComponent(Char ? Char->StatComponent.Get() : nullptr);
		CharacterInfoWidget->SetBuildSources(Char ? Char->Gems.Get() : nullptr, Char ? Char->Engravings.Get() : nullptr,
			Char ? Char->Inventory.Get() : nullptr, Char ? Char->SkillManager.Get() : nullptr);
		if (CharacterPreviewActor)
		{
			// 열 때마다 복사 — 레벨 이동·캐릭터 교체 후에도 지금 캐릭터 모습
			CharacterPreviewActor->SetupFromCharacter(Char);
			CharacterPreviewActor->SetPreviewActive(true);
		}
		CharacterInfoWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		CharacterInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
		SetEngravingSetupOpen(false);
		if (CharacterPreviewActor)
		{
			CharacterPreviewActor->SetPreviewActive(false);
		}
	}
}

// ─── 보석·각인 (J) ─────────────────────────────────────────────────────────

bool ALoAPlayerController::IsGemFuseOpen() const
{
	return GemFuseWidget && GemFuseWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

void ALoAPlayerController::SetGemFuseOpen(bool bOpen)
{
	if (!GemFuseWidget) return;
	if (bOpen == IsGemFuseOpen()) return;

	if (bOpen)
	{
		const ALoACharacter* Char = GetPawn<ALoACharacter>();
		SetInventoryOpen(true);  // 합성 창은 소지품 창 옆에 붙는다
		GemFuseWidget->Open(Char ? Char->Gems.Get() : nullptr, InventoryWidget ? InventoryWidget->GetPanel() : nullptr);
		GemFuseWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		GemFuseWidget->Close();  // 올린 재료는 소지품으로
		GemFuseWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool ALoAPlayerController::IsEngravingSetupOpen() const
{
	return EngravingSetupWidget && EngravingSetupWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

void ALoAPlayerController::SetEngravingSetupOpen(bool bOpen)
{
	if (!EngravingSetupWidget) return;
	if (bOpen == IsEngravingSetupOpen()) return;

	if (bOpen)
	{
		const ALoACharacter* Char = GetPawn<ALoACharacter>();
		EngravingSetupWidget->Open(Char ? Char->Engravings.Get() : nullptr, Char ? Char->Inventory.Get() : nullptr);
		EngravingSetupWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		EngravingSetupWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ─── 데미지 미터 (L) ─────────────────────────────────────────────────────────

bool ALoAPlayerController::IsDamageMeterOpen() const
{
	return DamageMeterWidget && DamageMeterWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

void ALoAPlayerController::OnDamageMeterToggle()
{
	if (bRaidIntroActive) return;
	SetDamageMeterOpen(!IsDamageMeterOpen());
}

void ALoAPlayerController::SetDamageMeterOpen(bool bOpen)
{
	if (!DamageMeterWidget) return;
	if (bOpen == IsDamageMeterOpen()) return;

	if (bOpen)
	{
		const ALoACharacter* Char = GetPawn<ALoACharacter>();
		DamageMeterWidget->SetSources(DamageMeter, Char ? Char->SkillManager.Get() : nullptr);
		DamageMeterWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		DamageMeterWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ALoAPlayerController::HandleDamageMeterChanged()
{
	// 허수아비를 처음 때리면 한 번 자동으로 연다 — 닫은 뒤엔 다시 안 엶(L키로 직접)
	if (!bDamageMeterAutoOpened && DamageMeter && DamageMeter->IsTrainingSession())
	{
		bDamageMeterAutoOpened = true;
		SetDamageMeterOpen(true);
	}
}

void ALoAPlayerController::LoADpsTest(float Seconds)
{
	if (!DamageMeter) return;
	DamageMeter->StartDpsTest(Seconds);
	SetDamageMeterOpen(true);
}

void ALoAPlayerController::HandleInventoryDropOutside(int32 InventoryIndex)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->BattleItems) return;

	int32 Group = -1, Index = -1;
	if (!FindHUDSlotUnderCursor(Group, Index) || Group != HUDGroupBattle) return;

	FText Reason;
	if (!Char->BattleItems->EquipFromInventory(Index, InventoryIndex, Reason) && !Reason.IsEmpty())
	{
		ShowTimedNotice(Reason, FText::GetEmpty(), 1.5f);
	}
}

void ALoAPlayerController::LoAGiveItem(FName ItemId, int32 Count)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Inventory) return;
	if (!LoAItems::Find(ItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] LoAGiveItem — 없는 아이템 '%s'"), *ItemId.ToString());
		return;
	}
	const int32 Left = Char->Inventory->AddItem(ItemId, FMath::Max(1, Count));
	UE_LOG(LogTemp, Log, TEXT("[Cheat] LoAGiveItem %s x%d (못 넣은 개수 %d)"), *ItemId.ToString(), Count, Left);
}

void ALoAPlayerController::LoAInvTest()
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Inventory) return;

	TArray<int32> Empty;
	for (int32 i = 0; i < UInventoryComponent::Capacity; ++i)
	{
		if (Char->Inventory->GetSlot(i).IsEmpty()) Empty.Add(i);
	}
	for (int32 n = 0; n < 8 && Empty.Num() > 0; ++n)
	{
		const int32 Pick = FMath::RandRange(0, Empty.Num() - 1);
		Char->Inventory->DebugSetSlot(Empty[Pick], LoAItems::SpiritRecoveryPotion, FMath::RandRange(1, 20));
		Empty.RemoveAtSwap(Pick);
	}
	UE_LOG(LogTemp, Log, TEXT("[Cheat] LoAInvTest — 회복약 묶음을 흩뿌림"));
}

void ALoAPlayerController::LoAGiveSilling(int64 Amount)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Inventory) return;
	if (Amount >= 0) Char->Inventory->AddSilling(Amount);
	else Char->Inventory->SpendSilling(-Amount);
	UE_LOG(LogTemp, Log, TEXT("[Cheat] LoAGiveSilling %lld — 현재 %s 실링"), Amount, *LoAItems::FormatWithCommas(Char->Inventory->GetSilling()));
}

void ALoAPlayerController::LoAGiveGold(int64 Amount)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Inventory) return;
	if (Amount >= 0) Char->Inventory->AddGold(Amount);
	else Char->Inventory->SpendGold(-Amount);
	UE_LOG(LogTemp, Log, TEXT("[Cheat] LoAGiveGold %lld — 현재 %s 골드"), Amount, *LoAItems::FormatWithCommas(Char->Inventory->GetGold()));
}

void ALoAPlayerController::LoAStats()
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->StatComponent) return;
	TArray<FString> Lines = Char->StatComponent->DescribeStats();
	Lines.Add(FString::Printf(TEXT("캐릭터 반영값 — AttackPower %.0f / MaxHP %.0f / MaxWalkSpeed %.1f"),
		Char->AttackPower, Char->GetMaxHP(), Char->GetCharacterMovement()->MaxWalkSpeed));

	// UE_LOG는 Output Log에만 남아서 ~ 콘솔로는 안 보인다 → 콘솔(ClientMessage)·화면에도 같이 띄움
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		UE_LOG(LogTemp, Log, TEXT("[Stat] %s"), *Lines[i]);
		ClientMessage(Lines[i]);
		if (GEngine)
		{
			// 키를 줄마다 고정 → 다시 치면 같은 자리에서 갱신
			GEngine->AddOnScreenDebugMessage(9100 + i, 8.f, FColor::Cyan, Lines[i]);
		}
	}
}

void ALoAPlayerController::LoASetStat(const FString& Target, double Value)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->StatComponent) return;

	static const FName CheatSource(TEXT("Cheat"));

	if (Target.Equals(TEXT("Reset"), ESearchCase::IgnoreCase))
	{
		Char->StatComponent->ClearModifiers(CheatSource);
		UE_LOG(LogTemp, Log, TEXT("[Cheat] LoASetStat Reset — 임시 스탯 전부 제거"));
		LoAStats();
		return;
	}

	const UEnum* TargetEnum = StaticEnum<EStatModifierTarget>();
	const int64 EnumValue = TargetEnum->GetValueByNameString(Target);
	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] LoASetStat — 모르는 대상 '%s' (PrimaryStat/WeaponPower/Vitality/Crit/Specialization/Swiftness/Domination/Endurance/Expertise/CritRate/CritDamage/AdditiveDamage/MultiplicativeDamage)"), *Target);
		return;
	}
	const EStatModifierTarget ModTarget = static_cast<EStatModifierTarget>(EnumValue);

	TArray<FStatModifier> Mods = Char->StatComponent->GetModifiers(CheatSource);
	Mods.RemoveAll([ModTarget](const FStatModifier& M) { return M.Target == ModTarget; });
	if (!FMath::IsNearlyZero(Value))
	{
		FStatModifier& Mod = Mods.AddDefaulted_GetRef();
		Mod.Target = ModTarget;
		Mod.Value = Value;
	}
	Char->StatComponent->SetModifiers(CheatSource, Mods);

	bCheatUsedThisRaid = true;
	UE_LOG(LogTemp, Log, TEXT("[Cheat] LoASetStat %s %g — 이번 판은 기록하지 않음"), *Target, Value);
	LoAStats();
}

void ALoAPlayerController::LoASetRefine(const FString& Slot, int32 RefineLevel)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Equipment) return;

	TArray<EEquipSlot> Targets;
	if (Slot.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		for (int32 i = 0; i < LoAEquipRules::GearSlotCount; ++i) Targets.Add(static_cast<EEquipSlot>(i));
	}
	else
	{
		const int64 EnumValue = StaticEnum<EEquipSlot>()->GetValueByNameString(Slot);
		if (EnumValue == INDEX_NONE || EnumValue >= static_cast<int64>(EEquipSlot::MAX))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Cheat] LoASetRefine — 모르는 부위 '%s' (All/Helmet/Shoulder/Chest/Pants/Gloves/Weapon)"), *Slot);
			return;
		}
		Targets.Add(static_cast<EEquipSlot>(EnumValue));
	}

	int32 Changed = 0;
	for (const EEquipSlot Target : Targets)
	{
		if (Char->Equipment->DebugSetRefine(Target, RefineLevel)) ++Changed;
	}

	bCheatUsedThisRaid = true;
	const FString Line = FString::Printf(TEXT("[Cheat] LoASetRefine %s %d — %d부위 변경, 장착 아이템 레벨 %.2f"),
		*Slot, FMath::Clamp(RefineLevel, 0, LoAEquipRules::MaxRefineLevel), Changed, Char->Equipment->GetItemLevel());
	UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
	ClientMessage(Line);
}

void ALoAPlayerController::LoARefineSim(int32 TargetLevel, int32 Runs)
{
	double Average = 0.0;
	int32 Worst = 0;
	LoARefine::Simulate(TargetLevel, FMath::Clamp(Runs, 1, 1000000), Average, Worst);
	const FString Line = FString::Printf(TEXT("[Cheat] LoARefineSim +%d (기본 %.0f%%) x%d — 평균 %.2f회, 최악 %d회"),
		TargetLevel, LoARefine::GetBaseChance(TargetLevel) * 100.f, Runs, Average, Worst);
	UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
	ClientMessage(Line);
}

// ─── 상점 NPC ───────────────────────────────────────────────────────────────

void ALoAPlayerController::UpdateNPCInteraction()
{
	APawn* MyPawn = GetPawn();

	// 거리 안 가장 가까운 NPC
	AInteractableNPCActor* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	if (MyPawn && !bNPCMenuOpen)
	{
		for (TActorIterator<AInteractableNPCActor> It(GetWorld()); It; ++It)
		{
			if (!It->IsPawnInRange(MyPawn)) continue;
			const float DistSq = FVector::DistSquared2D(It->GetActorLocation(), MyPawn->GetActorLocation());
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = *It;
			}
		}
	}
	InRangeNPC = Nearest;

	// 커서 아래 NPC — 창 위에 커서가 있으면 무시
	AInteractableNPCActor* Hovered = nullptr;
	const bool bCursorOverUI = (InventoryWidget && InventoryWidget->IsCursorOverPanel()) || (ShopWidget && ShopWidget->IsCursorOverPanel()) || (RefineWidget && RefineWidget->IsCursorOverPanel())
		|| (CharacterInfoWidget && CharacterInfoWidget->IsCursorOverPanel())
		|| (EngravingSetupWidget && EngravingSetupWidget->IsCursorOverPanel())
		|| (GemFuseWidget && GemFuseWidget->IsCursorOverPanel())
		|| (DamageMeterWidget && DamageMeterWidget->IsCursorOverPanel());
	if (!bNPCMenuOpen && !bCursorOverUI)
	{
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			Hovered = Cast<AInteractableNPCActor>(Hit.GetActor());
		}
	}
	HoveredNPC = Hovered;

	for (TActorIterator<AInteractableNPCActor> It(GetWorld()); It; ++It)
	{
		AInteractableNPCActor* NPC = *It;
		NPC->SetHighlighted(!bNPCMenuOpen && (NPC == Nearest || NPC == Hovered));
		NPC->SetPromptVisible(!bNPCMenuOpen && NPC == Nearest);
	}
}

void ALoAPlayerController::OnInteractKey()
{
	if (bNPCMenuOpen) return;
	if (AInteractableNPCActor* NPC = InRangeNPC.Get())
	{
		OpenNPCMenu(NPC);
	}
}

void ALoAPlayerController::OpenNPCMenu(AInteractableNPCActor* NPC)
{
	if (bNPCMenuOpen || !NPC || !IsLocalController()) return;
	if (bRaidIntroActive || bDefeatActive || bClearActive || bLevelTravelPending) return;

	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || Char->IsActionLocked() || Char->IsCharmed() || !NPC->IsPawnInRange(Char)) return;

	bNPCMenuOpen = true;
	ActiveNPC = NPC;

	// 하던 이동 멈추고 상점 동안 제자리 — 플레이어는 NPC를 바라봄
	CancelAutoMove();
	bHoldMoving = false;
	Char->SetHeldByPattern(true);
	FVector ToNPC = NPC->GetActorLocation() - Char->GetActorLocation();
	ToNPC.Z = 0.f;
	if (!ToNPC.IsNearlyZero())
	{
		Char->SetActorRotation(FRotator(0.f, ToNPC.Rotation().Yaw, 0.f));
	}

	// 카메라 줌인 — NPC가 화면 오른쪽에 오도록 (카메라 오른쪽 = Yaw + 90)
	const float CameraYaw = ToNPC.IsNearlyZero() ? 0.f : ToNPC.Rotation().Yaw - 90.f;
	Char->SetCameraZoomOverride(NPC->MenuCameraArmLength);
	Char->SetCameraRotationOverride(FRotator(NPC->MenuCameraPitch, CameraYaw, 0.f));

	// HUD는 숨기고, 열려 있던 스킬창·캐릭터 정보 창은 닫는다
	SetCharacterInfoOpen(false);
	SetEngravingSetupOpen(false);
	SetGemFuseOpen(false);
	if (HUDWidget)
	{
		HUDVisibilityBeforeNPCMenu = static_cast<uint8>(HUDWidget->GetVisibility());
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (SkillTreeWidget && SkillTreeWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		SkillTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 하단 대화창
	if (!ShopDialogueWidget)
	{
		ShopDialogueWidget = CreateWidget<UShopDialogueWidget>(this, UShopDialogueWidget::StaticClass());
	}
	if (ShopDialogueWidget)
	{
		if (!ShopDialogueWidget->IsInViewport()) ShopDialogueWidget->AddToViewport(7);
		ShopDialogueWidget->SetDialogue(NPC->NPCName, NPC->GreetingLine);
		ShopDialogueWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// NPC 종류별 창
	if (AShopNPCActor* ShopNPC = Cast<AShopNPCActor>(NPC))
	{
		// 왼쪽 상점 창 + 바로 오른쪽 소지품 (둘 다 따로 끌 수 있음)
		if (!ShopWidget)
		{
			ShopWidget = CreateWidget<UShopWidget>(this, UShopWidget::StaticClass());
		}
		if (ShopWidget)
		{
			if (!ShopWidget->IsInViewport()) ShopWidget->AddToViewport(9);
			ShopWidget->OpenShop(ShopNPC->ShopItems, ShopNPC->CartSlotMaxCount, Char->Inventory.Get());
			ShopWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		SetInventoryOpen(true);
		if (InventoryWidget)
		{
			InventoryWidget->SetPanelPlacement(FVector2D(0.5f, 0.5f), FVector2D(0.f, 0.5f), FVector2D(4.f, -40.f));
		}
	}
	else if (Cast<ARefineNPCActor>(NPC))
	{
		// 가운데 재련 창 (소지품은 안 띄움 — 재료 보유량은 창 안에 나온다)
		SetInventoryOpen(false);
		if (!RefineWidget)
		{
			RefineWidget = CreateWidget<URefineWidget>(this, URefineWidget::StaticClass());
		}
		if (RefineWidget)
		{
			if (!RefineWidget->IsInViewport()) RefineWidget->AddToViewport(9);
			RefineWidget->OpenRefine(Char->Equipment.Get(), Char->Inventory.Get());
			RefineWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Refine] 재련 창 생성 실패"));
		}
	}

	NPC->SetHighlighted(false);
	NPC->SetPromptVisible(false);
	UE_LOG(LogTemp, Log, TEXT("[NPC] %s 메뉴 열림"), *NPC->NPCName.ToString());
}

bool ALoAPlayerController::IsShopOpen() const
{
	return bNPCMenuOpen && ShopWidget && Cast<AShopNPCActor>(ActiveNPC.Get()) != nullptr;
}

void ALoAPlayerController::CloseNPCMenu()
{
	if (!bNPCMenuOpen) return;
	bNPCMenuOpen = false;
	ActiveNPC = nullptr;

	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		Char->SetHeldByPattern(false);
		Char->ClearCameraZoomOverride();
		Char->ClearCameraRotationOverride();
	}

	if (HUDWidget)
	{
		HUDWidget->SetVisibility(static_cast<ESlateVisibility>(HUDVisibilityBeforeNPCMenu));
	}
	if (ShopWidget)
	{
		ShopWidget->CloseShop();
		ShopWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (RefineWidget)
	{
		RefineWidget->CloseRefine();
		RefineWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ShopDialogueWidget)
	{
		ShopDialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetInventoryOpen(false);
	if (InventoryWidget)
	{
		InventoryWidget->ResetPanelPlacement();
	}
	UE_LOG(LogTemp, Log, TEXT("[NPC] 메뉴 닫힘"));
}

void ALoAPlayerController::HandleInventorySlotRightClick(int32 InventoryIndex, bool bShift)
{
	if (IsShopOpen())
	{
		ShopWidget->HandleInventoryRightClick(InventoryIndex, bShift);
		return;
	}

	// 상점 밖 — 장비면 장착 (같은 부위 장비는 그 칸으로 돌아온다)
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Equipment) return;

	// 보석 / 유물 각인서 (7단계)
	if (Char->Inventory && Char->Inventory->IsValidIndex(InventoryIndex))
	{
		const FName ItemId = Char->Inventory->GetSlot(InventoryIndex).ItemId;
		const FItemDefinition* Def = LoAItems::Find(ItemId);
		if (Def && Def->Category == EItemCategory::Gem)
		{
			// 합성 창이 열려 있으면 재료로, 아니면 로아처럼 캐릭터 정보 [보석] 탭을 연 채로 우클릭 → 스킬을 골라 장착
			if (IsGemFuseOpen())
			{
				GemFuseWidget->AddInput(ItemId);
			}
			else if (IsCharacterInfoOpen() && CharacterInfoWidget->GetTab() == 2)
			{
				CharacterInfoWidget->BeginGemEquip(ItemId);
			}
			else
			{
				ShowTimedNotice(NSLOCTEXT("Gem", "GemTitle", "보석"), NSLOCTEXT("Gem", "OpenGemTab", "캐릭터 정보(P)의 [보석] 탭을 연 채로 우클릭하면 장착할 수 있습니다."));
			}
			return;
		}
		if (Def && Def->Category == EItemCategory::Book && Char->Engravings)
		{
			const double BookNow = GetWorld()->GetTimeSeconds();
			if (InventoryIndex == LastEquipClickIndex && BookNow - LastEquipClickTime < 0.3) return;  // 빠른 연타로 두 권 읽지 않게
			LastEquipClickIndex = InventoryIndex;
			LastEquipClickTime = BookNow;
			if (const FEngravingDefinition* Eng = LoAEngravings::FindByBook(ItemId))
			{
				FText Fail;
				if (Char->Engravings->ReadBook(Eng->Id, Fail))
				{
					const int32 Books = Char->Engravings->GetBooksRead(Eng->Id);
					ShowTimedNotice(NSLOCTEXT("Engraving", "BookTitle", "각인서"), FText::FromString(FString::Printf(TEXT("%s — 유물 %d단계 (읽은 각인서 %d / %d권)"),
						*Eng->Name.ToString(), Char->Engravings->GetLevel(Eng->Id), Books, LoAEngravings::MaxBooks)));
				}
				else if (!Fail.IsEmpty())
				{
					ShowTimedNotice(NSLOCTEXT("Engraving", "BookTitle", "각인서"), Fail);
				}
			}
			return;
		}
	}

	// 빠른 연타의 두 번째 클릭(DoubleClick)도 우클릭으로 들어온다 — 그대로 처리하면 장착 직후 돌아온 장비를 다시 껴서
	// 원래대로 되돌아가므로 같은 칸 0.3초 안 재클릭은 무시
	const double Now = GetWorld()->GetTimeSeconds();
	if (InventoryIndex == LastEquipClickIndex && Now - LastEquipClickTime < 0.3) return;
	LastEquipClickIndex = InventoryIndex;
	LastEquipClickTime = Now;

	FText FailReason;
	if (!Char->Equipment->EquipFromInventory(InventoryIndex, FailReason) && !FailReason.IsEmpty())
	{
		ShowTimedNotice(NSLOCTEXT("Equipment", "EquipFailTitle", "장비"), FailReason);
	}
}

void ALoAPlayerController::HandleEquipSlotRightClick(EEquipSlot Slot)
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->Equipment) return;
	FText FailReason;
	if (!Char->Equipment->Unequip(Slot, FailReason) && !FailReason.IsEmpty())
	{
		ShowTimedNotice(NSLOCTEXT("Equipment", "EquipFailTitle", "장비"), FailReason);
	}
}
