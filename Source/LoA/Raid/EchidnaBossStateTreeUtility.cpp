#include "EchidnaBossStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "EchidnaBoss.h"

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

#undef LOCTEXT_NAMESPACE
