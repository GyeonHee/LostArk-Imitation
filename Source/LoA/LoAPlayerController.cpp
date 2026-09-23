// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoAPlayerController.h"
#include "UI/ScreenFogWidget.h"
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

ALoAPlayerController::ALoAPlayerController()
{
	bIsTouch = false;
	bMoveToMouseCursor = false;
	ShortPressThreshold = 0.5f;

	// create the path following comp
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));

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

	// OnPossess가 BeginPlay 전에 이미 호출된 경우를 대비해 여기서도 바인딩 시도
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		BindCharacterEvents(Char);
		if (Char->SkillManager && SkillTreeViewModel)
		{
			SkillTreeViewModel->Initialize(Char->SkillManager);
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
}

void ALoAPlayerController::OnBossHPChanged(double NewHP, double NewMaxHP)
{
	if (!BossHPWidget || !TrackedBoss.IsValid()) return;

	BossHPWidget->SetBossHP(NewHP, NewMaxHP, TrackedBoss->TotalLines);
}

void ALoAPlayerController::SetScreenFog(bool bEnable, float FadeTime)
{
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

	HUDViewModel->SetMaxHP(InCharacter->GetMaxHP());
	HUDViewModel->SetHP(InCharacter->GetHP());
	HUDViewModel->SetMaxMP(InCharacter->GetMaxMP());
	HUDViewModel->SetMP(InCharacter->GetMP());

	InCharacter->OnHPChanged.AddUObject(this, &ALoAPlayerController::OnPlayerHPChanged);
	InCharacter->OnMPChanged.AddUObject(this, &ALoAPlayerController::OnPlayerMPChanged);
	InCharacter->OnCharmedChanged.AddUObject(this, &ALoAPlayerController::OnPlayerCharmedChanged);
}

void ALoAPlayerController::OnSkillTreeToggle()
{
	if (!SkillTreeWidget) return;

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
	}
	else
	{
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

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent());
	if (!CMC) return;

	// 행동불능 체크보다 먼저 — 캐스팅 도중 기절/넉다운으로 스킬이 끊겼을 때도 바가 남지 않고 사라져야 한다
	UpdateCastBar();
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
	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && (Char->IsActionLocked() || Char->IsCharmed()))
	{
		return;
	}

	bAutoMoving = false;
	bHoldMoving = true;

	FollowTime += GetWorld()->GetDeltaSeconds();

	// 커서 위치 업데이트만 - 실제 이동은 Tick에서 처리
	UpdateCachedDestination();
}

void ALoAPlayerController::OnSetDestinationReleased()
{
	bHoldMoving = false;

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
	if (Char->IsStaggered() || Char->IsCharmed()) return;

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
	if (ALoACharacter* Char = GetPawn<ALoACharacter>(); Char && (Char->IsActionLocked() || Char->IsCharmed()))
	{
		return;
	}

	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyDown(SlotIndex);
	}
}

void ALoAPlayerController::OnSkillKeyHeld(int32 SlotIndex)
{
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
