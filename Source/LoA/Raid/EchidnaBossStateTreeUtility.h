#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "EchidnaBossStateTreeUtility.generated.h"

class AEchidnaBoss;
class AEchidnaMirrorActor;
class AAIController;

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

/**
 * FStateTreeTask_EchidnaFourMirrorPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaFourMirrorPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	UPROPERTY(EditAnywhere, Category = "Mirror")
	TSubclassOf<AEchidnaMirrorActor> MirrorClass;

	// 보스 중심에서 거울까지 배치 거리 (cm) — 보스 정면 기준 대각 4방향(45/135/225/315도)에 배치
	UPROPERTY(EditAnywhere, Category = "Mirror")
	float MirrorSpawnRadius = 500.f;

	UPROPERTY(EditAnywhere, Category = "Mirror")
	float Damage = 10.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaMirrorActor>> SpawnedMirrors;
};

/**
 * "4거울" 짤패턴 — 보스 정면 기준 대각 4방향(45/135/225/315도)에 거울을 동시에 스폰한다.
 * 각 거울은 스폰 직후부터 개별적으로 플레이어를 추적 조준하다가 스스로 빛줄기를 발사한다
 * (거울 개별 동작은 AEchidnaMirrorActor 참조). 이 Task는 스폰만 담당하고,
 * 스폰된 거울 전부가 발사를 마칠 때까지 State를 Running으로 유지한다.
 */
USTRUCT(meta = (DisplayName = "Echidna Four Mirror Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaFourMirrorPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaFourMirrorPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};

/**
 * FStateTreeTask_EchidnaPatrol의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaPatrolInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	// 보스 현재 위치 기준으로 패트롤 목표 지점을 고를 반경 (cm)
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolRadius = 600.f;

	// 목표 지점 도착 판정 반경 (cm)
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float AcceptanceRadius = 50.f;
};

/**
 * 쿨다운(짤패턴 사이 대기) 중 보스가 제자리에 멈춰있지 않도록 무작위 지점으로 걸어가게 하는 패트롤.
 * 목적지에 도착하거나(Succeeded) State가 중간에 다른 이유로 끝나면(ExitState에서 StopMovement) 종료.
 * 레벨에 Nav Mesh Bounds Volume이 없으면 MoveToLocation이 실패해서 보스가 움직이지 않는다.
 */
USTRUCT(meta = (DisplayName = "Echidna Patrol", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaPatrol : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaPatrolInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR
};
