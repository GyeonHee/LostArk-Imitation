#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "EchidnaBossStateTreeUtility.generated.h"

class AEchidnaBoss;

/**
 * FStateTreeCondition_BossLineThreshold의 Instance Data
 */
USTRUCT()
struct FStateTreeBossLineThresholdInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 이 줄 이하로 내려가면 조건 통과 (예: 210)
	UPROPERTY(EditAnywhere, Category = "Condition")
	int32 TriggerLine = 0;

	// AEchidnaBoss::BigPatternThresholds의 PatternName과 일치해야 함 — 이미 발동한 패턴은 다시 통과하지 않음
	UPROPERTY(EditAnywhere, Category = "Condition")
	FName PatternName;
};

/**
 * 보스 HP가 지정한 줄 이하이면서 아직 발동하지 않은 패턴이면 통과하는 조건.
 * 대형 패턴 State의 Enter Condition으로 사용 — SmallPatternRotation보다 우선순위 높은 상태에 걸어두면
 * HP가 트리거 줄을 통과하는 즉시 자동으로 전이된다.
 */
USTRUCT(meta = (DisplayName = "Boss Line Threshold Reached", Category = "EchidnaBoss"))
struct FStateTreeCondition_BossLineThreshold : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeBossLineThresholdInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

/**
 * FStateTreeTask_MarkBossPatternTriggered의 Instance Data
 */
USTRUCT()
struct FStateTreeMarkBossPatternTriggeredInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	UPROPERTY(EditAnywhere, Category = "Task")
	FName PatternName;
};

/**
 * 대형 패턴 State 진입 시 호출 — 해당 패턴을 "발동함"으로 표시해서
 * 이후 같은 HP 구간을 다시 지나가도(또는 State가 재진입해도) 재발동하지 않게 한다.
 */
USTRUCT(meta = (DisplayName = "Mark Boss Pattern Triggered", Category = "EchidnaBoss"))
struct FStateTreeTask_MarkBossPatternTriggered : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeMarkBossPatternTriggeredInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

/**
 * FStateTreeTask_WaitRandomDuration의 Instance Data
 */
USTRUCT()
struct FStateTreeWaitRandomDurationInstanceData
{
	GENERATED_BODY()

	// 짤패턴 사이 대기시간 최소/최대 (초) — 로테이션 텀 조절용
	UPROPERTY(EditAnywhere, Category = "Task")
	float MinDuration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Task")
	float MaxDuration = 4.f;

	UPROPERTY()
	float RemainingTime = 0.f;
};

/**
 * Min~Max 사이 랜덤 시간을 대기한 뒤 Succeeded — 짤패턴 로테이션의 대기 구간에 사용.
 */
USTRUCT(meta = (DisplayName = "Wait Random Duration", Category = "EchidnaBoss"))
struct FStateTreeTask_WaitRandomDuration : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeWaitRandomDurationInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};
