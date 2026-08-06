#include "EchidnaBossStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "EchidnaBoss.h"
#include "EchidnaMirrorActor.h"
#include "EchidnaFanZoneActor.h"
#include "LoA.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

#define LOCTEXT_NAMESPACE "EchidnaBoss"

bool FStateTreeCondition_BossLineThreshold::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Boss)
	{
		return false;
	}

	return InstanceData.Boss->GetCurrentLine() <= InstanceData.TriggerLine
		&& !InstanceData.Boss->IsPatternTriggered(InstanceData.PatternName);
}

#if WITH_EDITOR
FText FStateTreeCondition_BossLineThreshold::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("BossLineThresholdDesc", "<b>Boss Line Threshold Reached</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_MarkBossPatternTriggered::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Boss)
	{
		InstanceData.Boss->MarkPatternTriggered(InstanceData.PatternName);
	}

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FStateTreeTask_MarkBossPatternTriggered::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("MarkBossPatternTriggeredDesc", "<b>Mark Boss Pattern Triggered</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_WaitRandomDuration::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime = FMath::RandRange(InstanceData.MinDuration, InstanceData.MaxDuration);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_WaitRandomDuration::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	return InstanceData.RemainingTime <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreeTask_WaitRandomDuration::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("WaitRandomDurationDesc", "<b>Wait Random Duration</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_EchidnaFourMirrorPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SpawnedMirrors.Reset();

	if (!InstanceData.Boss || !InstanceData.MirrorClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaFourMirror] EnterState 실패 — Boss=%s MirrorClass=%s (StateTree에서 Context Actor 바인딩/MirrorClass 할당을 확인하세요)"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.MirrorClass ? *InstanceData.MirrorClass->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector BossLocation = InstanceData.Boss->GetActorLocation();
	const FRotator BossRotation = InstanceData.Boss->GetActorRotation();
	AController* BossController = InstanceData.Boss->GetController();

	// 보스 정면 기준 대각 4방향 (전방좌/전방우/후방좌/후방우)
	static const float DiagonalYawOffsets[4] = { 45.f, 135.f, 225.f, 315.f };

	for (const float YawOffset : DiagonalYawOffsets)
	{
		const FRotator SpawnRotation = BossRotation + FRotator(0.f, YawOffset, 0.f);
		const FVector SpawnLocation = BossLocation + SpawnRotation.Vector() * InstanceData.MirrorSpawnRadius;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = InstanceData.Boss;
		AEchidnaMirrorActor* Mirror = World->SpawnActor<AEchidnaMirrorActor>(
			InstanceData.MirrorClass, SpawnLocation, SpawnRotation, SpawnParams);

		if (Mirror)
		{
			Mirror->Activate(InstanceData.Damage, BossController);
			InstanceData.SpawnedMirrors.Add(Mirror);
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[EchidnaFourMirror] SpawnActor 실패 (YawOffset=%.0f)"), YawOffset);
		}
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaFourMirror] 거울 %d개 스폰 완료 (BossLoc=%s)"),
		InstanceData.SpawnedMirrors.Num(), *BossLocation.ToString());

	return InstanceData.SpawnedMirrors.Num() > 0 ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FStateTreeTask_EchidnaFourMirrorPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	for (const TObjectPtr<AEchidnaMirrorActor>& Mirror : InstanceData.SpawnedMirrors)
	{
		if (Mirror && !Mirror->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}
	}

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaFourMirrorPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaFourMirrorPatternDesc", "<b>Echidna Four Mirror Pattern</b>");
}
#endif // WITH_EDITOR

