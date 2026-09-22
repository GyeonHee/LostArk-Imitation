#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "EchidnaBossStateTreeUtility.generated.h"

class AEchidnaBoss;
class AEchidnaMirrorActor;
class AEchidnaFanZoneActor;
class AEchidnaTetherActor;
class AEchidnaHeartActor;
class AEchidnaOrbActor;
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

	// 레이저 1틱당 데미지 — 1초간 4틱이라 풀히트 시 플레이어 최대체력(10만)의 16%
	UPROPERTY(EditAnywhere, Category = "Mirror")
	float Damage = 4000.f;

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
enum class EEchidnaEightMirrorPhase : uint8
{
	PlusWave,	// 1차 — 미리 스폰해둔 "+" 대형(0/90/180/270도) 4개 Activate
	CrossWave,	// 2차 — 미리 스폰해둔 "X" 대형(45/135/225/315도) 4개 Activate
	Done
};

/**
 * FStateTreeTask_EchidnaEightMirrorPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaEightMirrorPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	UPROPERTY(EditAnywhere, Category = "Mirror")
	TSubclassOf<AEchidnaMirrorActor> MirrorClass;

	// 보스 중심에서 거울까지 배치 거리 (cm)
	UPROPERTY(EditAnywhere, Category = "Mirror")
	float MirrorSpawnRadius = 500.f;

	// 거울이 8개라 동시 피격이 잦음 — 개당 데미지는 4거울(4000)보다 낮게
	UPROPERTY(EditAnywhere, Category = "Mirror")
	float Damage = 3000.f;

	// "개인 유도레이저" — 8거울과 별개로 패턴 시작 시 1개만 스폰, 패턴이 끝날 때까지 계속 추적+반복 발사
	UPROPERTY(EditAnywhere, Category = "Guided")
	float GuidedDamage = 5000.f;

	// 유도 거울이 처음 스폰될 때 보스 오른쪽으로 얼마나 떨어진 지점인지 (cm) — 스폰 직후부터 플레이어를 쫓아다님.
	// (필드 이름은 GuidedHoverHeight로 남아있지만 용도가 "스폰 오프셋"으로 바뀜 — StateTree 인스턴스 데이터
	// 구조체 레이아웃을 또 바꾸면 이미 배치된 ST_Echidna 태스크가 Live Coding에서 크래시 나서 필드 재사용함)
	UPROPERTY(EditAnywhere, Category = "Guided", meta = (DisplayName = "Guided Spawn Offset"))
	float GuidedHoverHeight = 500.f;

	UPROPERTY(Transient)
	EEchidnaEightMirrorPhase Phase = EEchidnaEightMirrorPhase::PlusWave;

	// 8개 전부 — 패턴 시작 시 한 번에 스폰(전부 화면에 존재), "+" 4개만 먼저 Activate되고 "X" 4개는 대기하다 2파동에 Activate
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaMirrorActor>> PlusMirrors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaMirrorActor>> CrossMirrors;

	// 패턴 시작 시 1회만 스폰되어 패턴이 끝날 때까지 독립적으로 반복 추적+발사하는 유도 거울
	UPROPERTY(Transient)
	TObjectPtr<AEchidnaMirrorActor> GuidedMirror;

	// 패턴 시작(EnterState) 시점에 한 번만 고정하는 기준 방향 — "+"(0/90/180/270)/"X"(45/135/225/315) 스포크 각도의 기준선
	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;
};

/**
 * "거울 8개 레이저" 짤패턴 — 패턴 시작 시 보스 중심으로 "+"대형(0/90/180/270도)과 "X"대형(45/135/225/315도)
 * 총 8개의 고정 스포크 거울을 한꺼번에 스폰한다(전부 화면에 존재) — 단, 실제 판정(장판)은 "+" 4개가 먼저
 * 발동(Activate)하고, 그게 끝나면 미리 스폰해둔 "X" 4개가 그제서야 Activate되어 순차적으로 나간다
 * (플레이어를 쫓지 않고 스폰 방향 고정 — AEchidnaMirrorActor::bLockDirectionOnSpawn).
 * 이와 별개로 패턴 시작 시 "개인 유도레이저" 거울 1개를 보스 오른쪽(GuidedHoverHeight만큼 떨어진 지점, 높이는
 * 보스 캡슐 중심 Z 그대로 = 보스 키의 절반)에 추가로 스폰한다 — 스폰 직후부터 플레이어 위치를 계속
 * 따라다니며(AEchidnaMirrorActor::bSkyGuidedMode) 위에서 아래로 비스듬히 레이저를 반복 발사하다가,
 * 8거울 두 파동이 모두 끝나면 StopRepeating()으로 멈춰 마지막 한 발만 더 쏘고 소멸한다.
 * 유도 거울까지 완전히 끝나면(IsFinished) Succeeded.
 */
