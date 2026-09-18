#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "EchidnaBossStateTreeUtility.generated.h"

class AEchidnaBoss;
class AEchidnaMirrorActor;
class AEchidnaFanZoneActor;
class AEchidnaTetherActor;
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

	UPROPERTY(EditAnywhere, Category = "Mirror")
	float Damage = 10.f;

	// "개인 유도레이저" — 8거울과 별개로 패턴 시작 시 1개만 스폰, 패턴이 끝날 때까지 계속 추적+반복 발사
	UPROPERTY(EditAnywhere, Category = "Guided")
	float GuidedDamage = 10.f;

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

	UPROPERTY(EditAnywhere, Category = "Fan")
	float Damage = 10.f;

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
