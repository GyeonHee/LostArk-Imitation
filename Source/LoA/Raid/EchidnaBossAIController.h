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

	// StateTree 가동 — 빙의·BeginPlay 때 자동으로 시작하지 않고 AEchidnaBoss::StartCombat(인트로 연출 뒤)이 부른다
	void StartBossLogic();

	// StateTree 정지 — 보스 처치 시
	void StopBossLogic();
};