USTRUCT(meta = (DisplayName = "Echidna Eight Mirror Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaEightMirrorPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaEightMirrorPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	FRotator ComputeAimRotation(const AEchidnaBoss* Boss) const;
	void SpawnSpokeGroup(FInstanceDataType& InstanceData, const float (&AnglesDeg)[4], TArray<TObjectPtr<AEchidnaMirrorActor>>& OutMirrors, bool bActivateNow) const;
	void SpawnGuidedMirror(FInstanceDataType& InstanceData) const;
	bool AreMirrorsFinished(const TArray<TObjectPtr<AEchidnaMirrorActor>>& Mirrors) const;
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

	// 정면(패턴 시작 시점의 플레이어 방향 — 이후 재조준 안 함) 기준 좌/우로 얼마나 비스듬히 쏠지 (도) —
	// 1번은 -값(왼쪽), 2번은 +값(오른쪽)으로 적용. FanZoneClass의 FanAngle 절반보다 작아야 가운데가 겹침
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

	// 고리 1개당 1회만 판정 — 플레이어 최대체력(10만)의 12%
	UPROPERTY(EditAnywhere, Category = "Fan")
	float Damage = 12000.f;

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

	// 패턴 시작(EnterState) 시점에 한 번만 고정하는 발판 위치(지면 높이 보정 완료) — 1번/2번 장판 모두 이 위치에서
	// 스폰. 매번 Boss->GetActorLocation()을 쓰면 1번 발동 순간 보스가 후방으로 홉하면서 2번은 완전히 다른
	// 지점에서 스폰돼(두 부채꼴이 서로 다른 원점을 가짐) 각도가 심하게 어긋나 보이는 문제가 있었음
	UPROPERTY(Transient)
	FVector BaseSpawnLocation = FVector::ZeroVector;
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

UENUM()
enum class EEchidnaDragFanPhase : uint8
{
	Tethering,	// 줄기를 뻗어 당기는 중 (판정 대상이 있으면 보스 쪽으로 끌려옴)
	Casting1,	// 1번째 장판(왼쪽으로 비스듬히) 진행 중
	Casting2,	// 2번째 장판(오른쪽으로 비스듬히) 진행 중
	Done
};

/**
 * FStateTreeTask_EchidnaDragFanPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaDragFanPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동(패트롤 등)을 멈추는 용도로만 사용
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, Category = "Fan")
	TSubclassOf<AEchidnaFanZoneActor> FanZoneClass;

	// 1번째 장판(먼저 터짐) 부채꼴 전체 각도 (도) — FanZoneClass의 FanAngle 기본값을 덮어씀. 레퍼런스 기준 약 120도
	UPROPERTY(EditAnywhere, Category = "Fan")
	float FirstFanAngle = 120.f;

	// 1번째 장판의 중심 방향 — 정면(BaseAimRotation) 기준 오프셋 (도)
	UPROPERTY(EditAnywhere, Category = "Fan")
	float FirstYawOffset = -90.f;

	// 2번째 장판(나중에 터짐) 부채꼴 전체 각도 (도) — 1번보다 넓게. 레퍼런스 기준 약 180도
	UPROPERTY(EditAnywhere, Category = "Fan")
	float SecondFanAngle = 180.f;

	// 2번째 장판의 중심 방향 — 정면(BaseAimRotation) 기준 오프셋 (도). 1번과 살짝만 겹치도록
	// FirstYawOffset+FirstFanAngle/2 근처 값으로 잡을 것 (기본값 기준 겹침 폭 약 5도)
	UPROPERTY(EditAnywhere, Category = "Fan")
	float SecondYawOffset = 55.f;

	// 끌려간 뒤 맞는 구조라 회피 여지가 적음 — 플레이어 최대체력(10만)의 15%
	UPROPERTY(EditAnywhere, Category = "Fan")
	float Damage = 15000.f;

	// 1단계 "줄기" — 보스 정면 기준으로 부채꼴 형태로 동시에 뻗어 나가 맞은 대상을 보스 쪽으로 끌어당김
	UPROPERTY(EditAnywhere, Category = "Tether")
	TSubclassOf<AEchidnaTetherActor> TetherClass;

	// 동시에 뻗는 줄기 개수 (레퍼런스: 7개)
	UPROPERTY(EditAnywhere, Category = "Tether", meta = (ClampMin = "1"))
	int32 TetherCount = 7;

	// 줄기들이 정면(BaseAimRotation) 기준으로 좌우 합쳐서 덮는 총 각도 (도) — 레퍼런스 사진 기준 약 150도
	UPROPERTY(EditAnywhere, Category = "Tether", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float TetherFanAngle = 150.f;

	// 당겨지는 힘 (수평, cm/s)
	UPROPERTY(EditAnywhere, Category = "Tether")
	float PullStrength = 1500.f;

	UPROPERTY(Transient)
	EEchidnaDragFanPhase Phase = EEchidnaDragFanPhase::Tethering;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaFanZoneActor> CurrentFan;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaTetherActor>> SpawnedTethers;

	// 패턴 시작(EnterState) 시점에 한 번만 계산해서 고정하는 기준 조준 방향 — 줄기/1번/2번 캐스팅 모두 이 값 기준으로만 씀
	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;
};

/**
 * "끌고간후 장판터지는" 짤패턴 — 1단계로 보스 정면 기준 부채꼴 모양으로 줄기(촉수) TetherCount개를 동시에 뻗어
 * (레퍼런스: 7개, TetherFanAngle 범위 안에 균등 분포 — 나머지 각도는 안전지대) SnapDelay 뒤 맞은 대상을 보스 쪽으로
 * 끌어당긴다. 줄기에 맞은 대상이 단 한 명도 없으면(AnyTetherHit false) 2단계 없이 바로 패턴이 끝난다 — 아무도
 * 끌려오지 않았는데 장판이 터지는 것을 막기 위함.
 * 누군가 끌려왔다면 2단계로 FirstFanAngle(기본 120도) 부채꼴(1)을 먼저 터뜨리고 이어서 SecondFanAngle(기본 180도,
 * 1보다 넓음) 부채꼴(2)을 터뜨린다. First/SecondYawOffset을 조절해 두 부채꼴이 살짝만 겹치게 배치 —
 * 겹치는 구간은 1·2 둘 다 맞아 매혹 게이지가 2스택 쌓인다(레퍼런스의 "빨간 원 2스택 주의").
 * 넉백 없이 매혹 게이지만 쌓이도록 FanZoneClass는 bApplyKnockdownOnHit=false / bApplyCharmGaugeOnHit=true로
 * 설정한 별도 BP 서브클래스 사용 권장 (뒤로 빠지며 좌우장판에 쓰는 BP_EchidnaFanZone과는 다른 설정).
 * 장판 2까지 다 끝나면(IsFinished) Succeeded.
 */
USTRUCT(meta = (DisplayName = "Echidna Drag Fan Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaDragFanPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaDragFanPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	FRotator ComputeAimRotation(const AEchidnaBoss* Boss) const;
	AEchidnaFanZoneActor* SpawnFan(FInstanceDataType& InstanceData, float YawOffsetDeg, float FanAngleOverride) const;
	void SpawnTethers(FInstanceDataType& InstanceData) const;
	bool AreTethersFinished(const FInstanceDataType& InstanceData) const;
	bool AnyTetherHit(const FInstanceDataType& InstanceData) const;
};

UENUM()
enum class EEchidnaDonutSlashPhase : uint8
{
	Slash1,				// 1번 슬래시(우측 대각) 진행 중
	Slash2,				// 2번 슬래시(좌측 대각) 진행 중
	InnerDonutTelegraph,	// 작은 도넛(3번) 예고 표시 중 — 보스는 아직 지상에 그대로 있음
	Rising,				// 작은 도넛(3번)이 터진 직후부터 보스가 하늘로 상승, 정점에서 대기
	FallAndOuterDonut,	// 보스가 하강하며 외곽 도넛(4번) 예고→폭발
	Done
};

/**
 * FStateTreeTask_EchidnaDonutSlashPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaDonutSlashPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동(패트롤 등)을 멈추는 용도로만 사용
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	// 1번/2번 슬래시에 쓰는 클래스 — 넉다운 대신 가벼운 경직(bApplyStaggerOnHit=true) / 매혹 X 설정 권장.
	// 슬래시에 맞아도 캐릭터가 튕겨나가거나 쓰러지지 않고 짧게만 행동불능이 되도록 넉다운과는 다른 BP를 씀
	UPROPERTY(EditAnywhere, Category = "Fan")
	TSubclassOf<AEchidnaFanZoneActor> SlashZoneClass;

	// 3번(작은 도넛)에 쓰는 클래스 — 넉다운 O(bApplyKnockdownOnHit=true) / 매혹 X 설정 권장
	UPROPERTY(EditAnywhere, Category = "Fan")
	TSubclassOf<AEchidnaFanZoneActor> FanZoneClass;

	// 4번(외곽 도넛)에 쓰는 클래스 — 넉다운 O + 매혹 O(bApplyKnockdownOnHit=true, bApplyCharmGaugeOnHit=true 둘 다)
	// 설정 권장 — "1~3은 매혹 X, 4번만 매혹 O"이면서 "3·4번 다 넘어짐"이므로 3번용(FanZoneClass)과는
	// 매혹 여부만 다른 별도 BP 서브클래스 필요
	UPROPERTY(EditAnywhere, Category = "Fan")
	TSubclassOf<AEchidnaFanZoneActor> OuterDonutClass;

	// 슬래시(1·2번) 부채꼴 전체 각도 (도) — 180도라 각 슬래시가 정면 기준 절반(90도)씩 좌우로 크게 덮음.
	// Slash1/2YawOffset과 합쳐서 "정면보다 살짝 반대편부터 반대쪽 뒷편까지" 넓게 훑고 정면 부근에서 겹치게 함
	UPROPERTY(EditAnywhere, Category = "Slash")
	float SlashFanAngle = 180.f;

	// 슬래시 안쪽 반지름 (cm) — 레퍼런스처럼 중심이 아니라 호(arc) 끝부분만 때리도록 SlashRange에 가깝게 잡아
	// 얇은 부채꼴 고리(annulus)로 만듦. 0으로 두면 중심에서 뻗는 일반 부채꼴로 되돌아감
	UPROPERTY(EditAnywhere, Category = "Slash")
	float SlashInnerRadius = 350.f;

	// 슬래시 바깥 반지름 = 사거리 (cm)
	UPROPERTY(EditAnywhere, Category = "Slash")
	float SlashRange = 850.f;

	// 슬래시(1·2번) 전용 예고시간 오버라이드 (초) — 예고 후 한 번만 딱 때리도록(RingCount=1 강제) SpawnZone에서 씀
	UPROPERTY(EditAnywhere, Category = "Slash")
	float SlashTelegraphDuration = 1.2f;

	// 1번(우측 대각) — 정면(BaseAimRotation) 기준 부채꼴 중심의 Yaw 오프셋 (도, +값=오른쪽).
	// SlashFanAngle=180 기준 범위는 [YawOffset-90, YawOffset+90] — 기본값 75도면 정면 -15도(살짝 왼쪽)부터
	// 우측 뒤(165도)까지 덮어서 "정면보다 살짝 왼쪽부터 우측 뒷편까지" 레퍼런스 그림과 맞음
	UPROPERTY(EditAnywhere, Category = "Slash")
	float Slash1YawOffset = 75.f;

	// 2번(좌측 대각) — 1번과 좌우 대칭 (도, -값=왼쪽). 기본값 -75도면 정면 +15도(살짝 오른쪽)부터
	// 좌측 뒤(-165도)까지 덮음 — 1번의 [-15,165]와 겹쳐서 정면 [-15,15] 구간(30도)은 둘 다 맞아 색이 진해짐
	UPROPERTY(EditAnywhere, Category = "Slash")
	float Slash2YawOffset = -75.f;

	// 작은 도넛(3번) — 안쪽 구멍 반지름 (cm)
	UPROPERTY(EditAnywhere, Category = "Donut")
	float InnerDonutInnerRadius = 0.f;

	// 작은 도넛(3번) — 바깥 반지름 (cm)
	UPROPERTY(EditAnywhere, Category = "Donut")
	float InnerDonutOuterRadius = 450.f;

	// 작은 도넛(3번) 전용 예고시간 오버라이드 (초) — FanZoneClass의 기본 TelegraphDuration을 무시하고 이 값을 씀.
	// "올라가면서 터져야" 하므로 RiseDuration보다 확실히 짧게 잡아 상승 도중에 다 터지도록 강제함
	UPROPERTY(EditAnywhere, Category = "Donut")
	float InnerDonutTelegraphDuration = 0.5f;

	// 외곽 도넛(4번) — 안쪽 반지름 (cm) — OuterDonutOuterRadius에 가깝게 잡아 "원테두리만" 터지는 얇은 고리로 구성
	UPROPERTY(EditAnywhere, Category = "Donut")
	float OuterDonutInnerRadius = 750.f;

	// 외곽 도넛(4번) — 바깥 반지름 (cm)
	UPROPERTY(EditAnywhere, Category = "Donut")
	float OuterDonutOuterRadius = 1000.f;

	// 외곽 도넛(4번) 전용 예고시간 오버라이드 (초) — OuterDonutClass의 기본 TelegraphDuration을 무시하고 이 값을 씀.
	// 하강(FallDuration)과 동시에 예고가 뜨므로 착지 타이밍과 맞추려면 FallDuration과 비슷하게 잡을 것
	UPROPERTY(EditAnywhere, Category = "Donut")
	float OuterDonutTelegraphDuration = 0.9f;

	// 보스가 하늘로 상승하는 높이 (cm)
	UPROPERTY(EditAnywhere, Category = "Rise")
	float RiseHeight = 500.f;

	// 상승 시간 (초) — 작은 도넛(3번) 예고→폭발이 다 끝난 뒤(InnerDonutTelegraph 단계 종료)부터 세는 시간
	UPROPERTY(EditAnywhere, Category = "Rise")
	float RiseDuration = 1.0f;

	// 정점 도달 후 대기 시간 (초) — 작은 도넛(3번)은 이미 상승 시작 전에 예고→폭발이 끝난 상태이므로,
	// 이 구간은 순수하게 "정점에서 잠깐 멈춰있는" 연출용 대기 시간
	UPROPERTY(EditAnywhere, Category = "Rise")
	float ApexHoldDuration = 0.7f;

	// 하강 시간 (초) — 이 구간 시작과 동시에 외곽 도넛(4번) 예고가 뜸. 착지 무렵 터지도록 OuterDonutTelegraphDuration을
	// 이 값과 비슷하게 맞춰두면 좋음
	UPROPERTY(EditAnywhere, Category = "Rise")
	float FallDuration = 0.9f;

	// 슬래시(1·2번)·도넛(3·4번) 공용 — 1~2번은 경직, 3~4번은 넉다운이라 체감은 뒤쪽이 더 아픔
	UPROPERTY(EditAnywhere, Category = "Fan")
	float Damage = 15000.f;

	UPROPERTY(Transient)
	EEchidnaDonutSlashPhase Phase = EEchidnaDonutSlashPhase::Slash1;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaFanZoneActor> CurrentFan;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaFanZoneActor> InnerDonut;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaFanZoneActor> OuterDonut;

	// 패턴 시작(EnterState) 시점에 한 번만 고정하는 기준 조준 방향 — 1번/2번 슬래시 모두 이 값 + YawOffset만 씀
	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;

	// 패턴 시작 시점에 고정하는 발판 위치(지면 높이 보정 완료) — 슬래시·도넛 모두 이 위치 기준으로 스폰
	UPROPERTY(Transient)
	FVector BaseSpawnLocation = FVector::ZeroVector;

	// 상승 시작 직전 보스의 실제 액터 Z(캡슐 중심) — 상승/하강 높이 계산 및 하강 완료 후 복귀 기준
	UPROPERTY(Transient)
	float GroundActorZ = 0.f;

	// Rising/FallAndOuterDonut 단계 진입 후 경과 시간 (단계 진입 시 0으로 리셋)
	UPROPERTY(Transient)
	float PhaseElapsed = 0.f;
};

/**
 * "두번긋고 도넛장판" 짤패턴 — 보스 정면 기준 우측 대각(1)·좌측 대각(2) 부채꼴 슬래시를 순서대로 터뜨린 뒤,
 * 작은 도넛(3, 중심 근처 고리)이 예고→폭발까지 끝나야(보스는 아직 지상) 그 직후부터 보스가 하늘로 상승해
 * 잠시 에어본 상태로 대기하다가, 하강하면서 외곽 도넛(4, 사거리 끝의 얇은 고리 — "원테두리만")이 예고→폭발한다.
 * **순서 주의**: "예고 표시 → 폭발 → (그제서야) 보스 상승"이어야 하며, 예고를 보여주면서 동시에 이미 공중에
 * 떠서 뜬 채로 터지는 것처럼 보이면 안 됨(레퍼런스 요구사항) — 그래서 InnerDonutTelegraph 단계를 Rising 단계와
 * 분리해 InnerDonut이 `IsFinished()`(예고+단발판정 완료)가 된 다음에만 MOVE_Flying 전환/상승을 시작한다.
 * 1~3번은 넉다운(FanZoneClass, bApplyKnockdownOnHit=true 권장)만 적용되고 매혹 게이지는 쌓이지 않으며,
 * 4번(OuterDonutClass, bApplyCharmGaugeOnHit=true 권장)만 매혹 게이지가 쌓인다 — 실제 판정 효과는
 * AEchidnaFanZoneActor 서브클래스(BP)의 bApplyKnockdownOnHit/bApplyCharmGaugeOnHit 설정을 그대로 따르므로
 * FanZoneClass/OuterDonutClass에 서로 다른 BP를 할당해야 함.
 * 3·4번(도넛)은 AEchidnaFanZoneActor의 FanAngle=360 오버라이드로 표현(부채꼴이 아니라 원형 고리가 됨) —
 * FanInnerRadius/FanRange도 함께 오버라이드해서 고리 두께를 조절한다.
 * **1~4번 전부 예고 후 한 번만 딱 때리는 단발 판정** — "뒤로 빠지며 좌우장판"이 쓰는 RingCount 계단식 확장(안→밖으로
 * 훑으며 여러 번 판정)은 이 패턴에는 안 맞아서, SpawnZone()이 4번 스폰 전부에 RingCount=1을 강제 오버라이드한다
 * (BP 기본 RingCount가 5여도 무시됨). 예고시간은 Slash1/2YawOffset 옆의 SlashTelegraphDuration, 작은 도넛의
 * InnerDonutTelegraphDuration, 외곽 도넛의 OuterDonutTelegraphDuration으로 각각 따로 조절한다.
 * 보스의 상승/하강은 물리(LaunchCharacter)가 아니라 CharacterMovementComponent를 MOVE_Flying으로 바꾼 뒤
 * 매 틱 Z를 직접 보간하는 방식으로 처리 — 넉백 패턴들과 달리 정점 대기·착지 타이밍이 중요해서 물리 예측에
 * 맡기지 않고 스크립트로 직접 제어한다(EndRiseFall에서 MOVE_Walking으로 복구 + Z를 원래 지상 높이로 스냅).
 * 4번까지 다 끝나면(OuterDonut->IsFinished()) Succeeded.
 */
USTRUCT(meta = (DisplayName = "Echidna Donut Slash Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaDonutSlashPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaDonutSlashPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	FRotator ComputeAimRotation(const AEchidnaBoss* Boss) const;
	// TelegraphDurationOverride가 음수면 무시(BP 기본값 유지) — RingCountOverride는 이 Task의 모든 호출부에서
	// 항상 1을 넘겨 "예고 후 단발 판정"을 강제한다 (BP 기본 RingCount가 몇이든 무시됨)
	AEchidnaFanZoneActor* SpawnZone(FInstanceDataType& InstanceData, TSubclassOf<AEchidnaFanZoneActor> ZoneClass,
		float YawOffsetDeg, float FanAngleOverride, float InnerRadiusOverride, float OuterRadiusOverride,
		float TelegraphDurationOverride, int32 RingCountOverride) const;
	void EndRiseFall(FInstanceDataType& InstanceData) const;
};

UENUM()
enum class EEchidnaHeartBurstPhase : uint8
{
	Telegraph,	// 보스 머리 위 하트 표시, 보스는 가만히 있음
	Firing,		// FireDuration 동안 반복적으로 랜덤 방향·랜덤 개수 하트 발사
	Done
};

/**
 * FStateTreeTask_EchidnaHeartBurstPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaHeartBurstPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동(패트롤 등)을 멈추는 용도로만 사용 — 이 패턴은 보스가 계속 제자리에 있으므로 이동 명령엔 안 씀
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, Category = "Heart")
	TSubclassOf<AEchidnaHeartActor> HeartClass;

	// 예고(보스 머리 위 하트 표시) 지속시간 (초) — 이 동안 보스는 가만히 있고 하트는 아직 발사되지 않음
	UPROPERTY(EditAnywhere, Category = "Heart")
	float TelegraphDuration = 2.f;

	// 하트 발사가 지속되는 총 시간 (초) — 이 시간이 지나면 아직 날아가는 하트가 있어도 패턴 자체는 종료됨
	UPROPERTY(EditAnywhere, Category = "Heart")
	float FireDuration = 2.f;

	// 발사 웨이브 간격 (초) — 이 간격마다 랜덤 방향으로 하트 무리를 한 번씩 쏨
	UPROPERTY(EditAnywhere, Category = "Heart")
	float FireInterval = 0.3f;

	// 한 웨이브에 몇 개를 쏠지 — 각도는 정해진 방향 없이 0~360도 전방향 중 매번 완전히 랜덤(FRandRange)으로
	// 정해지므로 개수만 이 범위에서 랜덤으로 뽑음
	UPROPERTY(EditAnywhere, Category = "Heart", meta = (ClampMin = "1"))
	int32 MinHeartsPerWave = 2;

	UPROPERTY(EditAnywhere, Category = "Heart", meta = (ClampMin = "1"))
	int32 MaxHeartsPerWave = 6;

	// 하트 비행 속도 (cm/s)
	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartSpeed = 800.f;

	// 실제로 발사되는 하트의 높이 — 지면(보스 발밑) 기준 오프셋 (cm). 예고 마커는 보스 머리 위에 표시되지만,
	// 발사되는 하트까지 그 높이로 날아가면 플레이어 캡슐(중심이 대략 지면+96cm) 위를 그냥 지나쳐서 안 맞았음 —
	// 플레이어 캡슐 중심 높이와 비슷하게 잡아서 실제로 충돌하도록 함
	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartFireHeight = 100.f;

	// 3초 기절이 같이 걸려 후속 피격으로 이어지므로 데미지 자체는 낮게 — 최대체력(10만)의 10%
	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartDamage = 10000.f;

	// 하트에 맞으면 걸리는 기절 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartStunDuration = 3.f;

	// 하트에 맞으면 쌓이는 매혹 게이지
	UPROPERTY(EditAnywhere, Category = "Heart")
	int32 HeartCharmGaugeAmount = 1;

	UPROPERTY(Transient)
	EEchidnaHeartBurstPhase Phase = EEchidnaHeartBurstPhase::Telegraph;

	// 예고 단계에서 Launch 없이 스폰해두는 하트 — 보스 머리 위 마커로 재사용 (예고 끝나면 소멸)
	UPROPERTY(Transient)
	TObjectPtr<AEchidnaHeartActor> TelegraphHeart;

	// 예고 마커가 표시되는 위치(보스 머리 위 높이로 보정 완료) — 패턴 시작 시점에 한 번만 고정
	UPROPERTY(Transient)
	FVector SpawnLocation = FVector::ZeroVector;

	// 실제 발사되는 하트들의 원점(HeartFireHeight로 보정 — 플레이어 캡슐과 충돌 가능한 높이) — 패턴 시작 시점에 한 번만 고정
	UPROPERTY(Transient)
	FVector FireSpawnLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float PhaseElapsed = 0.f;

	// Firing 단계에서 다음 웨이브까지 남은 경과시간 누적 (FireInterval마다 리셋)
	UPROPERTY(Transient)
	float FireIntervalElapsed = 0.f;
};

/**
 * "전방향 하트발사" 짤패턴(단순화 버전 — 레퍼런스의 "랜덤 1명 잡기+씨앗" 무력화는 제외, 스턴+매혹만 적용) —
 * 패턴 시작 시 보스 머리 위에 하트 마커(AEchidnaHeartActor를 Launch 없이 스폰해 예고용으로 재사용)를
 * TelegraphDuration(기본 2초) 동안 표시하고, 그동안 보스는 제자리에 가만히 있는다(별도 이동/회전 명령 없음).
 * 예고가 끝나면 FireDuration(기본 2초) 동안 FireInterval(기본 0.3초)마다 [MinHeartsPerWave, MaxHeartsPerWave]
 * 범위에서 매번 랜덤으로 고른 개수만큼 하트를 쏘는데, 각 하트의 방향은 정해진 8방향이 아니라 0~360도
 * 전방향 중 완전히 랜덤(FMath::FRandRange)으로 정해진다 — 매 웨이브마다 개수도 방향도 전부 달라짐.
 * 하트에 맞은 플레이어는 데미지 +
 * HeartStunDuration(기본 3초) 동안 완전 기절(ALoACharacter::ApplyStun — 넉다운과 달리 서 있는 채로
 * 얼어붙어 이동/스킬 입력이 전부 차단됨) + HeartCharmGaugeAmount(기본 1) 매혹 게이지가 적용된다.
 * FireDuration이 다 지나면(개별 하트가 아직 날아가고 있어도) 곧바로 Succeeded — 발사된 하트들은 각자
 * 알아서 맞거나 MaxRange에서 소멸한다(패턴 State 종료와 하트 생존은 서로 독립적).
 */
USTRUCT(meta = (DisplayName = "Echidna Heart Burst Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaHeartBurstPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaHeartBurstPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	void FireRandomWave(FInstanceDataType& InstanceData) const;
};

UENUM()
enum class EEchidnaBackstepHeartPhase : uint8
{
	Backstep,	// 뒤로 튕겨나가는 중 — 그동안 보스는 계속 플레이어를 바라본다
	Fire,		// 하트 4개를 부채꼴로 발사한 뒤 짧게 대기
	Done
};

/**
 * FStateTreeTask_EchidnaBackstepHeartPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaBackstepHeartPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동(패트롤 등)을 멈추는 용도로만 사용 — 백스텝은 LaunchCharacter라 이동 명령엔 안 씀
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, Category = "Heart")
	TSubclassOf<AEchidnaHeartActor> HeartClass;

	// 부채꼴로 쏠 하트 개수 (레퍼런스: 4개)
	UPROPERTY(EditAnywhere, Category = "Heart", meta = (ClampMin = "1"))
	int32 HeartCount = 4;

	// 부채꼴 전체 벌어짐 각도 (도) — 하트들이 이 범위에 균등 분포한다. 개수와 무관하게 양 끝은 항상 ±절반
	UPROPERTY(EditAnywhere, Category = "Heart")
	float FanSpreadAngle = 60.f;

	// 뒤로 튕겨나가는 힘 (cm/s) — 플레이어 반대 방향
	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepStrength = 900.f;

	// 같은 순간 위로 띄우는 힘 (cm/s) — 값이 있어야 점프하듯 포물선을 그림
	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepUpwardStrength = 350.f;

	// 착지 예상 지점(뒤로 이 거리만큼)에 바닥이 있는지 미리 검사 — 없으면 백스텝을 건너뛴다(낙사 방지)
	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepCheckDistance = 450.f;

	// 백스텝 후 하트를 쏘기까지의 시간 (초) — 착지할 때쯤 발사되도록 맞춰둘 것
	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepDuration = 0.6f;

	// 발사 후 State가 끝나기까지의 여유 시간 (초) — 발사 모션이 끊겨 보이지 않게 하는 용도
	UPROPERTY(EditAnywhere, Category = "Heart")
	float FireLingerDuration = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartSpeed = 900.f;

	// 발사 높이 — 지면(보스 발밑) 기준 오프셋 (cm). 플레이어 캡슐 중심 높이와 비슷해야 실제로 맞는다
	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartFireHeight = 100.f;

	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartDamage = 10000.f;

	// 레퍼런스의 "상태이상" — 이 프로젝트엔 정화(해제) 시스템이 없어 기존 기절로 대체
	UPROPERTY(EditAnywhere, Category = "Heart")
	float HeartStunDuration = 3.f;

	UPROPERTY(EditAnywhere, Category = "Heart")
	int32 HeartCharmGaugeAmount = 1;

	UPROPERTY(Transient)
	EEchidnaBackstepHeartPhase Phase = EEchidnaBackstepHeartPhase::Backstep;

	// 발사 직전에 고정되는 조준 방향 — 이 방향을 중심으로 부채꼴이 펼쳐진다
	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float PhaseElapsed = 0.f;
};

/**
 * "백스탭 후 하트발사" 짤패턴 (레퍼런스 8번) —
 * 보스가 플레이어를 바라본 채 뒤로 크게 튕겨나가고(LaunchCharacter 기반 백스텝), 착지할 즈음
 * 정면으로 하트 HeartCount(기본 4)개를 FanSpreadAngle(기본 60도) 범위에 균등하게 부채꼴로 발사한다.
 *
 * - **백스텝 중에는 매 틱 플레이어를 다시 바라본다.** 뒤로 밀려나는 동안에도 정면이 플레이어를 향해야
 *   레퍼런스 그림처럼 "물러나면서 앞으로 쏘는" 모양이 나온다. 발사 직전에 그 시점의 조준 방향을
 *   BaseAimRotation으로 고정하고, 4개 하트는 전부 이 기준선 ± 오프셋으로만 나간다
 *   (RetreatFan처럼 발사 순간 이후로는 다시 조준하지 않음)
 * - 하트는 "전방향 하트발사"와 같은 AEchidnaHeartActor를 그대로 재사용한다 — 맞으면 데미지 +
 *   HeartStunDuration 기절 + HeartCharmGaugeAmount 매혹 게이지. 레퍼런스의 "상태이상은 정화로 해제가능"은
 *   이 프로젝트에 정화 시스템이 없어 기존 기절로 대체했다
 * - 착지 예상 지점에 바닥이 없으면 백스텝을 건너뛰고 제자리에서 발사만 한다(낙사 방지 — RetreatFan과 동일)
 * - FireLingerDuration이 지나면 (하트가 아직 날아가고 있어도) Succeeded — 하트는 각자 맞거나 소멸한다
 */
USTRUCT(meta = (DisplayName = "Echidna Backstep Heart Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaBackstepHeartPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaBackstepHeartPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	/** 플레이어 방향(없으면 보스 현재 회전)을 구해 보스를 그쪽으로 즉시 회전시키고 그 Rotation을 반환 */
	FRotator FacePlayer(AEchidnaBoss* Boss) const;

	/** 플레이어 반대 방향으로 LaunchCharacter — 착지 지점에 바닥이 없으면 아무것도 하지 않는다 */
	void Backstep(FInstanceDataType& InstanceData) const;

	/** BaseAimRotation 기준 부채꼴로 하트를 한 번에 전부 발사 */
	void FireFan(FInstanceDataType& InstanceData) const;

	bool HasGroundBelow(const FInstanceDataType& InstanceData, const FVector& Location) const;
};

UENUM()
enum class EEchidnaReturningOrbPhase : uint8
{
	Throw1,		// 플레이어 방향으로 1번째 구체
	Throw2,		// 플레이어 방향으로 2번째 구체
	Backstep,	// 뒤로 물러나는 중
	WaitReturn,	// 부채꼴 4개까지 던지고, 구체들이 전부 돌아올 때까지 대기
	Done
};

/**
 * FStateTreeTask_EchidnaReturningOrbPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaReturningOrbPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동을 멈추는 용도로만 사용 — 백스텝은 LaunchCharacter라 이동 명령엔 안 씀
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, Category = "Orb")
	TSubclassOf<AEchidnaOrbActor> OrbClass;

	// 1번째 → 2번째 구체 사이 간격 (초). 각 발사 시점에 플레이어를 다시 조준한다
	UPROPERTY(EditAnywhere, Category = "Orb")
	float ThrowInterval = 1.2f;

	// 백스텝 후 부채꼴 발사까지의 시간 (초) — 착지할 즈음 쏘도록 맞춰둘 것
	UPROPERTY(EditAnywhere, Category = "Orb")
	float BackstepDuration = 0.7f;

	// 부채꼴로 쏠 구체 개수 (레퍼런스: 4개 — 앞선 2개까지 합쳐 총 6개)
	UPROPERTY(EditAnywhere, Category = "Orb", meta = (ClampMin = "1"))
	int32 FanOrbCount = 4;

	// 부채꼴 전체 벌어짐 각도 (도)
	UPROPERTY(EditAnywhere, Category = "Orb")
	float FanSpreadAngle = 70.f;

	UPROPERTY(EditAnywhere, Category = "Orb")
	float OrbDamage = 15000.f;

	// 발사 높이 — 지면(보스 발밑) 기준 오프셋 (cm). 플레이어 캡슐 중심과 비슷해야 실제로 맞는다
	UPROPERTY(EditAnywhere, Category = "Orb")
	float OrbSpawnHeight = 100.f;

	// 구체가 전부 돌아오길 기다리는 최대 시간 (초) — 하나가 지형에 끼어도 패턴이 안 끝나는 일이 없도록
	UPROPERTY(EditAnywhere, Category = "Orb")
	float MaxWaitDuration = 10.f;

	// ── 아래 4개는 BP_EchidnaOrb 기본값을 덮어쓰는 값. **음수면 BP 값을 그대로 쓴다.**
	//    BP를 새로 파지 않고 StateTree에서 바로 크기/속도를 조절하기 위한 것 —
	//    AEchidnaOrbActor::ApplyOverrides()가 스폰 직후 적용한다

	// 구체 판정 반지름 (cm)
	UPROPERTY(EditAnywhere, Category = "Orb|Override")
	float OrbCollisionRadiusOverride = -1.f;

	// 구체 시각적 크기 배율 (엔진 기본 Sphere 반지름 50cm 기준 — 2.0이면 지름 200cm)
	UPROPERTY(EditAnywhere, Category = "Orb|Override")
	float OrbVisualScaleOverride = -1.f;

	// 비행 속도 (cm/s)
	UPROPERTY(EditAnywhere, Category = "Orb|Override")
	float OrbSpeedOverride = -1.f;

	// 이 거리까지 나간 뒤 되돌아온다 (cm)
	UPROPERTY(EditAnywhere, Category = "Orb|Override")
	float OrbMaxRangeOverride = -1.f;

	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepStrength = 900.f;

	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepUpwardStrength = 350.f;

	// 착지 예상 지점에 바닥이 있는지 미리 검사 — 없으면 백스텝을 건너뛴다(낙사 방지)
	UPROPERTY(EditAnywhere, Category = "Backstep")
	float BackstepCheckDistance = 450.f;

	UPROPERTY(Transient)
	EEchidnaReturningOrbPhase Phase = EEchidnaReturningOrbPhase::Throw1;

	// 부채꼴 발사 직전에 고정되는 조준 방향 — 4개 구체가 이 기준선 ± 오프셋으로 나간다
	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float PhaseElapsed = 0.f;

	// 전부 돌아왔는지 폴링하기 위해 들고 있는 목록 (총 6개)
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaOrbActor>> SpawnedOrbs;
};

/**
 * "되돌아오는 구체(자야패턴)" 짤패턴 (레퍼런스 10번) —
 * 보스가 큰 구체를 전방으로 던지는데, **한 번 나간 구체는 그 경로를 그대로 되짚어 돌아온다**
 * (`AEchidnaOrbActor`). 나갈 때 피했어도 돌아올 때 다시 맞을 수 있다.
 *
 * 순서: 플레이어 조준 후 1번째 → ThrowInterval 뒤 **플레이어를 다시 조준해서** 2번째 →
 * 백스텝(플레이어 반대 방향으로 LaunchCharacter) → 착지할 즈음 정면으로 부채꼴 FanOrbCount(기본 4)개 →
 * 6개가 전부 돌아오면(각 구체의 IsFinished) Succeeded.
 *
 * - **앞의 2개는 발사 시점마다 다시 조준**한다(플레이어를 쫓아감). 반면 **부채꼴 4개는 발사 직전에
 *   방향을 한 번 고정**해서 서로의 각도 간격이 항상 일정하게 유지된다 — RetreatFan/BackstepHeart와 같은 방침
 * - 착지 예상 지점에 바닥이 없으면 백스텝을 건너뛰고 제자리에서 쏜다(낙사 방지)
 * - `MaxWaitDuration`이 지나면 아직 안 돌아온 구체가 있어도 패턴을 끝낸다(지형에 끼는 경우 대비)
 */
USTRUCT(meta = (DisplayName = "Echidna Returning Orb Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaReturningOrbPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaReturningOrbPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	/** 플레이어 방향으로 보스를 즉시 회전시키고 그 Rotation을 반환 */
	FRotator FacePlayer(AEchidnaBoss* Boss) const;

	/** BaseYaw 방향으로 구체 1개 발사 — SpawnedOrbs에 등록된다 */
	void ThrowOrb(FInstanceDataType& InstanceData, float YawDeg) const;

	/** 부채꼴로 FanOrbCount개를 한 번에 발사 */
	void ThrowFan(FInstanceDataType& InstanceData) const;

	void Backstep(FInstanceDataType& InstanceData) const;
	bool HasGroundBelow(const FInstanceDataType& InstanceData, const FVector& Location) const;
	bool AreOrbsFinished(const FInstanceDataType& InstanceData) const;
};

UENUM()
enum class EEchidnaRibbonPhase : uint8
{
	Ribbon1,	// 1차 리본 2갈래 — 판정이 끝날 때까지 대기
	MoveToEnd,	// 1차가 빗나감 → 리본이 뻗었던 끝자락까지 이동
	Ribbon2,	// 2차 리본 2갈래 (그 자리에서 플레이어를 다시 조준)
	CircleZone,	// 리본에 맞음 → 보스 중심 원형 장판(넉다운)
	ArcSlash,	// 둘 다 빗나감 → 좌측 전방 호 내려치기
	Done
};

/**
 * FStateTreeTask_EchidnaRibbonPattern의 Instance Data
 */
USTRUCT()
struct FStateTreeEchidnaRibbonPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AEchidnaBoss> Boss;

	// 패턴 시작 시 잔여 이동을 멈추는 용도 — 리본 끝자락 이동은 NavMesh가 아니라 직접 보간이라 여기 안 씀
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController;

	/** 리본(줄기) — `AEchidnaTetherActor`를 재사용한다. **끌기 없이 기절+매혹만** 주도록
	 *  `bApplyPullOnHit=false` / `bApplyStunOnHit=true` / `CharmGaugeAmount=1`로 설정한 BP를 넣을 것
	 *  (끌고간후 장판터지는에 쓰는 BP_EchidnaTether와는 다른 설정) */
	UPROPERTY(EditAnywhere, Category = "Ribbon")
	TSubclassOf<AEchidnaTetherActor> RibbonClass;

	// 한 번에 뻗는 리본 갈래 수 (레퍼런스: 2갈래)
	UPROPERTY(EditAnywhere, Category = "Ribbon", meta = (ClampMin = "1"))
	int32 RibbonCount = 2;

	// 갈래들이 벌어지는 총 각도 (도) — 2갈래면 정면 기준 ±절반
	UPROPERTY(EditAnywhere, Category = "Ribbon")
	float RibbonSpreadAngle = 16.f;

	// 리본 길이 (cm) — **음수면 BP(BP_EchidnaRibbon)의 TetherRange를 그대로 쓴다.**
	// 1차가 빗나갔을 때 이동하는 거리도 이 값을 따라간다(끝자락까지 가므로)
	UPROPERTY(EditAnywhere, Category = "Ribbon|Override")
	float RibbonRangeOverride = -1.f;

	// 리본 절반 폭 (cm) — 판정 박스와 표시 메시 양쪽에 적용. 음수면 BP 기본값 유지
	UPROPERTY(EditAnywhere, Category = "Ribbon|Override")
	float RibbonHalfWidthOverride = -1.f;

	// 1차가 빗나갔을 때 리본 끝자락까지 이동하는 시간 (초)
	UPROPERTY(EditAnywhere, Category = "Ribbon")
	float MoveDuration = 0.8f;

	/** 리본 피격 시 터지는 보스 중심 원형 장판 — `FanAngle=360`으로 덮어써 원형으로 쓴다.
	 *  넉다운이 걸려야 하므로 `bApplyKnockdownOnHit=true`인 BP를 넣을 것 */
	UPROPERTY(EditAnywhere, Category = "Circle")
	TSubclassOf<AEchidnaFanZoneActor> CircleZoneClass;

	UPROPERTY(EditAnywhere, Category = "Circle")
	float CircleRadius = 700.f;

	UPROPERTY(EditAnywhere, Category = "Circle")
	float CircleInnerRadius = 0.f;

	UPROPERTY(EditAnywhere, Category = "Circle")
	float CircleTelegraphDuration = 0.8f;

	/** 둘 다 빗나갔을 때 좌측 전방으로 내려치는 호 — 얇은 부채꼴 고리(annulus)로 만든다
	 *  (두번긋고 도넛장판의 슬래시와 같은 방식) */
	UPROPERTY(EditAnywhere, Category = "Arc")
	TSubclassOf<AEchidnaFanZoneActor> ArcZoneClass;

	// 정면 기준 호의 중심 방향 (도) — **음수가 좌측**
	UPROPERTY(EditAnywhere, Category = "Arc")
	float ArcYawOffset = -60.f;

	UPROPERTY(EditAnywhere, Category = "Arc")
	float ArcAngle = 110.f;

	// 안쪽 반지름 — 0보다 크면 중심이 아니라 호(arc) 끝부분만 타격하는 고리가 된다
	UPROPERTY(EditAnywhere, Category = "Arc")
	float ArcInnerRadius = 300.f;

	UPROPERTY(EditAnywhere, Category = "Arc")
	float ArcRange = 900.f;

	UPROPERTY(EditAnywhere, Category = "Arc")
	float ArcTelegraphDuration = 0.9f;

	// 원형/호 장판 공통 데미지
	UPROPERTY(EditAnywhere, Category = "Zone")
	float Damage = 15000.f;

	// 장판이 끝나길 기다리는 최대 시간 (초) — 안전장치
	UPROPERTY(EditAnywhere, Category = "Zone")
	float ZoneWaitTimeout = 6.f;

	UPROPERTY(Transient)
	EEchidnaRibbonPhase Phase = EEchidnaRibbonPhase::Ribbon1;

	UPROPERTY(Transient)
	FRotator BaseAimRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float PhaseElapsed = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaTetherActor>> SpawnedRibbons;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaFanZoneActor> CurrentZone;

	// 리본이 뻗었던 끝 지점 — 1차가 빗나가면 여기로 이동한다
	UPROPERTY(Transient)
	FVector RibbonEndLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector MoveStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector MoveTargetLocation = FVector::ZeroVector;
};

/**
 * "정면 리본 공격" 짤패턴 (레퍼런스 11번) —
 * 보스가 정면으로 긴 리본 2갈래를 뻗는다. 분기 구조가 핵심:
 *
 * - **맞았으면**: 3초 기절 + 매혹 1스택이 걸리고, 이어서 **보스 중심 원형 장판**이 터진다(넉다운). 패턴 종료
 * - **빗나갔으면**: 리본이 뻗었던 **끝자락까지 보스가 이동**한 뒤, 그 자리에서 플레이어를 다시 조준해 2차 리본
 * - **2차도 빗나갔으면**: 정면 기준 **좌측 전방으로 호(arc) 내려치기**
 *
 * 새 액터 클래스 없이 기존 두 개를 재사용한다 —
 * 리본은 `AEchidnaTetherActor`(끌기 대신 기절+매혹만 켠 BP), 원형 장판과 호는 `AEchidnaFanZoneActor`
 * (원형은 `FanAngle=360`, 호는 `ArcInnerRadius`를 준 얇은 고리). 둘 다 `RingCount=1`로 강제해
 * "예고 후 단발 판정"만 나오게 한다(계단식 확장은 뒤로 빠지며 좌우장판 전용 연출).
 *
 * 리본 끝자락 이동은 NavMesh(`MoveToLocation`)가 아니라 **직접 위치 보간**이다 — 도착 시점이
 * 정확해야 다음 단계 타이밍이 어긋나지 않고, NavMesh 유무에 의존하지 않기 위함(두번긋고 도넛장판의 상승/하강과 같은 방침).
 */
USTRUCT(meta = (DisplayName = "Echidna Ribbon Pattern", Category = "EchidnaBoss"))
struct FStateTreeTask_EchidnaRibbonPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeEchidnaRibbonPatternInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif // WITH_EDITOR

private:
	FRotator FacePlayer(AEchidnaBoss* Boss) const;

	/** 리본 RibbonCount갈래를 부채꼴로 뻗고, 끝 지점을 RibbonEndLocation에 기록한다 */
	void SpawnRibbons(FInstanceDataType& InstanceData) const;

	bool AreRibbonsFinished(const FInstanceDataType& InstanceData) const;
	bool AnyRibbonHit(const FInstanceDataType& InstanceData) const;

	/** 보스 중심 원형 장판(FanAngle=360) */
	void SpawnCircleZone(FInstanceDataType& InstanceData) const;

	/** 좌측 전방 호(얇은 부채꼴 고리) */
	void SpawnArcZone(FInstanceDataType& InstanceData) const;

	/** 보스 발밑(지면) 높이 — 장판은 여기에 깔려야 공중에 떠 보이지 않는다 */
	FVector GetBossFeetLocation(const AEchidnaBoss* Boss) const;
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

	// 보스 현재 위치 기준으로 패트롤 목표 지점을 고를 최대 반경 (cm)
	UPROPERTY(EditAnywhere, Category = "Patrol")
	float PatrolRadius = 600.f;

	// 패트롤 목표 지점까지 최소 거리 (cm) — 이보다 가까운 지점은 안 뽑히게 해서 "한 발자국만 움직이고 마는"
	// 애매한 걸음을 방지. [MinPatrolRadius, PatrolRadius] 사이 원형 고리(annulus)에서 균등하게 뽑음
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MinPatrolRadius = 300.f;

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
