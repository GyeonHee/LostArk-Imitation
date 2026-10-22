#include "Lobby/RaidEntranceActor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Item/EquipmentComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ARaidEntranceActor::ARaidEntranceActor()
{
	// 팔각형 — 22.5도 돌려서 변이 축에 나란하게
	BorderSides = 8;
	BorderAngleOffset = 22.5f;
	ZoneRadius = 280.f;
	BorderColor = FLinearColor(1.5f, 0.6f, 6.f, 0.9f);
	FillColor = FLinearColor(0.5f, 0.3f, 1.f, 0.14f);
	LabelColor = FLinearColor(0.85f, 0.7f, 1.f, 1.f);

	TargetLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/LostArk/Level/Echidna2-1.Echidna2-1")));

	LabelText = NSLOCTEXT("Lobby", "EntranceLabel", "에키드나 2관문");
	NoticeTitle = NSLOCTEXT("Lobby", "EntranceTitle", "에키드나 2관문");
	NoticeMessage = NSLOCTEXT("Lobby", "EntranceMessage", "잠시 후 다음 지역으로 이동됩니다.");
}

void ARaidEntranceActor::OnPlayerEnterZone(APawn* Player)
{
	if (bTravelling) return;

	float ItemLevel = 0.f;
	if (!MeetsItemLevel(Player, ItemLevel))
	{
		bBlockedByItemLevel = true;
		ShowBlockedNotice(ItemLevel);
		UE_LOG(LogTemp, Log, TEXT("[RaidEntrance] 입장 불가 — 아이템 레벨 %.2f < %d"), ItemLevel, RequiredItemLevel);
		return;
	}
	StartCountdown();
}

bool ARaidEntranceActor::MeetsItemLevel(APawn* Player, float& OutItemLevel) const
{
	OutItemLevel = 0.f;
	const ALoACharacter* Character = Cast<ALoACharacter>(Player);
	if (RequiredItemLevel <= 0 || !Character || !Character->Equipment) return true;
	OutItemLevel = Character->Equipment->GetItemLevel();
	// 평균이라 1619.999… 같은 부동소수 오차로 막히지 않게 약간 여유
	return OutItemLevel + KINDA_SMALL_NUMBER * 10.f >= static_cast<float>(RequiredItemLevel);
}

void ARaidEntranceActor::StartCountdown()
{
	bBlockedByItemLevel = false;
	Remaining = EnterCountdown;
	ShownSeconds = -1;
	ShowNotice(NoticeTitle, NoticeMessage);
	UpdateCounterText();
	UE_LOG(LogTemp, Log, TEXT("[RaidEntrance] 입장 카운트다운 시작 (%.1f초)"), EnterCountdown);
}

void ARaidEntranceActor::ShowBlockedNotice(float ItemLevel)
{
	ShownBlockedItemLevel = ItemLevel;
	FNumberFormattingOptions Fmt;
	Fmt.MinimumFractionalDigits = 2;
	Fmt.MaximumFractionalDigits = 2;
	Fmt.UseGrouping = false;
	ShowNotice(NoticeTitle,
		FText::Format(NSLOCTEXT("Lobby", "EntranceItemLevel", "입장 가능 아이템 레벨 {0}"), FText::AsNumber(RequiredItemLevel, &FNumberFormattingOptions::DefaultNoGrouping())),
		FText::Format(NSLOCTEXT("Lobby", "EntranceCurrentItemLevel", "현재 {0}"), FText::AsNumber(ItemLevel, &Fmt)));
}

void ARaidEntranceActor::OnPlayerExitZone(APawn* Player)
{
	// 이동이 확정된 뒤(초록 테두리)엔 나가도 취소하지 않는다
	if (bTravelling) return;

	bBlockedByItemLevel = false;
	Remaining = EnterCountdown;
	ShownSeconds = -1;
	HideNotice();
	UE_LOG(LogTemp, Log, TEXT("[RaidEntrance] 구역 이탈 — 카운트다운 초기화"));
}

void ARaidEntranceActor::TickPlayerInZone(APawn* Player, float DeltaSeconds)
{
	if (bTravelling) return;

	// 구역 안에서 장비를 바꿀 수 있으므로 매 틱 다시 확인 — 조건을 넘기면 그 자리에서 시작, 내려가면 다시 막음
	float ItemLevel = 0.f;
	const bool bMeets = MeetsItemLevel(Player, ItemLevel);
	if (!bMeets)
	{
		if (!bBlockedByItemLevel || !FMath::IsNearlyEqual(ItemLevel, ShownBlockedItemLevel, 0.005f))
		{
			bBlockedByItemLevel = true;
			ShowBlockedNotice(ItemLevel);
		}
		return;
	}
	if (bBlockedByItemLevel)
	{
		StartCountdown();
		return;
	}

	Remaining -= DeltaSeconds;
	if (Remaining <= 0.f)
	{
		BeginTravel();
		return;
	}
	UpdateCounterText();
}

void ARaidEntranceActor::UpdateCounterText()
{
	// 표시 초는 올림 — 3.0~2.01초 동안 "3초"
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Remaining));
	if (Seconds == ShownSeconds) return;
	ShownSeconds = Seconds;
	ShowNotice(NoticeTitle, NoticeMessage, FText::Format(NSLOCTEXT("Lobby", "EntranceCounter", "{0}초"), FText::AsNumber(Seconds)));
}

void ARaidEntranceActor::BeginTravel()
{
	if (TargetLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaidEntrance] TargetLevel 미설정 — 이동 불가"));
		HideNotice();
		return;
	}

	bTravelling = true;
	SetCurrentBorderColor(ReadyBorderColor);

	// 이동 확정 — 레벨이 바뀔 때까지 제자리에 묶는다. 붙잡힘은 이동·스킬·대시 입력을 전부 막고,
	// 새 레벨에선 캐릭터가 새로 스폰되므로 따로 풀 필요가 없다
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ALoACharacter* Character = Cast<ALoACharacter>(Pawn))
		{
			Character->SetHeldByPattern(true);
		}
		else if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
			Pawn->DisableInput(nullptr);
		}
	}
	ShowNotice(NoticeTitle, NSLOCTEXT("Lobby", "EntranceTravelling", "이동합니다."));
	UE_LOG(LogTemp, Log, TEXT("[RaidEntrance] 카운트다운 완료 — 테두리 초록, %.1f초 뒤 이동"), TravelDelay);

	if (TravelDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(TravelTimer, this, &ARaidEntranceActor::TravelToTarget, TravelDelay, false);
	}
	else
	{
		TravelToTarget();
	}
}

void ARaidEntranceActor::TravelToTarget()
{
	UE_LOG(LogTemp, Log, TEXT("[RaidEntrance] %s 로 이동"), *TargetLevel.ToString());
	// 검게 페이드 아웃하며 이동 — 보스 맵에선 인트로가 페이드 인
	if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->TravelToLevelWithFade(TargetLevel);
	}
	else
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);
	}
}
