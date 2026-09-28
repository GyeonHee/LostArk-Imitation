#include "Lobby/RepairStationActor.h"
#include "LoACharacter.h"
#include "Skill/SkillManagerComponent.h"
#include "TimerManager.h"

ARepairStationActor::ARepairStationActor()
{
	BorderSides = 64;
	BorderColor = FLinearColor(6.f, 4.f, 1.2f, 0.9f);
	FillColor = FLinearColor(1.f, 0.8f, 0.3f, 0.12f);
	LabelColor = FLinearColor(1.f, 0.85f, 0.45f, 1.f);

	LabelText = NSLOCTEXT("Lobby", "RepairLabel", "정비소");
	NoticeTitle = NSLOCTEXT("Lobby", "RepairTitle", "정비소");
	NoticeMessage = NSLOCTEXT("Lobby", "RepairMessage", "정비가 완료되었습니다. 이곳에서 스킬을 등록할 수 있습니다.");
}

void ARepairStationActor::OnPlayerEnterZone(APawn* Pawn)
{
	ALoACharacter* Player = Cast<ALoACharacter>(Pawn);
	if (!Player) return;

	Player->RestoreFullStatus();
	if (USkillManagerComponent* SkillManager = Player->FindComponentByClass<USkillManagerComponent>())
	{
		SkillManager->SetSlotEditAllowed(true);
		if (bResetSkillCooldowns)
		{
			SkillManager->ResetAllCooldowns();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RepairStation] 정비 완료 — HP/MP 회복%s"), bResetSkillCooldowns ? TEXT(" + 쿨타임 초기화") : TEXT(""));

	ShowNotice(NoticeTitle, NoticeMessage);
	GetWorldTimerManager().SetTimer(NoticeTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { HideNotice(); }), NoticeDuration, false);
}

void ARepairStationActor::OnPlayerExitZone(APawn* Pawn)
{
	if (!Pawn) return;
	if (USkillManagerComponent* SkillManager = Pawn->FindComponentByClass<USkillManagerComponent>())
	{
		SkillManager->SetSlotEditAllowed(false);
	}
}