bool FStateTreeTask_EchidnaRetreatFanPattern::HasGroundBelow(const FInstanceDataType& InstanceData, const FVector& Location) const
{
	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return false;

	FHitResult Hit;
	const FVector TraceStart = Location + FVector(0.f, 0.f, 200.f);
	const FVector TraceEnd = Location - FVector(0.f, 0.f, 500.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(InstanceData.Boss);

	return World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

void FStateTreeTask_EchidnaRetreatFanPattern::HopBackward(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss) return;

	FVector AwayFromPlayer = -InstanceData.Boss->GetActorForwardVector();

	// 보스 자체 회전이 아니라 "플레이어 반대 방향"으로 밀려나야 항상 플레이어에게서 멀어짐
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(InstanceData.Boss->GetWorld(), 0))
	{
		FVector Away = InstanceData.Boss->GetActorLocation() - PlayerChar->GetActorLocation();
		Away.Z = 0.f;
		if (!Away.IsNearlyZero())
		{
			AwayFromPlayer = Away.GetSafeNormal();
		}
	}

	// 착지 예상 지점에 바닥이 없으면(맵 끝자락) 홉 자체를 취소 — 낙사 방지
	const FVector LandingPoint = InstanceData.Boss->GetActorLocation() + AwayFromPlayer * InstanceData.HopCheckDistance;
	if (!HasGroundBelow(InstanceData, LandingPoint))
	{
		UE_LOG(LogLoA, Log, TEXT("[EchidnaRetreatFan] 후방 홉 취소 — 착지 예상 지점에 바닥 없음: %s"), *LandingPoint.ToString());
		return;
	}

	const FVector LaunchVelocity = AwayFromPlayer * InstanceData.HopBackStrength + FVector(0.f, 0.f, InstanceData.HopUpwardStrength);
	InstanceData.Boss->LaunchCharacter(LaunchVelocity, true, true);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaRetreatFan] 후방 홉 — Loc=%s Velocity=%s"),
		*InstanceData.Boss->GetActorLocation().ToString(), *LaunchVelocity.ToString());
}

FRotator FStateTreeTask_EchidnaRetreatFanPattern::ComputeAimRotation(const AEchidnaBoss* Boss) const
{
	if (!Boss)
	{
		return FRotator::ZeroRotator;
	}

	FRotator AimRotation = Boss->GetActorRotation();

	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(Boss->GetWorld(), 0))
	{
		FVector ToPlayer = PlayerChar->GetActorLocation() - Boss->GetActorLocation();
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			AimRotation = ToPlayer.GetSafeNormal().Rotation();
		}
	}

	return AimRotation;
}

