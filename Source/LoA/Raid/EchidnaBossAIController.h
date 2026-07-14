#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EchidnaBossAIController.generated.h"

class UStateTreeAIComponent;

/**
 * 에키드나 보스 전용 AIController — StateTree로 패턴 로테이션/대형 패턴 전이를 구동한다.
 */
UCLASS()
class LOA_API AEchidnaBossAIController : public AAIController
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

public:
	AEchidnaBossAIController();
};
