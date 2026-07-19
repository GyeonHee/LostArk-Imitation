#include "EchidnaBossStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "EchidnaBoss.h"
#include "EchidnaMirrorActor.h"
#include "LoA.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

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