AEchidnaFanZoneActor* FStateTreeTask_EchidnaRetreatFanPattern::SpawnFan(FInstanceDataType& InstanceData, float YawOffsetDeg) const
{
	if (!InstanceData.Boss || !InstanceData.FanZoneClass)
	{
		return nullptr;
	}

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Boss->GetActorLocation()은 캡슐 중심(지면에서 캡슐 절반 높이만큼 위) 기준이라
	// 그대로 쓰면 장판이 공중에 뜬 것처럼 보임 — 캡슐 절반 높이를 빼서 발밑(지면) 높이로 보정
	FVector SpawnLocation = InstanceData.Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
	{
		SpawnLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	// 매 캐스팅마다 플레이어 위치를 다시 조준하면 1번(왼쪽)과 2번(오른쪽)의 기준선이 서로 달라져
	// 패턴이 시작된 후 플레이어가 움직이면 겹치는 구간이 어긋나 보임 — EnterState에서 한 번만 정한
	// BaseAimRotation을 그대로 쓰고, 여기서는 좌/우 각도(YawOffsetDeg)만 더한다
	FRotator SpawnRotation = InstanceData.BaseAimRotation;
	SpawnRotation.Yaw += YawOffsetDeg;

	// 보스가 실제로 쏘는 방향을 쳐다보도록 — 장판과 동일한 Yaw로 직접 회전시킴
	InstanceData.Boss->SetActorRotation(FRotator(0.f, SpawnRotation.Yaw, 0.f));

	AController* BossController = InstanceData.Boss->GetController();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaFanZoneActor* Fan = World->SpawnActor<AEchidnaFanZoneActor>(
		InstanceData.FanZoneClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (Fan)
	{
		Fan->Activate(InstanceData.Damage, BossController);
	}
	else
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRetreatFan] SpawnActor 실패"));
	}

	return Fan;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRetreatFanPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentFan = nullptr;
	InstanceData.bHoppedForCurrentCast = false;

	if (!InstanceData.Boss || !InstanceData.FanZoneClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRetreatFan] EnterState 실패 — Boss=%s FanZoneClass=%s (StateTree에서 Context Actor 바인딩/FanZoneClass 할당을 확인하세요)"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.FanZoneClass ? *InstanceData.FanZoneClass->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	// 패턴 시작 시 보스를 제자리에 멈춰 세움 (직전 패트롤 등의 잔여 이동 취소)
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 패턴 시작 순간(그 순간 쳐다본 곳=플레이어 방향)의 조준 방향을 한 번만 고정 —
	// 1번/2번 캐스팅 모두 이 기준선에 좌/우 각도만 더해서 씀 (매번 플레이어 위치로 다시 조준하지 않음)
	InstanceData.BaseAimRotation = ComputeAimRotation(InstanceData.Boss);

	// 1번째 장판 — 정면 기준 왼쪽으로 비스듬히
	InstanceData.CurrentFan = SpawnFan(InstanceData, -InstanceData.FanYawOffset);
	if (!InstanceData.CurrentFan)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Phase = EEchidnaRetreatFanPhase::Casting1;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRetreatFanPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	switch (InstanceData.Phase)
	{
	case EEchidnaRetreatFanPhase::Casting1:
	{
		if (!InstanceData.bHoppedForCurrentCast && InstanceData.CurrentFan && InstanceData.CurrentFan->HasStartedExploding())
		{
			HopBackward(InstanceData);
			InstanceData.bHoppedForCurrentCast = true;
		}

		if (!InstanceData.CurrentFan || !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		// 2번째 장판 — 정면 기준 오른쪽으로 비스듬히 (가운데가 1번과 겹침)
		InstanceData.bHoppedForCurrentCast = false;
		InstanceData.CurrentFan = SpawnFan(InstanceData, InstanceData.FanYawOffset);
		if (!InstanceData.CurrentFan)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.Phase = EEchidnaRetreatFanPhase::Casting2;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaRetreatFanPhase::Casting2:
	{
		if (!InstanceData.bHoppedForCurrentCast && InstanceData.CurrentFan && InstanceData.CurrentFan->HasStartedExploding())
		{
			HopBackward(InstanceData);
			InstanceData.bHoppedForCurrentCast = true;
		}

		if (!InstanceData.CurrentFan || !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.Phase = EEchidnaRetreatFanPhase::Done;
		return EStateTreeRunStatus::Succeeded;
	}
	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaRetreatFanPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaRetreatFanPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaRetreatFanPatternDesc", "<b>Echidna Retreat Fan Pattern</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_EchidnaPatrol::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss || !InstanceData.AIController)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaPatrol] EnterState 실패 — Boss=%s AIController=%s"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.AIController ? TEXT("Valid") : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	const FVector Origin = InstanceData.Boss->GetActorLocation();
	const FVector2D RandOffset = FMath::RandPointInCircle(InstanceData.PatrolRadius);
	const FVector TargetPoint = Origin + FVector(RandOffset.X, RandOffset.Y, 0.f);

	const EPathFollowingRequestResult::Type Result = InstanceData.AIController->MoveToLocation(
		TargetPoint, InstanceData.AcceptanceRadius);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaPatrol] MoveToLocation 실패 — Target=%s (레벨에 Nav Mesh Bounds Volume이 있는지 확인하세요)"),
			*TargetPoint.ToString());
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaPatrol::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AIController)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const EPathFollowingStatus::Type Status = InstanceData.AIController->GetMoveStatus();
	return (Status == EPathFollowingStatus::Moving) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FStateTreeTask_EchidnaPatrol::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaPatrol::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaPatrolDesc", "<b>Echidna Patrol</b>");
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
