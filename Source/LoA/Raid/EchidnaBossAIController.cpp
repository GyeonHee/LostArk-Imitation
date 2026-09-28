#include "EchidnaBossAIController.h"
#include "Components/StateTreeAIComponent.h"

AEchidnaBossAIController::AEchidnaBossAIController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAI);

	// AI 시작 경로가 두 개라 둘 다 끈다 — 빙의 시(AAIController::OnPossess)와 컴포넌트 BeginPlay(UStateTreeComponent::bStartLogicAutomatically).
	// 보스 맵 진입 인트로 동안 보스가 가만히 있어야 해서, 실제 시작은 AEchidnaBoss::StartCombat → StartBossLogic
	bStartAILogicOnPossess = false;
	StateTreeAI->SetStartLogicAutomatically(false);
	bAttachToPawn = true;
}

void AEchidnaBossAIController::StartBossLogic()
{
	if (StateTreeAI && !StateTreeAI->IsRunning())
	{
		StateTreeAI->StartLogic();
	}
}

void AEchidnaBossAIController::StopBossLogic()
{
	if (StateTreeAI && StateTreeAI->IsRunning())
	{
		StateTreeAI->StopLogic(TEXT("BossDefeated"));
	}
}
