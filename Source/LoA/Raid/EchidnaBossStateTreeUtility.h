#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "EchidnaBossStateTreeUtility.generated.h"

class AEchidnaBoss;
class AEchidnaMirrorActor;
class AEchidnaFanZoneActor;
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

UENUM()
enum class EEchidnaRetreatFanPhase : uint8
{
	Casting1,	// 1번째 장판(왼쪽으로 비스듬히) 진행 중
	Casting2,	// 2번째 장판(오른쪽으로 비스듬히) 진행 중
	Done
};

/**
 * FStateTreeTask_EchidnaRetreatFanPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaRetreatFanPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동(패트롤 등)을 멈추는 용도로만 사용 — 후퇴 이동에는 더 이상 쓰이지 않음
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, Category = "Fan")
	TSubclassOf<AEchidnaFanZoneActor> FanZoneClass;

	// 정면(플레이어 방향) 기준 좌/우로 얼마나 비스듬히 쏠지 (도) — 1번은 -값(왼쪽), 2번은 +값(오른쪽)으로 적용.
	// FanZoneClass의 FanAngle 절반보다 작아야 가운데가 겹침
	UPROPERTY(EditAnywhere, Category = "Fan")
	float FanYawOffset = 32.f;

	// 장판이 실제로 터지는 순간(HasStartedExploding) 보스를 뒤로 밀어내는 힘 (수평, cm/s)
	UPROPERTY(EditAnywhere, Category = "Fan")
	float HopBackStrength = 500.f;

	// 같은 순간 위로 띄우는 힘 (cm/s) — 값이 있어야 점프하듯 포물선을 그림
	UPROPERTY(EditAnywhere, Category = "Fan")
	float HopUpwardStrength = 300.f;

	// 착지 예상 지점(뒤 방향으로 이 거리만큼)에 바닥이 있는지 미리 검사 — 없으면 맵 밖으로 떨어지지 않도록 홉 자체를 취소
	UPROPERTY(EditAnywhere, Category = "Fan")
	float HopCheckDistance = 350.f;

	UPROPERTY(EditAnywhere, Category = "Fan")
	float Damage = 10.f;

	UPROPERTY(Transient)
	EEchidnaRetreatFanPhase Phase = EEchidnaRetreatFanPhase::Casting1;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaFanZoneActor> CurrentFan;

	// 현재 진행 중인 장판에 대해 이미 후방 홉을 실행했는지 (장판 하나당 1회만)
	UPROPERTY(Transient)
	bool bHoppedForCurrentCast = false;

	// 패턴 시작(EnterState) 시점에 한 번만 계산해서 고정하는 기준 조준 방향 — 1번/2번 캐스팅 모두 이 값 + YawOffsetDeg를 씀.
	// 매 캐스팅마다 플레이어 위치를 다시 조준하면 후퇴/이동 중 기준선이 흔들려 1번·2번이 어긋나 보이므로 시작 시점 값으로 고정
	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;
};

/**
 * "뒤로 빠지며 좌우장판" 짤패턴 — 보스가 제자리에 멈춘 채로 정면 기준 왼쪽으로 비스듬한 부채꼴(1)을
 * 먼저 터뜨리고, 이어서 오른쪽으로 비스듬한 부채꼴(2)을 터뜨린다 (가운데는 두 부채꼴이 겹침).
 * 캐스팅 직전 보스를 그 장판이 날아가는 방향으로 회전시켜 실제로 조준하는 것처럼 보이게 한다.
 * 각 장판이 예고를 마치고 실제 판정이 시작되는 순간(AEchidnaFanZoneActor::HasStartedExploding)
 * 보스를 LaunchCharacter로 살짝 뒤로 띄워 점프하듯 물러나게 한다 (장판당 1회) —
 * 단, 착지 예상 지점에 바닥이 없으면(맵 끝자락) 홉 자체를 취소해 낙사를 막는다.
 * 장판 2까지 다 끝나면(IsFinished) Succeeded.
 */
USTRUCT(meta = (DisplayName = "Echidna Retreat Fan Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaRetreatFanPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaRetreatFanPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	FRotator ComputeAimRotation(const AEchidnaBoss* Boss) const;
	AEchidnaFanZoneActor* SpawnFan(FInstanceDataType& InstanceData, float YawOffsetDeg) const;
	void HopBackward(FInstanceDataType& InstanceData) const;
	bool HasGroundBelow(const FInstanceDataType& InstanceData, const FVector& Location) const;
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
