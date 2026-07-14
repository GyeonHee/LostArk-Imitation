#include "EchidnaBossAIController.h"
#include "Components/StateTreeAIComponent.h"

AEchidnaBossAIController::AEchidnaBossAIController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAI);

	bStartAILogicOnPossess = true;
	bAttachToPawn = true;
}
