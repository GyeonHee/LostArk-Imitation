#include "EchidnaBossStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "EchidnaBoss.h"
#include "EchidnaMirrorActor.h"
#include "EchidnaFanZoneActor.h"
#include "EchidnaTetherActor.h"
#include "EchidnaHeartActor.h"
#include "EchidnaOrbActor.h"
#include "EchidnaPoopMarkActor.h"
#include "EchidnaPoopBeamActor.h"
#include "EchidnaFlytrapZoneActor.h"
#include "EchidnaLinkMirrorActor.h"
#include "HexTile.h"
#include "LoAPlayerController.h"
#include "HexArena.h"
#include "EngineUtils.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/DamageType.h"

#define LOCTEXT_NAMESPACE "EchidnaBoss"

namespace
{
	/** 큰 패턴(시간·정산) Task의 발동 표시 — Task의 PatternName과 **이 Task가 속한 State 이름** 둘 다 표시한다.
	 *  StateTree 조건의 PatternName을 Task와 다르게 적는 실수(RenGrab·MirrorLink에서 반복)가 나도, 조건에 State 이름을
	 *  적었다면 맞아떨어져 패턴이 무한 반복되지 않는다. 이미 표시된 이름으로 재진입하면 경고를 남긴다 */
	void MarkBigPatternTriggered(FStateTreeExecutionContext& Context, AEchidnaBoss* Boss, FName PatternName, const TCHAR* LogTag)
	{
		// 지금 EnterState 중인 State — 조건에 흔히 State 이름을 그대로 적기 때문에 이것도 같이 표시한다
		FName StateName = NAME_None;
		if (const FCompactStateTreeState* State = Context.GetStateFromHandle(Context.GetCurrentlyProcessedState()))
		{
			StateName = State->Name;
		}

		if (Boss->IsPatternTriggered(PatternName) || (!StateName.IsNone() && Boss->IsPatternTriggered(StateName)))
		{
			UE_LOG(LogLoA, Warning, TEXT("[%s] 이미 발동한 패턴에 재진입 (PatternName '%s', State '%s') — StateTree 조건의 PatternName을 둘 중 하나로 맞출 것"),
				LogTag, *PatternName.ToString(), *StateName.ToString());
		}

		Boss->MarkPatternTriggered(PatternName);
		if (!StateName.IsNone())
		{
			Boss->MarkPatternTriggered(StateName);
		}
		Boss->SetActiveTimedPattern(PatternName);
	}
}

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

bool FStateTreeCondition_BossSettlementGauge::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Boss)
	{
		return false;
	}

	// 시간 패턴·다른 정산 패턴 진행 중이면 대기 (Root On Tick 전이가 진행 중인 큰 패턴을 끊지 않게).
	// 게이지가 한 번에 25·50을 둘 다 넘어도 25가 먼저 — "다음 차례" 지점만 통과
	if (InstanceData.Boss->IsTimedPatternActive()
		|| InstanceData.Boss->GetNextSettlementThreshold() != InstanceData.GaugePercent)
	{
		return false;
	}

	for (const FName& ActiveState : Context.GetActiveStateNames())
	{
		if (InstanceData.WaitWhileStatesActive.Contains(ActiveState))
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
FText FStateTreeCondition_BossSettlementGauge::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("BossSettlementGaugeDesc", "<b>Boss Settlement Gauge Reached</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_ResetSettlementGauge::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Boss)
	{
		// 게이지 0 + 사용한 발동 지점 초기화 (거울잇기 100%가 끝날 때 자동으로 하므로 보통은 필요 없음)
		InstanceData.Boss->ResetSettlementGauge();
		for (const FName& PatternName : InstanceData.PatternNamesToRearm)
		{
			InstanceData.Boss->UnmarkPatternTriggered(PatternName);
		}
	}

	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FStateTreeTask_ResetSettlementGauge::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("ResetSettlementGaugeDesc", "<b>Reset Settlement Gauge</b>");
}
#endif // WITH_EDITOR

bool FStateTreeCondition_BossEnrageTime::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Boss || InstanceData.Boss->GetHP() <= 0.0)
	{
		return false;
	}

	if (InstanceData.Boss->IsTimedPatternActive()
		|| InstanceData.Boss->GetEnrageRemainingTime() > InstanceData.RemainingSeconds
		|| InstanceData.Boss->IsPatternTriggered(InstanceData.PatternName))
	{
		return false;
	}

	// 짤패턴 진행 중이면 끝날 때까지 기다린다 (시간이 지났다는 사실은 그대로라 끝나는 즉시 통과)
	for (const FName& ActiveState : Context.GetActiveStateNames())
	{
		if (InstanceData.WaitWhileStatesActive.Contains(ActiveState))
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
FText FStateTreeCondition_BossEnrageTime::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("BossEnrageTimeDesc", "<b>Boss Enrage Time Reached</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_EchidnaPoopPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Elapsed = 0.f;
	InstanceData.bBeamSpawned = false;
	InstanceData.Mark = nullptr;
	InstanceData.Beam = nullptr;

	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaPoop] EnterState 실패 — Boss 바인딩 확인"));
		return EStateTreeRunStatus::Failed;
	}

	// State가 중간에 끊겨 재진입해도 두 번 나오지 않도록 진입 즉시 표시 (별도 Mark Task를 나란히 두면 그 Task가
	// 즉시 Succeeded를 반환해 State가 바로 끝나버릴 수 있어서 이 Task가 직접 한다)
	// 정산 패턴(25%/75%) — 발동 지점을 소모. 이름 표시는 안 한다(조건이 %로만 판단)
	InstanceData.ConsumedThreshold = Boss->ConsumeNextSettlementThreshold();
	Boss->SetActiveTimedPattern(InstanceData.PatternName);

	// 1) 보스는 그 자리에 정지
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	Boss->GetCharacterMovement()->StopMovementImmediately();

	// 2) 오염 장판 전부 활성화
	AHexArena* Arena = nullptr;
	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		Arena = *It;
		break;
	}
	if (Arena)
	{
		Arena->SetAllPoopTilesActive(true);
	}

	// 3) 카메라 줌아웃 + 플레이어 발밑 게이지
	ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	InstanceData.Player = Player;
	if (Player)
	{
		Player->SetCameraZoomOverride(InstanceData.CameraArmLength);

		UClass* MarkClass = InstanceData.MarkClass ? InstanceData.MarkClass.Get() : AEchidnaPoopMarkActor::StaticClass();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Boss;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InstanceData.Mark = World->SpawnActor<AEchidnaPoopMarkActor>(MarkClass, Player->GetActorTransform(), SpawnParams);
		if (InstanceData.Mark)
		{
			InstanceData.Mark->Activate(Player, Arena);
		}
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaPoop] 패턴 시작 — Arena:%s Player:%s"), Arena ? TEXT("O") : TEXT("X"), Player ? TEXT("O") : TEXT("X"));
	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaPoopPattern::SpawnBeam(FInstanceDataType& InstanceData) const
{
	InstanceData.bBeamSpawned = true;

	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World) return;

	// 캡슐 중심이 아니라 발밑에서 시작해야 장판이 바닥에 붙는다
	FVector Origin = Boss->GetActorLocation();
	Origin.Z -= Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FRotator Facing = Boss->GetActorRotation();
	if (ALoACharacter* Player = InstanceData.Player.Get())
	{
		const FVector ToPlayer = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		if (!ToPlayer.IsNearlyZero())
		{
			Facing = FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f);
		}
	}

	UClass* BeamClass = InstanceData.BeamClass ? InstanceData.BeamClass.Get() : AEchidnaPoopBeamActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InstanceData.Beam = World->SpawnActor<AEchidnaPoopBeamActor>(BeamClass, Origin, Facing, SpawnParams);
	if (InstanceData.Beam)
	{
		InstanceData.Beam->Activate(InstanceData.Player.Get(), InstanceData.BeamDamage, InstanceData.CircleDamage,
			Boss->GetController(), InstanceData.BeamLengthOverride, InstanceData.BeamHalfWidthOverride, InstanceData.CircleRadiusOverride);
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaPoopPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 패턴 내내 제자리
	InstanceData.Boss->GetCharacterMovement()->StopMovementImmediately();

	InstanceData.Elapsed += DeltaTime;
	if (!InstanceData.bBeamSpawned && InstanceData.Elapsed >= InstanceData.BeamSpawnDelay)
	{
		SpawnBeam(InstanceData);
	}

	const bool bMarkDone = !IsValid(InstanceData.Mark) || InstanceData.Mark->IsFinished();
	const bool bBeamDone = InstanceData.bBeamSpawned && (!IsValid(InstanceData.Beam) || InstanceData.Beam->IsFinished());
	return (bMarkDone && bBeamDone) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaPoopPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (ALoACharacter* Player = InstanceData.Player.Get())
	{
		Player->ClearCameraZoomOverride();
	}
	if (InstanceData.Boss)
	{
		InstanceData.Boss->ClearActiveTimedPattern();
	}

	// 패턴 동안만 활성 — 끝나면(중간에 끊겨도) 오염 장판은 다시 비활성(핑크)으로
	if (UWorld* World = InstanceData.Boss ? InstanceData.Boss->GetWorld() : nullptr)
	{
		for (TActorIterator<AHexArena> It(World); It; ++It)
		{
			It->SetAllPoopTilesActive(false);
		}
	}

	// 중간에 끊긴 경우에만 남은 것 정리 (정상 종료면 둘 다 스스로 소멸 예약된 상태)
	if (IsValid(InstanceData.Mark) && !InstanceData.Mark->IsFinished())
	{
		InstanceData.Mark->Destroy();
	}
	if (IsValid(InstanceData.Beam) && !InstanceData.Beam->IsFinished())
	{
		InstanceData.Beam->Destroy();
	}
	InstanceData.Mark = nullptr;
	InstanceData.Beam = nullptr;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaPoopPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaPoopPatternDesc", "<b>Echidna Poop Pattern</b>");
}
#endif // WITH_EDITOR

EStateTreeRunStatus FStateTreeTask_EchidnaRandomGrabPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Phase = EEchidnaRandomGrabPhase::WaitFog;
	InstanceData.PhaseElapsed = 0.f;
	InstanceData.RoundsStarted = 0;
	InstanceData.Traps.Reset();
	InstanceData.Arena = nullptr;

	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRandomGrab] EnterState 실패 — Boss 바인딩 확인"));
		return EStateTreeRunStatus::Failed;
	}

	// 재진입해도 두 번 나오지 않도록 진입 즉시 표시 (똥장판 패턴과 같은 이유로 Task가 직접)
	MarkBigPatternTriggered(Context, Boss, InstanceData.PatternName, TEXT("EchidnaRandomGrab"));

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	Boss->GetCharacterMovement()->StopMovementImmediately();

	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		InstanceData.Arena = *It;
		break;
	}
	if (InstanceData.Arena)
	{
		InstanceData.Arena->SetAllPoopTilesActive(true);
	}

	InstanceData.Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));

	UE_LOG(LogLoA, Log, TEXT("[EchidnaRandomGrab] 패턴 시작 — Arena:%s Player:%s"),
		InstanceData.Arena ? TEXT("O") : TEXT("X"), InstanceData.Player.IsValid() ? TEXT("O") : TEXT("X"));
	return EStateTreeRunStatus::Running;
}

bool FStateTreeTask_EchidnaRandomGrabPattern::SpawnTrapAt(FInstanceDataType& InstanceData, const FIntPoint& Coord) const
{
	AHexArena* Arena = InstanceData.Arena;
	AHexTile* Tile = Arena ? Arena->GetTile(Coord) : nullptr;
	if (!Tile) return false;

	// 타일과 같은 회전 — 장판의 헥스 모양이 타일과 딱 겹친다. 높이는 타일 윗면(바운즈 꼭대기)
	FVector Location = Tile->GetActorLocation();
	Location.Z = Tile->TileMesh->Bounds.Origin.Z + Tile->TileMesh->Bounds.BoxExtent.Z;

	UClass* TrapClass = InstanceData.FlytrapClass ? InstanceData.FlytrapClass.Get() : AEchidnaFlytrapZoneActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEchidnaFlytrapZoneActor* Trap = Arena->GetWorld()->SpawnActor<AEchidnaFlytrapZoneActor>(
		TrapClass, Location, Tile->GetActorRotation(), SpawnParams);
	if (!Trap) return false;

	Trap->Activate(Arena, Coord, Arena->TileSpacing * 0.5f, InstanceData.EatDamageRatio,
		InstanceData.Boss ? InstanceData.Boss->GetController() : nullptr);

	InstanceData.Traps.Add(Trap);
	++InstanceData.RoundsStarted;
	return true;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRandomGrabPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Boss->GetCharacterMovement()->StopMovementImmediately();
	InstanceData.PhaseElapsed += DeltaTime;

	ALoACharacter* Player = InstanceData.Player.Get();

	switch (InstanceData.Phase)
	{
	case EEchidnaRandomGrabPhase::WaitFog:
		if (InstanceData.PhaseElapsed >= InstanceData.FogDelay)
		{
			if (Player)
			{
				if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
				{
					PC->SetScreenFog(true, InstanceData.FogFadeTime);
				}
			}
			InstanceData.Phase = EEchidnaRandomGrabPhase::FogIn;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaRandomGrabPhase::FogIn:
		if (InstanceData.PhaseElapsed >= InstanceData.FirstTrapDelay)
		{
			InstanceData.Phase = EEchidnaRandomGrabPhase::Rounds;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaRandomGrabPhase::Rounds:
	{
		const bool bPlayerHeld = Player && Player->IsHeldByPattern();
		const bool bCanSpawnMore = !bPlayerHeld
			&& InstanceData.RoundsStarted < InstanceData.RoundCount
			&& InstanceData.PhaseElapsed < InstanceData.MaxRoundsDuration;

		// 깔린 장판이 전부 꽃까지 나왔는가 — 아니면 다음 장판을 깔지 않는다 (한 번에 하나씩)
		const bool bAllShown = !InstanceData.Traps.ContainsByPredicate([](const TObjectPtr<AEchidnaFlytrapZoneActor>& Trap)
		{
			return IsValid(Trap) && !Trap->IsTrapShown();
		});

		// 앞 장판의 꽃이 다 나왔으면 지금 서 있는 타일에 다음 장판 — 같은 타일엔 중복 불가(이미 꽃이 있는 타일 등)
		FIntPoint Coord;
		if (bCanSpawnMore && bAllShown && Player && InstanceData.Arena && InstanceData.Arena->WorldToTileCoord(Player->GetActorLocation(), Coord))
		{
			const bool bAlreadyHasTrap = InstanceData.Traps.ContainsByPredicate([&Coord](const TObjectPtr<AEchidnaFlytrapZoneActor>& Trap)
			{
				return IsValid(Trap) && Trap->GetCoord() == Coord;
			});
			if (!bAlreadyHasTrap)
			{
				SpawnTrapAt(InstanceData, Coord);
			}
		}

		// 더 깔 게 없고 깔린 것들이 전부 꽃까지 나왔으면 마무리 (방금 깐 장판은 아직 꽃 전이라 다음 틱 이후)
		if (!bCanSpawnMore && bAllShown)
		{
			InstanceData.Phase = EEchidnaRandomGrabPhase::Ending;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;
	}

	case EEchidnaRandomGrabPhase::Ending:
		if (InstanceData.PhaseElapsed >= InstanceData.EndDelay)
		{
			InstanceData.Phase = EEchidnaRandomGrabPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		break;

	default:
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaRandomGrabPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.Boss)
	{
		InstanceData.Boss->ClearActiveTimedPattern();
	}

	// 꽃은 패턴이 끝날 때 사라진다 (정상 종료든 중간에 끊겼든)
	for (const TObjectPtr<AEchidnaFlytrapZoneActor>& Trap : InstanceData.Traps)
	{
		if (IsValid(Trap))
		{
			Trap->Dismiss();
		}
	}
	InstanceData.Traps.Reset();

	// 먹혀서 붙잡혀 있던 플레이어 해제 — 시간 제한이 없는 상태라 여기서 반드시 풀어야 한다
	if (InstanceData.Boss)
	{
		for (TActorIterator<ALoACharacter> It(InstanceData.Boss->GetWorld()); It; ++It)
		{
			It->SetHeldByPattern(false);
		}
	}

	// 연기 걷힘 + 오염 장판 비활성
	if (ALoACharacter* Player = InstanceData.Player.Get())
	{
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
		{
			PC->SetScreenFog(false, InstanceData.FogFadeTime);
		}
	}
	if (InstanceData.Arena)
	{
		InstanceData.Arena->SetAllPoopTilesActive(false);
	}
	InstanceData.Arena = nullptr;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaRandomGrabPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaRandomGrabPatternDesc", "<b>Echidna Random Grab Pattern</b>");
}
#endif // WITH_EDITOR

namespace
{
	void SetBossVanished(AEchidnaBoss* Boss, bool bVanished)
	{
		if (!Boss) return;
		Boss->SetActorHiddenInGame(bVanished);
		Boss->SetActorEnableCollision(!bVanished);
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaMirrorLinkPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Phase = EEchidnaMirrorLinkPhase::Vanished;
	InstanceData.PhaseElapsed = 0.f;
	InstanceData.Mirror = nullptr;
	InstanceData.Arena = nullptr;

	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorLink] EnterState 실패 — Boss 바인딩 확인"));
		return EStateTreeRunStatus::Failed;
	}

	// 정산 패턴(50%/100%) — 발동 지점을 소모. 100%였으면 끝날 때 게이지를 0으로 되돌린다
	InstanceData.ConsumedThreshold = Boss->ConsumeNextSettlementThreshold();
	Boss->SetActiveTimedPattern(InstanceData.PatternName);

	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		InstanceData.Arena = *It;
		break;
	}
	if (!InstanceData.Arena || InstanceData.Arena->MarkerTileCoords.Num() < 2)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorLink] 파란 테두리 타일 정보 없음 — 아레나/초기 배치 확인"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	Boss->GetCharacterMovement()->StopMovementImmediately();
	SetBossVanished(Boss, true);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirrorLink] 패턴 시작 — 거울 (%d,%d), 보스 (%d,%d)"),
		InstanceData.Arena->MarkerTileCoords[0].X, InstanceData.Arena->MarkerTileCoords[0].Y,
		InstanceData.Arena->MarkerTileCoords[1].X, InstanceData.Arena->MarkerTileCoords[1].Y);
	return EStateTreeRunStatus::Running;
}

bool FStateTreeTask_EchidnaMirrorLinkPattern::AppearBossAndMirror(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	AHexArena* Arena = InstanceData.Arena;
	if (!Boss || !Arena) return false;

	const FIntPoint MirrorCoord = Arena->MarkerTileCoords[0];
	InstanceData.BossCoord = Arena->MarkerTileCoords[1];

	FVector MirrorLocation, BossTileTop;
	if (!Arena->GetTileTopLocation(MirrorCoord, MirrorLocation) || !Arena->GetTileTopLocation(InstanceData.BossCoord, BossTileTop))
	{
		return false;
	}

	// 보스 — 안쪽 파란 타일 위에(캡슐 중심 = 윗면 + 절반 높이), 거울 쪽을 바라보고 선다
	InstanceData.BossStandLocation = BossTileTop + FVector(0.f, 0.f, Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector ToMirror = (MirrorLocation - BossTileTop).GetSafeNormal2D();
	Boss->SetActorLocationAndRotation(InstanceData.BossStandLocation, FRotator(0.f, ToMirror.Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Boss->GetCharacterMovement()->StopMovementImmediately();
	SetBossVanished(Boss, false);

	// 거울 — 외곽 파란 타일 위
	UClass* MirrorClass = InstanceData.MirrorClass ? InstanceData.MirrorClass.Get() : AEchidnaLinkMirrorActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InstanceData.Mirror = Boss->GetWorld()->SpawnActor<AEchidnaLinkMirrorActor>(MirrorClass, MirrorLocation, FRotator::ZeroRotator, SpawnParams);
	if (!InstanceData.Mirror) return false;

	InstanceData.Mirror->Activate(InstanceData.Player.Get(), Arena, Boss, InstanceData.BossCoord);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EchidnaMirrorLinkPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	Boss->GetCharacterMovement()->StopMovementImmediately();
	InstanceData.PhaseElapsed += DeltaTime;

	switch (InstanceData.Phase)
	{
	case EEchidnaMirrorLinkPhase::Vanished:
		if (InstanceData.PhaseElapsed >= InstanceData.VanishDuration)
		{
			if (!AppearBossAndMirror(InstanceData))
			{
				UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorLink] 보스/거울 등장 실패"));
				return EStateTreeRunStatus::Failed;
			}
			InstanceData.Phase = EEchidnaMirrorLinkPhase::Linking;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaMirrorLinkPhase::Linking:
	{
		// 패턴 내내 그 타일 위에 가만히
		Boss->SetActorLocation(InstanceData.BossStandLocation);

		if (!IsValid(InstanceData.Mirror))
		{
			return EStateTreeRunStatus::Failed;
		}

		const EEchidnaLinkResult Result = InstanceData.Mirror->GetResult();
		if (Result == EEchidnaLinkResult::Success)
		{
			InstanceData.Phase = EEchidnaMirrorLinkPhase::Ending;
			InstanceData.PhaseElapsed = 0.f;
		}
		else if (Result == EEchidnaLinkResult::Fail)
		{
			// 보스까지 못 이었다 — 맵 전체가 빨갛게 터지고 즉사급 데미지
			if (InstanceData.Arena)
			{
				InstanceData.Arena->SetAllTilesDangerFlash(true);
			}
			for (TActorIterator<ALoACharacter> It(Boss->GetWorld()); It; ++It)
			{
				UGameplayStatics::ApplyDamage(*It, It->GetMaxHP() * InstanceData.FailDamageRatio, Boss->GetController(), Boss, UDamageType::StaticClass());
			}
			InstanceData.Phase = EEchidnaMirrorLinkPhase::Failing;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;
	}

	case EEchidnaMirrorLinkPhase::Failing:
		if (InstanceData.PhaseElapsed >= InstanceData.FailFlashDuration)
		{
			if (InstanceData.Arena)
			{
				InstanceData.Arena->SetAllTilesDangerFlash(false);
			}
			InstanceData.Phase = EEchidnaMirrorLinkPhase::Ending;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaMirrorLinkPhase::Ending:
		if (InstanceData.PhaseElapsed >= InstanceData.EndDelay)
		{
			InstanceData.Phase = EEchidnaMirrorLinkPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		break;

	default:
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaMirrorLinkPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;

	if (Boss)
	{
		// 어떤 경우든 보스는 보이는 상태로 복구 (사라진 채 끊기면 영영 안 보임)
		SetBossVanished(Boss, false);
		Boss->ClearActiveTimedPattern();

		// 풀정산(100%)이었으면 한 바퀴 끝 — 게이지 0 + 25/50/75/100 다시 사용 가능
		if (InstanceData.ConsumedThreshold >= 100)
		{
			Boss->ResetSettlementGauge();
		}

		for (TActorIterator<ALoACharacter> It(Boss->GetWorld()); It; ++It)
		{
			It->SetHeldByPattern(false);
		}
	}

	if (InstanceData.Arena)
	{
		InstanceData.Arena->ClearAllLinkHighlights();
		InstanceData.Arena->SetAllTilesDangerFlash(false);
	}

	if (IsValid(InstanceData.Mirror))
	{
		InstanceData.Mirror->Destroy();
	}
	InstanceData.Mirror = nullptr;
	InstanceData.Arena = nullptr;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaMirrorLinkPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaMirrorLinkPatternDesc", "<b>Echidna Mirror Link Pattern</b>");
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

FRotator FStateTreeTask_EchidnaEightMirrorPattern::ComputeAimRotation(const AEchidnaBoss* Boss) const
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

void FStateTreeTask_EchidnaEightMirrorPattern::SpawnSpokeGroup(FInstanceDataType& InstanceData, const float (&AnglesDeg)[4], TArray<TObjectPtr<AEchidnaMirrorActor>>& OutMirrors, bool bActivateNow) const
{
	OutMirrors.Reset();

	if (!InstanceData.Boss || !InstanceData.MirrorClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	const FVector BossLocation = InstanceData.Boss->GetActorLocation();
	AController* BossController = InstanceData.Boss->GetController();

	for (const float AngleDeg : AnglesDeg)
	{
		FRotator SpawnRotation = InstanceData.BaseAimRotation;
		SpawnRotation.Yaw += AngleDeg;

		// 스포크 방향(보스 바깥쪽)으로 배치 — 레이저도 같은 방향(바깥쪽)으로 뻗어나감
		const FVector SpawnLocation = BossLocation + SpawnRotation.Vector() * InstanceData.MirrorSpawnRadius;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = InstanceData.Boss;
		AEchidnaMirrorActor* Mirror = World->SpawnActor<AEchidnaMirrorActor>(
			InstanceData.MirrorClass, SpawnLocation, SpawnRotation, SpawnParams);

		if (Mirror)
		{
			// 고정 스포크 — 플레이어를 조준하지 않고 스폰 방향(바깥쪽) 그대로 유지.
			// bActivateNow=false면 몸체만 보이는 채로 대기(Tick의 bActivated 가드 덕에 혼자 타임아웃되지 않음) —
			// 자기 차례(다음 파동)가 되면 Task가 별도로 Activate()를 호출
			Mirror->bLockDirectionOnSpawn = true;
			if (bActivateNow)
			{
				Mirror->Activate(InstanceData.Damage, BossController);
			}
			OutMirrors.Add(Mirror);
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[EchidnaEightMirror] 스포크 거울 SpawnActor 실패 (Angle=%.0f)"), AngleDeg);
		}
	}
}

void FStateTreeTask_EchidnaEightMirrorPattern::SpawnGuidedMirror(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss || !InstanceData.MirrorClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	const FVector BossLocation = InstanceData.Boss->GetActorLocation();
	AController* BossController = InstanceData.Boss->GetController();

	// 보스 오른쪽으로 GuidedHoverHeight만큼 떨어진 지점에서 스폰 — 높이는 보스 캡슐 중심 Z(=보스 키의 절반)
	// 그대로 사용. bSkyGuidedMode의 Tracking 단계 로직이 스폰 직후부터 매 틱 X/Y를 플레이어 위치로 맞춰가므로
	// 여기서 플레이어 위치를 미리 계산할 필요 없음 — 스폰되자마자 자연스럽게 쫓아가기 시작함
	const FVector SpawnLocation = BossLocation + InstanceData.Boss->GetActorRightVector() * InstanceData.GuidedHoverHeight;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaMirrorActor* Mirror = World->SpawnActor<AEchidnaMirrorActor>(
		InstanceData.MirrorClass, SpawnLocation, InstanceData.BaseAimRotation, SpawnParams);

	if (Mirror)
	{
		// bLockDirectionOnSpawn=false(기본값) — 플레이어를 계속 추적. bSkyGuidedMode=true — X/Y를 따라다니고,
		// 위→아래 비스듬한 각도로 조준하며, 발사 후에도 StopRepeating() 전까지 무한 반복
		Mirror->bSkyGuidedMode = true;
		Mirror->Activate(InstanceData.GuidedDamage, BossController);
		InstanceData.GuidedMirror = Mirror;
	}
	else
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaEightMirror] 유도 거울 SpawnActor 실패"));
	}
}

bool FStateTreeTask_EchidnaEightMirrorPattern::AreMirrorsFinished(const TArray<TObjectPtr<AEchidnaMirrorActor>>& Mirrors) const
{
	for (const TObjectPtr<AEchidnaMirrorActor>& Mirror : Mirrors)
	{
		if (Mirror && !Mirror->IsFinished())
		{
			return false;
		}
	}
	return true;
}

EStateTreeRunStatus FStateTreeTask_EchidnaEightMirrorPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.PlusMirrors.Reset();
	InstanceData.CrossMirrors.Reset();
	InstanceData.GuidedMirror = nullptr;

	if (!InstanceData.Boss || !InstanceData.MirrorClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaEightMirror] EnterState 실패 — Boss=%s MirrorClass=%s (StateTree에서 Context Actor 바인딩/MirrorClass 할당을 확인하세요)"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.MirrorClass ? *InstanceData.MirrorClass->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	// 패턴 시작 순간의 방향을 한 번만 고정 — 두 대형("+"/"X") 모두 이 기준선 + 스포크 각도만 씀
	InstanceData.BaseAimRotation = ComputeAimRotation(InstanceData.Boss);

	static const float PlusAngles[4] = { 0.f, 90.f, 180.f, 270.f };
	static const float CrossAngles[4] = { 45.f, 135.f, 225.f, 315.f };

	// 8개 전부 한 번에 스폰 — "+" 4개는 바로 Activate, "X" 4개는 몸체만 보이는 채로 대기
	SpawnSpokeGroup(InstanceData, PlusAngles, InstanceData.PlusMirrors, /*bActivateNow=*/true);
	SpawnSpokeGroup(InstanceData, CrossAngles, InstanceData.CrossMirrors, /*bActivateNow=*/false);

	// 유도 거울은 패턴 시작 시 1회만 스폰 — 파동 전환과 무관하게 독립적으로 반복 진행
	SpawnGuidedMirror(InstanceData);

	InstanceData.Phase = EEchidnaEightMirrorPhase::PlusWave;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaEightMirrorPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	switch (InstanceData.Phase)
	{
	case EEchidnaEightMirrorPhase::PlusWave:
	{
		if (!AreMirrorsFinished(InstanceData.PlusMirrors))
		{
			return EStateTreeRunStatus::Running;
		}

		// "+" 4개 다 끝남 — 미리 스폰해둔 "X" 4개를 이제서야 Activate
		AController* BossController = InstanceData.Boss->GetController();
		for (const TObjectPtr<AEchidnaMirrorActor>& Mirror : InstanceData.CrossMirrors)
		{
			if (Mirror)
			{
				Mirror->Activate(InstanceData.Damage, BossController);
			}
		}
		InstanceData.Phase = EEchidnaEightMirrorPhase::CrossWave;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaEightMirrorPhase::CrossWave:
	{
		if (!AreMirrorsFinished(InstanceData.CrossMirrors))
		{
			return EStateTreeRunStatus::Running;
		}

		// 두 대형 다 끝남 — 유도 거울에게 마지막 사이클을 끝으로 반복을 멈추라고 알림
		if (InstanceData.GuidedMirror)
		{
			InstanceData.GuidedMirror->StopRepeating();
		}
		InstanceData.Phase = EEchidnaEightMirrorPhase::Done;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaEightMirrorPhase::Done:
	{
		// 유도 거울이 마지막 사이클(추적 또는 발사)을 마저 끝낼 때까지 대기
		if (InstanceData.GuidedMirror && !InstanceData.GuidedMirror->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}
		return EStateTreeRunStatus::Succeeded;
	}
	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaEightMirrorPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// State가 중간에 다른 이유로(디버그 강제 전이 등) 끝나더라도 유도 거울이 무한 반복 상태로 남지 않도록 정지 요청
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.GuidedMirror)
	{
		InstanceData.GuidedMirror->StopRepeating();
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaEightMirrorPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaEightMirrorPatternDesc", "<b>Echidna Eight Mirror Pattern</b>");
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

	// 그 순간의 플레이어 위치가 아니라 장판이 실제로 뻗어나가는 고정 기준선(BaseAimRotation)의 정반대 방향으로
	// 물러나야 장판과 일직선이 됨 — 플레이어 위치 기준으로 계산하면 플레이어가 옆으로 이동해 있을 때
	// 장판 방향과 다른 대각선으로 튀어서 "장판이랑 따로 노는" 것처럼 보였음
	FVector AwayFromZone = -InstanceData.BaseAimRotation.Vector();
	AwayFromZone.Z = 0.f;
	if (AwayFromZone.IsNearlyZero())
	{
		AwayFromZone = -InstanceData.Boss->GetActorForwardVector();
	}
	else
	{
		AwayFromZone = AwayFromZone.GetSafeNormal();
	}

	// 착지 예상 지점에 바닥이 없으면(맵 끝자락) 홉 자체를 취소 — 낙사 방지
	const FVector LandingPoint = InstanceData.Boss->GetActorLocation() + AwayFromZone * InstanceData.HopCheckDistance;
	if (!HasGroundBelow(InstanceData, LandingPoint))
	{
		UE_LOG(LogLoA, Log, TEXT("[EchidnaRetreatFan] 후방 홉 취소 — 착지 예상 지점에 바닥 없음: %s"), *LandingPoint.ToString());
		return;
	}

	const FVector LaunchVelocity = AwayFromZone * InstanceData.HopBackStrength + FVector(0.f, 0.f, InstanceData.HopUpwardStrength);
	InstanceData.Boss->LaunchCharacter(LaunchVelocity, true, true);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaRetreatFan] 후방 홉 — Loc=%s Velocity=%s"),
		*InstanceData.Boss->GetActorLocation().ToString(), *LaunchVelocity.ToString());
}

FRotator FStateTreeTask_EchidnaRetreatFanPattern::ComputeAimRotation(const AEchidnaBoss* Boss) const
{
	// 패턴 시작 순간의 플레이어 방향을 한 번만 계산해서 고정 기준선으로 씀(플레이어 없으면 보스 정면 fallback).
	// 1번/2번 장판 모두 이 값 + FanYawOffset만 쓰고 다시 조준하지 않으므로, 두 장판의 방향 차이는 항상
	// 2*FanYawOffset(고정값)로 일정함 — "2번이 플레이어를 다시 보면서 크게 틀어지는" 것처럼 보였던 원인은
	// 이 조준 계산이 아니라 후방 홉(LaunchCharacter) 중 보스 캐릭터무브먼트가 이동 방향으로 자동 회전하던
	// 것이었음(HopBackward 참고, bOrientRotationToMovement 비활성화로 별도 수정)
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

	// 1번/2번 모두 EnterState에서 한 번만 고정해둔 BaseSpawnLocation을 그대로 씀 — 매번 Boss->GetActorLocation()을
	// 쓰면 1번 발동 순간 보스가 후방으로 홉해버려서 2번은 완전히 다른 지점에서 스폰되고, 결과적으로 두 부채꼴의
	// 원점이 달라져 "각도가 심하게 어긋나 보이는" 문제가 있었음. 보스 자신은 계속 뒤로 물러나도 장판 원점은 고정
	const FVector SpawnLocation = InstanceData.BaseSpawnLocation;

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

	// 장판 원점도 패턴 시작 시점의 보스 위치로 한 번만 고정 — 1번 발동 후 보스가 후방으로 홉해도
	// 2번 장판은 계속 이 지점에서 스폰됨(보스 자신의 위치와는 별개)
	InstanceData.BaseSpawnLocation = InstanceData.Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
	{
		InstanceData.BaseSpawnLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

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

void FStateTreeTask_EchidnaDragFanPattern::SpawnTethers(FInstanceDataType& InstanceData) const
{
	InstanceData.SpawnedTethers.Reset();

	if (!InstanceData.Boss || !InstanceData.TetherClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaDragFan] TetherClass 미할당 — 줄기 단계 생략"));
		return;
	}

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	// Boss->GetActorLocation()은 캡슐 중심 기준이라 그대로 쓰면 장판이 공중에 뜬 것처럼 보임
	FVector SpawnLocation = InstanceData.Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
	{
		SpawnLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	AController* BossController = InstanceData.Boss->GetController();
	const FVector PullTarget = InstanceData.Boss->GetActorLocation();

	// BaseAimRotation(정면) 기준으로 TetherFanAngle 범위 안에 TetherCount개를 균등 분포 —
	// 나머지(360 - TetherFanAngle)는 아무 줄기도 없는 안전지대로 남음
	const int32 Count = FMath::Max(InstanceData.TetherCount, 1);
	const float HalfAngle = InstanceData.TetherFanAngle * 0.5f;

	for (int32 i = 0; i < Count; i++)
	{
		const float T = (Count > 1) ? ((float)i / (float)(Count - 1)) : 0.5f;
		const float YawOffset = FMath::Lerp(-HalfAngle, HalfAngle, T);

		FRotator SpawnRotation = InstanceData.BaseAimRotation;
		SpawnRotation.Yaw += YawOffset;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = InstanceData.Boss;
		AEchidnaTetherActor* Tether = World->SpawnActor<AEchidnaTetherActor>(
			InstanceData.TetherClass, SpawnLocation, SpawnRotation, SpawnParams);

		if (Tether)
		{
			Tether->Activate(PullTarget, InstanceData.PullStrength, BossController);
			InstanceData.SpawnedTethers.Add(Tether);
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[EchidnaDragFan] Tether SpawnActor 실패 (YawOffset=%.0f)"), YawOffset);
		}
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaDragFan] 줄기 %d개 스폰 완료 (BossLoc=%s)"),
		InstanceData.SpawnedTethers.Num(), *SpawnLocation.ToString());
}

bool FStateTreeTask_EchidnaDragFanPattern::AreTethersFinished(const FInstanceDataType& InstanceData) const
{
	for (const TObjectPtr<AEchidnaTetherActor>& Tether : InstanceData.SpawnedTethers)
	{
		if (Tether && !Tether->IsFinished())
		{
			return false;
		}
	}
	return true;
}

bool FStateTreeTask_EchidnaDragFanPattern::AnyTetherHit(const FInstanceDataType& InstanceData) const
{
	for (const TObjectPtr<AEchidnaTetherActor>& Tether : InstanceData.SpawnedTethers)
	{
		if (Tether && Tether->DidHit())
		{
			return true;
		}
	}
	return false;
}

FRotator FStateTreeTask_EchidnaDragFanPattern::ComputeAimRotation(const AEchidnaBoss* Boss) const
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

AEchidnaFanZoneActor* FStateTreeTask_EchidnaDragFanPattern::SpawnFan(FInstanceDataType& InstanceData, float YawOffsetDeg, float FanAngleOverride) const
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

	// Boss->GetActorLocation()은 캡슐 중심 기준이라 그대로 쓰면 장판이 공중에 뜬 것처럼 보임 —
	// 캡슐 절반 높이를 빼서 발밑(지면) 높이로 보정
	FVector SpawnLocation = InstanceData.Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
	{
		SpawnLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	// 매 캐스팅마다 플레이어 위치를 다시 조준하면 1번·2번의 기준선이 어긋나므로,
	// EnterState에서 한 번만 정한 BaseAimRotation을 그대로 쓰고 좌/우 각도(YawOffsetDeg)만 더한다
	FRotator SpawnRotation = InstanceData.BaseAimRotation;
	SpawnRotation.Yaw += YawOffsetDeg;

	InstanceData.Boss->SetActorRotation(FRotator(0.f, SpawnRotation.Yaw, 0.f));

	AController* BossController = InstanceData.Boss->GetController();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaFanZoneActor* Fan = World->SpawnActor<AEchidnaFanZoneActor>(
		InstanceData.FanZoneClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (Fan)
	{
		// 1번/2번 장판마다 폭이 다르므로(FirstFanAngle/SecondFanAngle) FanZoneClass 기본값을 여기서 덮어씀 —
		// Activate()가 이 값을 읽어 메시를 만들기 전에 설정해야 함
		if (FanAngleOverride > 0.f)
		{
			Fan->FanAngle = FanAngleOverride;
		}
		Fan->Activate(InstanceData.Damage, BossController);
	}
	else
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaDragFan] SpawnActor 실패"));
	}

	return Fan;
}

EStateTreeRunStatus FStateTreeTask_EchidnaDragFanPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentFan = nullptr;
	InstanceData.SpawnedTethers.Reset();

	if (!InstanceData.Boss || !InstanceData.FanZoneClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaDragFan] EnterState 실패 — Boss=%s FanZoneClass=%s (StateTree에서 Context Actor 바인딩/FanZoneClass 할당을 확인하세요)"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.FanZoneClass ? *InstanceData.FanZoneClass->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 패턴 시작 순간의 조준 방향을 한 번만 고정 — 줄기/1번/2번 캐스팅 모두 이 기준선에 각도만 더해서 씀
	InstanceData.BaseAimRotation = ComputeAimRotation(InstanceData.Boss);

	// 보스가 조준 방향을 보도록 회전 (레퍼런스의 "턴 동작")
	InstanceData.Boss->SetActorRotation(FRotator(0.f, InstanceData.BaseAimRotation.Yaw, 0.f));

	// 1단계 — 부채꼴로 줄기를 동시에 뻗어 맞은 대상을 끌어당김
	SpawnTethers(InstanceData);

	InstanceData.Phase = EEchidnaDragFanPhase::Tethering;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaDragFanPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	switch (InstanceData.Phase)
	{
	case EEchidnaDragFanPhase::Tethering:
	{
		if (!AreTethersFinished(InstanceData))
		{
			return EStateTreeRunStatus::Running;
		}

		// 줄기에 아무도 안 끌려왔어도 1번·2번 장판은 그대로 순서대로 터진다 (예전엔 여기서 패턴을 끝냈음)
		if (!AnyTetherHit(InstanceData))
		{
			UE_LOG(LogLoA, Log, TEXT("[EchidnaDragFan] 줄기에 맞은 대상 없음 — 장판은 그대로 진행"));
		}

		// 2단계 — 1번째 장판 (좁게, FirstFanAngle)
		InstanceData.CurrentFan = SpawnFan(InstanceData, InstanceData.FirstYawOffset, InstanceData.FirstFanAngle);
		if (!InstanceData.CurrentFan)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.Phase = EEchidnaDragFanPhase::Casting1;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDragFanPhase::Casting1:
	{
		if (!InstanceData.CurrentFan || !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		// 2번째 장판 — 1번보다 넓게(SecondFanAngle), 살짝만 겹치도록 SecondYawOffset으로 배치
		InstanceData.CurrentFan = SpawnFan(InstanceData, InstanceData.SecondYawOffset, InstanceData.SecondFanAngle);
		if (!InstanceData.CurrentFan)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.Phase = EEchidnaDragFanPhase::Casting2;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDragFanPhase::Casting2:
	{
		if (!InstanceData.CurrentFan || !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.Phase = EEchidnaDragFanPhase::Done;
		return EStateTreeRunStatus::Succeeded;
	}
	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaDragFanPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 줄기에 맞아 끌려온 대상은 장판 2개가 다 터질 때까지 묶여 있다가 여기서 풀린다.
	// 패턴이 중간에 강제 전이되어도 반드시 풀리도록 Succeeded/Failed 구분 없이 ExitState에서 처리
	if (InstanceData.Boss)
	{
		if (ALoACharacter* PlayerChar = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(InstanceData.Boss->GetWorld(), 0)))
		{
			PlayerChar->ReleasePull();
		}
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaDragFanPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaDragFanPatternDesc", "<b>Echidna Drag Fan Pattern</b>");
}
#endif // WITH_EDITOR

FRotator FStateTreeTask_EchidnaDonutSlashPattern::ComputeAimRotation(const AEchidnaBoss* Boss) const
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

AEchidnaFanZoneActor* FStateTreeTask_EchidnaDonutSlashPattern::SpawnZone(FInstanceDataType& InstanceData, TSubclassOf<AEchidnaFanZoneActor> ZoneClass,
	float YawOffsetDeg, float FanAngleOverride, float InnerRadiusOverride, float OuterRadiusOverride,
	float TelegraphDurationOverride, int32 RingCountOverride) const
{
	if (!InstanceData.Boss || !ZoneClass)
	{
		return nullptr;
	}

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 슬래시(1·2번)는 정면 기준 좌/우로 틀어서, 도넛(3·4번)은 YawOffsetDeg=0(FanAngle=360이라 방향 무의미)으로 스폰
	FRotator SpawnRotation = InstanceData.BaseAimRotation;
	SpawnRotation.Yaw += YawOffsetDeg;

	AController* BossController = InstanceData.Boss->GetController();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaFanZoneActor* Fan = World->SpawnActor<AEchidnaFanZoneActor>(
		ZoneClass, InstanceData.BaseSpawnLocation, SpawnRotation, SpawnParams);

	if (Fan)
	{
		// 슬래시/작은 도넛/외곽 도넛마다 각도·반지름이 전부 다르므로 매번 명시적으로 덮어씀 —
		// Activate()가 이 값을 읽어 메시를 만들기 전에 설정해야 함
		Fan->FanAngle = FanAngleOverride;
		Fan->FanInnerRadius = InnerRadiusOverride;
		Fan->FanRange = OuterRadiusOverride;

		// 예고시간은 호출부마다 다르게 넘어오므로 명시적으로 덮어씀 (음수면 BP 기본값 유지).
		// RingCount는 이 패턴의 모든 스폰에서 항상 강제로 1로 덮어써 "예고 후 단발 판정"만 나오게 함 —
		// BP 기본 RingCount가 몇이든(뒤로 빠지며 좌우장판 등에서 쓰는 계단식 확장 값) 무시됨
		if (TelegraphDurationOverride >= 0.f)
		{
			Fan->TelegraphDuration = TelegraphDurationOverride;
		}
		Fan->RingCount = RingCountOverride;

		Fan->Activate(InstanceData.Damage, BossController);
	}
	else
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaDonutSlash] SpawnActor 실패 (Class=%s)"), *GetNameSafe(ZoneClass));
	}

	return Fan;
}

void FStateTreeTask_EchidnaDonutSlashPattern::EndRiseFall(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss) return;

	// 상승 중 Flying으로 바꿔둔 무브먼트모드를 원래대로(Walking) 복구하고 Z를 정확히 지상 높이로 스냅
	if (UCharacterMovementComponent* Movement = InstanceData.Boss->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	FVector Loc = InstanceData.Boss->GetActorLocation();
	Loc.Z = InstanceData.GroundActorZ;
	InstanceData.Boss->SetActorLocation(Loc);
}

EStateTreeRunStatus FStateTreeTask_EchidnaDonutSlashPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentFan = nullptr;
	InstanceData.InnerDonut = nullptr;
	InstanceData.OuterDonut = nullptr;
	InstanceData.PhaseElapsed = 0.f;

	if (!InstanceData.Boss || !InstanceData.SlashZoneClass || !InstanceData.FanZoneClass || !InstanceData.OuterDonutClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaDonutSlash] EnterState 실패 — Boss=%s SlashZoneClass=%s FanZoneClass=%s OuterDonutClass=%s (StateTree에서 Context Actor 바인딩/클래스 할당을 확인하세요)"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.SlashZoneClass ? *InstanceData.SlashZoneClass->GetName() : TEXT("NULL"),
			InstanceData.FanZoneClass ? *InstanceData.FanZoneClass->GetName() : TEXT("NULL"),
			InstanceData.OuterDonutClass ? *InstanceData.OuterDonutClass->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 패턴 시작 순간의 조준 방향을 한 번만 고정 — 1번/2번 슬래시 모두 이 기준선에 좌/우 각도만 더해서 씀
	InstanceData.BaseAimRotation = ComputeAimRotation(InstanceData.Boss);
	InstanceData.Boss->SetActorRotation(FRotator(0.f, InstanceData.BaseAimRotation.Yaw, 0.f));

	// Boss->GetActorLocation()은 캡슐 중심 기준이라 그대로 쓰면 장판이 공중에 뜬 것처럼 보임
	InstanceData.BaseSpawnLocation = InstanceData.Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
	{
		InstanceData.BaseSpawnLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	// 1번 — 우측 대각 슬래시 (중심이 아니라 호 끝부분만 때리는 얇은 부채꼴 고리, 예고 후 단발 판정)
	InstanceData.CurrentFan = SpawnZone(InstanceData, InstanceData.SlashZoneClass, InstanceData.Slash1YawOffset,
		InstanceData.SlashFanAngle, InstanceData.SlashInnerRadius, InstanceData.SlashRange,
		InstanceData.SlashTelegraphDuration, 1);
	if (!InstanceData.CurrentFan)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Phase = EEchidnaDonutSlashPhase::Slash1;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaDonutSlashPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 1~4번 전 구간 동안 보스가 한 방향(BaseAimRotation)만 계속 바라보도록 매 틱 강제 — 슬래시 1·2번의 공격 방향
	// 자체는 서로 다르게 틀어지지만(Slash1/2YawOffset), 보스 모델 자체는 패턴 시작 시점에 고정한 방향에서 회전하지 않음
	InstanceData.Boss->SetActorRotation(FRotator(0.f, InstanceData.BaseAimRotation.Yaw, 0.f));

	switch (InstanceData.Phase)
	{
	case EEchidnaDonutSlashPhase::Slash1:
	{
		if (!InstanceData.CurrentFan || !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		// 2번 — 좌측 대각 슬래시 (1번과 동일하게 호 끝부분만)
		InstanceData.CurrentFan = SpawnZone(InstanceData, InstanceData.SlashZoneClass, InstanceData.Slash2YawOffset,
			InstanceData.SlashFanAngle, InstanceData.SlashInnerRadius, InstanceData.SlashRange,
			InstanceData.SlashTelegraphDuration, 1);
		if (!InstanceData.CurrentFan)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.Phase = EEchidnaDonutSlashPhase::Slash2;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDonutSlashPhase::Slash2:
	{
		if (!InstanceData.CurrentFan || !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		// 3단계 시작 — 작은 도넛(3번) 예고 스폰 (FanAngle=360 오버라이드로 원형 고리가 됨). 보스는 아직 지상에
		// 그대로 둠 — "예고 표시 → 폭발 → 그제서야 상승"이어야 하므로 여기서는 Flying 전환/상승을 시작하지 않음
		InstanceData.InnerDonut = SpawnZone(InstanceData, InstanceData.FanZoneClass, 0.f, 360.f,
			InstanceData.InnerDonutInnerRadius, InstanceData.InnerDonutOuterRadius,
			InstanceData.InnerDonutTelegraphDuration, 1);
		if (!InstanceData.InnerDonut)
		{
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.Phase = EEchidnaDonutSlashPhase::InnerDonutTelegraph;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDonutSlashPhase::InnerDonutTelegraph:
	{
		// 예고 후 단발 판정(IsFinished())이 실제로 끝날 때까지는 보스가 계속 지상에 그대로 있음
		if (!InstanceData.InnerDonut || !InstanceData.InnerDonut->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		// 폭발이 끝난 바로 이 시점부터 상승 시작
		InstanceData.GroundActorZ = InstanceData.Boss->GetActorLocation().Z;
		InstanceData.PhaseElapsed = 0.f;
		if (UCharacterMovementComponent* Movement = InstanceData.Boss->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Flying);
			Movement->StopMovementImmediately();
		}

		InstanceData.Phase = EEchidnaDonutSlashPhase::Rising;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDonutSlashPhase::Rising:
	{
		InstanceData.PhaseElapsed += DeltaTime;

		// 상승 구간(0~RiseDuration) 동안만 Z를 끌어올리고, 그 이후(정점 대기 구간)엔 RiseHeight로 고정
		const float RiseAlpha = (InstanceData.RiseDuration > 0.f)
			? FMath::Clamp(InstanceData.PhaseElapsed / InstanceData.RiseDuration, 0.f, 1.f) : 1.f;
		FVector Loc = InstanceData.Boss->GetActorLocation();
		Loc.Z = InstanceData.GroundActorZ + FMath::Lerp(0.f, InstanceData.RiseHeight, RiseAlpha);
		InstanceData.Boss->SetActorLocation(Loc);

		if (InstanceData.PhaseElapsed < (InstanceData.RiseDuration + InstanceData.ApexHoldDuration))
		{
			return EStateTreeRunStatus::Running;
		}

		// 4단계 시작 — 외곽 도넛(4번) 예고 스폰(하강과 동시에 보여짐) + 하강 시작, 예고 후 단발 판정
		InstanceData.OuterDonut = SpawnZone(InstanceData, InstanceData.OuterDonutClass, 0.f, 360.f,
			InstanceData.OuterDonutInnerRadius, InstanceData.OuterDonutOuterRadius,
			InstanceData.OuterDonutTelegraphDuration, 1);
		if (!InstanceData.OuterDonut)
		{
			EndRiseFall(InstanceData);
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.PhaseElapsed = 0.f;
		InstanceData.Phase = EEchidnaDonutSlashPhase::FallAndOuterDonut;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDonutSlashPhase::FallAndOuterDonut:
	{
		InstanceData.PhaseElapsed += DeltaTime;

		const float FallAlpha = (InstanceData.FallDuration > 0.f)
			? FMath::Clamp(InstanceData.PhaseElapsed / InstanceData.FallDuration, 0.f, 1.f) : 1.f;

		// 착지 완료(FallAlpha>=1) 전까지만 Z를 직접 보간 — 이후엔 EndRiseFall이 Walking으로 복구하면서
		// 캐릭터무브먼트가 정상적으로 바닥을 다시 감지하게 둔다
		if (FallAlpha < 1.f)
		{
			FVector Loc = InstanceData.Boss->GetActorLocation();
			Loc.Z = InstanceData.GroundActorZ + FMath::Lerp(InstanceData.RiseHeight, 0.f, FallAlpha);
			InstanceData.Boss->SetActorLocation(Loc);
		}
		else if (InstanceData.Boss->GetCharacterMovement() && InstanceData.Boss->GetCharacterMovement()->MovementMode == MOVE_Flying)
		{
			EndRiseFall(InstanceData);
		}

		if (!InstanceData.OuterDonut || !InstanceData.OuterDonut->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.Phase = EEchidnaDonutSlashPhase::Done;
		return EStateTreeRunStatus::Succeeded;
	}
	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaDonutSlashPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 패턴이 도중에 끊겨도(예: 다른 State로 강제 전이) 보스가 공중에 뜬 채로 남지 않도록 안전 복구
	if (InstanceData.Boss && InstanceData.Boss->GetCharacterMovement() &&
		InstanceData.Boss->GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		EndRiseFall(InstanceData);
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaDonutSlashPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaDonutSlashPatternDesc", "<b>Echidna Donut Slash Pattern</b>");
}
#endif // WITH_EDITOR

void FStateTreeTask_EchidnaHeartBurstPattern::FireRandomWave(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss || !InstanceData.HeartClass)
	{
		return;
	}

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World)
	{
		return;
	}

	const int32 MinCount = FMath::Max(InstanceData.MinHeartsPerWave, 1);
	const int32 MaxCount = FMath::Max(InstanceData.MaxHeartsPerWave, MinCount);
	const int32 Count = FMath::RandRange(MinCount, MaxCount);

	AController* BossController = InstanceData.Boss->GetController();

	for (int32 i = 0; i < Count; i++)
	{
		// 정해진 8방향이 아니라 0~360도 전방향 중 매번 완전히 랜덤으로 방향을 정함
		const float AngleDeg = FMath::FRandRange(0.f, 360.f);
		const FVector Direction = FRotator(0.f, AngleDeg, 0.f).Vector();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = InstanceData.Boss;
		AEchidnaHeartActor* Heart = World->SpawnActor<AEchidnaHeartActor>(
			InstanceData.HeartClass, InstanceData.FireSpawnLocation, Direction.Rotation(), SpawnParams);

		if (Heart)
		{
			// 전방향 하트발사는 매혹 스택 대신 오염 장판 게이지
			Heart->SetSpawnPoopMarkOnHit(true);
			Heart->Launch(Direction, BossController, InstanceData.HeartSpeed, InstanceData.HeartDamage,
				InstanceData.HeartStunDuration, 0);
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[EchidnaHeartBurst] Heart SpawnActor 실패 (AngleDeg=%.0f)"), AngleDeg);
		}
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaHeartBurstPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TelegraphHeart = nullptr;
	InstanceData.PhaseElapsed = 0.f;
	InstanceData.FireIntervalElapsed = 0.f;

	if (!InstanceData.Boss || !InstanceData.HeartClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaHeartBurst] EnterState 실패 — Boss=%s HeartClass=%s (StateTree에서 Context Actor 바인딩/클래스 할당을 확인하세요)"),
			InstanceData.Boss ? TEXT("Valid") : TEXT("NULL"),
			InstanceData.HeartClass ? *InstanceData.HeartClass->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 패턴 시작 시 잔여 이동(패트롤 등)을 멈춤 — 이 패턴 자체는 보스를 움직이지 않음
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 예고 마커는 머리 위에 표시되도록 캡슐 중심에서 위로 절반 높이만큼 더 올림 (다른 패턴들의 "발밑 보정"과 반대 방향)
	InstanceData.SpawnLocation = InstanceData.Boss->GetActorLocation();

	// 실제 발사되는 하트는 플레이어 캡슐과 충돌 가능한 높이(지면 기준 HeartFireHeight)에서 나가야 함 —
	// 머리 위 높이 그대로 쏘면 플레이어 위를 그냥 지나쳐서 안 맞았음
	InstanceData.FireSpawnLocation = InstanceData.Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
	{
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		InstanceData.SpawnLocation.Z += HalfHeight;
		InstanceData.FireSpawnLocation.Z = InstanceData.FireSpawnLocation.Z - HalfHeight + InstanceData.HeartFireHeight;
	}

	// 예고 — Launch를 호출하지 않고 스폰해서 제자리에 가만히 떠 있는 마커로 재사용
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	InstanceData.TelegraphHeart = World->SpawnActor<AEchidnaHeartActor>(
		InstanceData.HeartClass, InstanceData.SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	InstanceData.Phase = EEchidnaHeartBurstPhase::Telegraph;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaHeartBurstPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	switch (InstanceData.Phase)
	{
	case EEchidnaHeartBurstPhase::Telegraph:
	{
		InstanceData.PhaseElapsed += DeltaTime;
		if (InstanceData.PhaseElapsed < InstanceData.TelegraphDuration)
		{
			return EStateTreeRunStatus::Running;
		}

		if (InstanceData.TelegraphHeart)
		{
			InstanceData.TelegraphHeart->Destroy();
			InstanceData.TelegraphHeart = nullptr;
		}

		InstanceData.PhaseElapsed = 0.f;
		InstanceData.FireIntervalElapsed = InstanceData.FireInterval; // 첫 웨이브가 곧바로 나가도록
		InstanceData.Phase = EEchidnaHeartBurstPhase::Firing;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaHeartBurstPhase::Firing:
	{
		InstanceData.PhaseElapsed += DeltaTime;
		InstanceData.FireIntervalElapsed += DeltaTime;

		if (InstanceData.FireIntervalElapsed >= InstanceData.FireInterval)
		{
			InstanceData.FireIntervalElapsed -= InstanceData.FireInterval;
			FireRandomWave(InstanceData);
		}

		if (InstanceData.PhaseElapsed >= InstanceData.FireDuration)
		{
			InstanceData.Phase = EEchidnaHeartBurstPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}

		return EStateTreeRunStatus::Running;
	}
	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaHeartBurstPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 패턴이 예고 도중 끊겨도 마커가 남지 않도록 정리
	if (InstanceData.TelegraphHeart)
	{
		InstanceData.TelegraphHeart->Destroy();
		InstanceData.TelegraphHeart = nullptr;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaHeartBurstPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaHeartBurstPatternDesc", "<b>Echidna Heart Burst Pattern</b>");
}
#endif // WITH_EDITOR

// ─── 백스탭 후 하트발사 ───────────────────────────────────────────────────────

FRotator FStateTreeTask_EchidnaBackstepHeartPattern::FacePlayer(AEchidnaBoss* Boss) const
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

	// 보스 모델도 같은 방향으로 즉시 돌린다 — 뒤로 밀려나는 동안에도 정면이 플레이어를 향해야
	// 레퍼런스처럼 "물러나면서 앞으로 쏘는" 모양이 된다
	const FRotator YawOnly(0.f, AimRotation.Yaw, 0.f);
	Boss->SetActorRotation(YawOnly);
	return YawOnly;
}

bool FStateTreeTask_EchidnaBackstepHeartPattern::HasGroundBelow(const FInstanceDataType& InstanceData, const FVector& Location) const
{
	UWorld* World = InstanceData.Boss ? InstanceData.Boss->GetWorld() : nullptr;
	if (!World) return false;

	FHitResult Hit;
	const FVector TraceStart = Location + FVector(0.f, 0.f, 200.f);
	const FVector TraceEnd = Location - FVector(0.f, 0.f, 500.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(InstanceData.Boss);

	return World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

void FStateTreeTask_EchidnaBackstepHeartPattern::Backstep(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss) return;

	// FacePlayer로 이미 플레이어를 보고 있으므로 "정면의 반대"가 곧 플레이어 반대 방향
	FVector Backward = -InstanceData.Boss->GetActorForwardVector();
	Backward.Z = 0.f;
	if (Backward.IsNearlyZero()) return;
	Backward = Backward.GetSafeNormal();

	// 착지 예상 지점에 바닥이 없으면 백스텝을 건너뛴다 — 맵 끝자락에서 떨어지지 않도록 (RetreatFan과 동일)
	const FVector LandingPoint = InstanceData.Boss->GetActorLocation() + Backward * InstanceData.BackstepCheckDistance;
	if (!HasGroundBelow(InstanceData, LandingPoint))
	{
		UE_LOG(LogLoA, Log, TEXT("[EchidnaBackstepHeart] 백스텝 취소 — 착지 예상 지점에 바닥 없음: %s"), *LandingPoint.ToString());
		return;
	}

	const FVector LaunchVelocity = Backward * InstanceData.BackstepStrength
		+ FVector(0.f, 0.f, InstanceData.BackstepUpwardStrength);
	InstanceData.Boss->LaunchCharacter(LaunchVelocity, true, true);
}

void FStateTreeTask_EchidnaBackstepHeartPattern::FireFan(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss || !InstanceData.HeartClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	const int32 Count = FMath::Max(InstanceData.HeartCount, 1);
	AController* BossController = InstanceData.Boss->GetController();

	// GetActorLocation은 캡슐 "중심"이라 그대로 쓰면 하트가 떠 보인다 — 발밑으로 내린 뒤 발사 높이를 더한다
	FVector Origin = InstanceData.Boss->GetActorLocation();
	Origin.Z -= InstanceData.Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Origin.Z += InstanceData.HeartFireHeight;

	const float HalfSpread = InstanceData.FanSpreadAngle * 0.5f;

	for (int32 i = 0; i < Count; i++)
	{
		// 1개면 정면 하나, 2개 이상이면 [-절반, +절반] 구간에 균등 분포 (양 끝이 항상 부채꼴 경계)
		const float YawOffset = (Count > 1)
			? FMath::Lerp(-HalfSpread, HalfSpread, static_cast<float>(i) / static_cast<float>(Count - 1))
			: 0.f;

		const FRotator HeartRotation(0.f, InstanceData.BaseAimRotation.Yaw + YawOffset, 0.f);
		const FVector Direction = HeartRotation.Vector();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = InstanceData.Boss;
		AEchidnaHeartActor* Heart = World->SpawnActor<AEchidnaHeartActor>(
			InstanceData.HeartClass, Origin, HeartRotation, SpawnParams);

		if (Heart)
		{
			Heart->Launch(Direction, BossController, InstanceData.HeartSpeed, InstanceData.HeartDamage,
				InstanceData.HeartStunDuration, InstanceData.HeartCharmGaugeAmount);
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[EchidnaBackstepHeart] Heart SpawnActor 실패 (YawOffset=%.1f)"), YawOffset);
		}
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaBackstepHeartPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaBackstepHeart] Boss 미바인딩 — 패턴 실패"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.HeartClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaBackstepHeart] HeartClass 미할당 — 패턴 실패"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.BaseAimRotation = FacePlayer(InstanceData.Boss);
	Backstep(InstanceData);

	InstanceData.Phase = EEchidnaBackstepHeartPhase::Backstep;
	InstanceData.PhaseElapsed = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaBackstepHeartPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PhaseElapsed += DeltaTime;

	switch (InstanceData.Phase)
	{
	case EEchidnaBackstepHeartPhase::Backstep:
	{
		// 밀려나는 동안에도 계속 플레이어를 바라본다 — 발사 직전의 이 값이 그대로 부채꼴 기준선이 된다
		InstanceData.BaseAimRotation = FacePlayer(InstanceData.Boss);

		if (InstanceData.PhaseElapsed >= InstanceData.BackstepDuration)
		{
			FireFan(InstanceData);
			InstanceData.Phase = EEchidnaBackstepHeartPhase::Fire;
			InstanceData.PhaseElapsed = 0.f;
		}
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaBackstepHeartPhase::Fire:
	{
		if (InstanceData.PhaseElapsed >= InstanceData.FireLingerDuration)
		{
			InstanceData.Phase = EEchidnaBackstepHeartPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;
	}

	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaBackstepHeartPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaBackstepHeartPatternDesc", "<b>Echidna Backstep Heart Pattern</b>");
}
#endif // WITH_EDITOR

// ─── 되돌아오는 구체 ─────────────────────────────────────────────────────────

FRotator FStateTreeTask_EchidnaReturningOrbPattern::FacePlayer(AEchidnaBoss* Boss) const
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

	const FRotator YawOnly(0.f, AimRotation.Yaw, 0.f);
	Boss->SetActorRotation(YawOnly);
	return YawOnly;
}

bool FStateTreeTask_EchidnaReturningOrbPattern::HasGroundBelow(const FInstanceDataType& InstanceData, const FVector& Location) const
{
	UWorld* World = InstanceData.Boss ? InstanceData.Boss->GetWorld() : nullptr;
	if (!World) return false;

	FHitResult Hit;
	const FVector TraceStart = Location + FVector(0.f, 0.f, 200.f);
	const FVector TraceEnd = Location - FVector(0.f, 0.f, 500.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(InstanceData.Boss);

	return World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

void FStateTreeTask_EchidnaReturningOrbPattern::Backstep(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss) return;

	FVector Backward = -InstanceData.Boss->GetActorForwardVector();
	Backward.Z = 0.f;
	if (Backward.IsNearlyZero()) return;
	Backward = Backward.GetSafeNormal();

	const FVector LandingPoint = InstanceData.Boss->GetActorLocation() + Backward * InstanceData.BackstepCheckDistance;
	if (!HasGroundBelow(InstanceData, LandingPoint))
	{
		UE_LOG(LogLoA, Log, TEXT("[EchidnaReturningOrb] 백스텝 취소 — 착지 예상 지점에 바닥 없음: %s"), *LandingPoint.ToString());
		return;
	}

	const FVector LaunchVelocity = Backward * InstanceData.BackstepStrength
		+ FVector(0.f, 0.f, InstanceData.BackstepUpwardStrength);
	InstanceData.Boss->LaunchCharacter(LaunchVelocity, true, true);
}

void FStateTreeTask_EchidnaReturningOrbPattern::ThrowOrb(FInstanceDataType& InstanceData, float YawDeg) const
{
	if (!InstanceData.Boss || !InstanceData.OrbClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	// GetActorLocation()은 캡슐 중심이라 발밑으로 내린 뒤 발사 높이를 더한다 —
	// 안 맞추면 구체가 플레이어 위를 그냥 지나간다(하트발사에서 이미 겪은 문제)
	FVector Origin = InstanceData.Boss->GetActorLocation();
	Origin.Z -= InstanceData.Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Origin.Z += InstanceData.OrbSpawnHeight;

	const FRotator OrbRotation(0.f, YawDeg, 0.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AEchidnaOrbActor* Orb = World->SpawnActor<AEchidnaOrbActor>(
		InstanceData.OrbClass, Origin, OrbRotation, SpawnParams))
	{
		// BeginPlay가 이미 BP 기본값을 적용한 뒤라, 오버라이드는 여기서 덮어써야 한다 (음수는 무시됨)
		Orb->ApplyOverrides(InstanceData.OrbVisualScaleOverride, InstanceData.OrbCollisionRadiusOverride,
			InstanceData.OrbSpeedOverride, InstanceData.OrbMaxRangeOverride);

		Orb->Launch(OrbRotation.Vector(), InstanceData.Boss->GetController(), InstanceData.OrbDamage);
		InstanceData.SpawnedOrbs.Add(Orb);
	}
	else
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaReturningOrb] Orb SpawnActor 실패 (Yaw=%.1f)"), YawDeg);
	}
}

void FStateTreeTask_EchidnaReturningOrbPattern::ThrowFan(FInstanceDataType& InstanceData) const
{
	const int32 Count = FMath::Max(InstanceData.FanOrbCount, 1);
	const float HalfSpread = InstanceData.FanSpreadAngle * 0.5f;

	for (int32 i = 0; i < Count; i++)
	{
		// 1개면 정면, 2개 이상이면 [-절반, +절반]에 균등 분포 (양 끝이 항상 부채꼴 경계)
		const float YawOffset = (Count > 1)
			? FMath::Lerp(-HalfSpread, HalfSpread, static_cast<float>(i) / static_cast<float>(Count - 1))
			: 0.f;

		ThrowOrb(InstanceData, InstanceData.BaseAimRotation.Yaw + YawOffset);
	}
}

bool FStateTreeTask_EchidnaReturningOrbPattern::AreOrbsFinished(const FInstanceDataType& InstanceData) const
{
	for (const TObjectPtr<AEchidnaOrbActor>& Orb : InstanceData.SpawnedOrbs)
	{
		if (Orb && !Orb->IsFinished())
		{
			return false;
		}
	}
	return true;
}

EStateTreeRunStatus FStateTreeTask_EchidnaReturningOrbPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SpawnedOrbs.Reset();

	if (!InstanceData.Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaReturningOrb] Boss 미바인딩 — 패턴 실패"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.OrbClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaReturningOrb] OrbClass 미할당 — 패턴 실패"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 1번째 — 그 시점의 플레이어 방향으로
	const FRotator Aim = FacePlayer(InstanceData.Boss);
	ThrowOrb(InstanceData, Aim.Yaw);

	InstanceData.Phase = EEchidnaReturningOrbPhase::Throw1;
	InstanceData.PhaseElapsed = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaReturningOrbPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PhaseElapsed += DeltaTime;

	switch (InstanceData.Phase)
	{
	case EEchidnaReturningOrbPhase::Throw1:
	{
		if (InstanceData.PhaseElapsed >= InstanceData.ThrowInterval)
		{
			// 2번째도 "그 시점의" 플레이어를 다시 조준 — 앞의 두 개는 플레이어를 쫓아간다
			const FRotator Aim = FacePlayer(InstanceData.Boss);
			ThrowOrb(InstanceData, Aim.Yaw);

			InstanceData.Phase = EEchidnaReturningOrbPhase::Throw2;
			InstanceData.PhaseElapsed = 0.f;
		}
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaReturningOrbPhase::Throw2:
	{
		if (InstanceData.PhaseElapsed >= InstanceData.ThrowInterval)
		{
			FacePlayer(InstanceData.Boss);
			Backstep(InstanceData);

			InstanceData.Phase = EEchidnaReturningOrbPhase::Backstep;
			InstanceData.PhaseElapsed = 0.f;
		}
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaReturningOrbPhase::Backstep:
	{
		// 밀려나는 동안에도 계속 플레이어를 바라본다 — 발사 직전 값이 그대로 부채꼴 기준선이 된다
		InstanceData.BaseAimRotation = FacePlayer(InstanceData.Boss);

		if (InstanceData.PhaseElapsed >= InstanceData.BackstepDuration)
		{
			ThrowFan(InstanceData);

			InstanceData.Phase = EEchidnaReturningOrbPhase::WaitReturn;
			InstanceData.PhaseElapsed = 0.f;
		}
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaReturningOrbPhase::WaitReturn:
	{
		// 지형에 끼어 영영 안 돌아오는 구체가 있어도 패턴이 멈추지 않도록 상한을 둔다
		if (AreOrbsFinished(InstanceData) || InstanceData.PhaseElapsed >= InstanceData.MaxWaitDuration)
		{
			InstanceData.Phase = EEchidnaReturningOrbPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;
	}

	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaReturningOrbPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaReturningOrbPatternDesc", "<b>Echidna Returning Orb Pattern</b>");
}
#endif // WITH_EDITOR

// ─── 정면 리본 공격 ──────────────────────────────────────────────────────────

FRotator FStateTreeTask_EchidnaRibbonPattern::FacePlayer(AEchidnaBoss* Boss) const
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

	const FRotator YawOnly(0.f, AimRotation.Yaw, 0.f);
	Boss->SetActorRotation(YawOnly);
	return YawOnly;
}

FVector FStateTreeTask_EchidnaRibbonPattern::GetBossFeetLocation(const AEchidnaBoss* Boss) const
{
	if (!Boss)
	{
		return FVector::ZeroVector;
	}

	// GetActorLocation()은 캡슐 중심이라 그대로 쓰면 장판/리본이 공중에 떠 보인다
	FVector Feet = Boss->GetActorLocation();
	Feet.Z -= Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return Feet;
}

void FStateTreeTask_EchidnaRibbonPattern::SpawnRibbons(FInstanceDataType& InstanceData) const
{
	InstanceData.SpawnedRibbons.Reset();

	if (!InstanceData.Boss || !InstanceData.RibbonClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	const FVector Origin = GetBossFeetLocation(InstanceData.Boss);
	AController* BossController = InstanceData.Boss->GetController();

	const int32 Count = FMath::Max(InstanceData.RibbonCount, 1);
	const float HalfSpread = InstanceData.RibbonSpreadAngle * 0.5f;

	float LongestRange = 0.f;

	for (int32 i = 0; i < Count; i++)
	{
		const float YawOffset = (Count > 1)
			? FMath::Lerp(-HalfSpread, HalfSpread, static_cast<float>(i) / static_cast<float>(Count - 1))
			: 0.f;

		const FRotator SpawnRotation(0.f, InstanceData.BaseAimRotation.Yaw + YawOffset, 0.f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = InstanceData.Boss;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AEchidnaTetherActor* Ribbon = World->SpawnActor<AEchidnaTetherActor>(
			InstanceData.RibbonClass, Origin, SpawnRotation, SpawnParams);

		if (Ribbon)
		{
			// Activate()가 이 값들로 표시 메시와 판정 박스를 만들므로 **반드시 그 전에** 덮어써야 한다
			// (음수면 BP 기본값 유지). BeginPlay가 아니라 Activate가 읽는 값이라 Orb처럼 별도 오버라이드
			// 함수가 필요 없다
			if (InstanceData.RibbonRangeOverride > 0.f)
			{
				Ribbon->TetherRange = InstanceData.RibbonRangeOverride;
			}
			if (InstanceData.RibbonHalfWidthOverride > 0.f)
			{
				Ribbon->TetherHalfWidth = InstanceData.RibbonHalfWidthOverride;
			}

			// PullTarget은 bApplyPullOnHit=false인 리본 설정에서는 쓰이지 않지만, 같은 액터를
			// 끌기 용도로도 쓸 수 있으므로 일단 보스 위치를 넘겨둔다
			Ribbon->Activate(InstanceData.Boss->GetActorLocation(), 0.f, BossController);
			InstanceData.SpawnedRibbons.Add(Ribbon);
			LongestRange = FMath::Max(LongestRange, Ribbon->TetherRange);
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[EchidnaRibbon] Ribbon SpawnActor 실패 (YawOffset=%.1f)"), YawOffset);
		}
	}

	// 1차가 빗나갔을 때 이동할 목표 — 리본이 실제로 뻗은 길이를 액터에서 읽어 계산한다
	InstanceData.RibbonEndLocation = Origin + InstanceData.BaseAimRotation.Vector() * LongestRange;
}

bool FStateTreeTask_EchidnaRibbonPattern::AreRibbonsFinished(const FInstanceDataType& InstanceData) const
{
	for (const TObjectPtr<AEchidnaTetherActor>& Ribbon : InstanceData.SpawnedRibbons)
	{
		if (Ribbon && !Ribbon->IsFinished())
		{
			return false;
		}
	}
	return true;
}

bool FStateTreeTask_EchidnaRibbonPattern::AnyRibbonHit(const FInstanceDataType& InstanceData) const
{
	for (const TObjectPtr<AEchidnaTetherActor>& Ribbon : InstanceData.SpawnedRibbons)
	{
		if (Ribbon && Ribbon->DidHit())
		{
			return true;
		}
	}
	return false;
}

void FStateTreeTask_EchidnaRibbonPattern::SpawnCircleZone(FInstanceDataType& InstanceData) const
{
	InstanceData.CurrentZone = nullptr;

	if (!InstanceData.Boss || !InstanceData.CircleZoneClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaFanZoneActor* Zone = World->SpawnActor<AEchidnaFanZoneActor>(
		InstanceData.CircleZoneClass, GetBossFeetLocation(InstanceData.Boss), FRotator::ZeroRotator, SpawnParams);

	if (!Zone)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRibbon] 원형 장판 SpawnActor 실패"));
		return;
	}

	// FanAngle=360이면 IsActorInRing의 각도 판정(반각 180도)이 항상 참이라 완전한 원이 된다 —
	// 별도 원형 액터 없이 부채꼴 액터를 그대로 쓰는 기존 방식(두번긋고 도넛장판과 동일)
	Zone->FanAngle = 360.f;
	Zone->FanInnerRadius = InstanceData.CircleInnerRadius;
	Zone->FanRange = InstanceData.CircleRadius;
	Zone->TelegraphDuration = InstanceData.CircleTelegraphDuration;
	Zone->RingCount = 1;	// 예고 후 단발 판정 (계단식 확장은 이 패턴에서 안 씀)

	Zone->Activate(InstanceData.Damage, InstanceData.Boss->GetController());
	InstanceData.CurrentZone = Zone;
}

void FStateTreeTask_EchidnaRibbonPattern::SpawnArcZone(FInstanceDataType& InstanceData) const
{
	InstanceData.CurrentZone = nullptr;

	if (!InstanceData.Boss || !InstanceData.ArcZoneClass) return;

	UWorld* World = InstanceData.Boss->GetWorld();
	if (!World) return;

	// 음수 Yaw가 좌측 — 레퍼런스의 "좌측 전방으로 내려침"
	FRotator SpawnRotation = InstanceData.BaseAimRotation;
	SpawnRotation.Yaw += InstanceData.ArcYawOffset;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaFanZoneActor* Zone = World->SpawnActor<AEchidnaFanZoneActor>(
		InstanceData.ArcZoneClass, GetBossFeetLocation(InstanceData.Boss), SpawnRotation, SpawnParams);

	if (!Zone)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRibbon] 호 장판 SpawnActor 실패"));
		return;
	}

	// ArcInnerRadius > 0이라 중심에서 뻗는 부채꼴이 아니라 호 끝부분만 타격하는 얇은 고리가 된다
	Zone->FanAngle = InstanceData.ArcAngle;
	Zone->FanInnerRadius = InstanceData.ArcInnerRadius;
	Zone->FanRange = InstanceData.ArcRange;
	Zone->TelegraphDuration = InstanceData.ArcTelegraphDuration;
	Zone->RingCount = 1;

	Zone->Activate(InstanceData.Damage, InstanceData.Boss->GetController());
	InstanceData.CurrentZone = Zone;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRibbonPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SpawnedRibbons.Reset();
	InstanceData.CurrentZone = nullptr;

	if (!InstanceData.Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRibbon] Boss 미바인딩 — 패턴 실패"));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.RibbonClass)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRibbon] RibbonClass 미할당 — 패턴 실패"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.BaseAimRotation = FacePlayer(InstanceData.Boss);
	SpawnRibbons(InstanceData);

	InstanceData.Phase = EEchidnaRibbonPhase::Ribbon1;
	InstanceData.PhaseElapsed = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRibbonPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PhaseElapsed += DeltaTime;

	switch (InstanceData.Phase)
	{
	case EEchidnaRibbonPhase::Ribbon1:
	{
		if (!AreRibbonsFinished(InstanceData))
		{
			return EStateTreeRunStatus::Running;
		}

		if (AnyRibbonHit(InstanceData))
		{
			SpawnCircleZone(InstanceData);
			InstanceData.Phase = EEchidnaRibbonPhase::CircleZone;
			InstanceData.PhaseElapsed = 0.f;
			return EStateTreeRunStatus::Running;
		}

		// 빗나감 — 리본이 뻗었던 끝자락으로 이동한다. 높이는 현재 높이를 유지(지면 높낮이는 무시)
		InstanceData.MoveStartLocation = InstanceData.Boss->GetActorLocation();
		InstanceData.MoveTargetLocation = InstanceData.RibbonEndLocation;
		InstanceData.MoveTargetLocation.Z = InstanceData.MoveStartLocation.Z;

		InstanceData.Phase = EEchidnaRibbonPhase::MoveToEnd;
		InstanceData.PhaseElapsed = 0.f;
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaRibbonPhase::MoveToEnd:
	{
		const float Alpha = (InstanceData.MoveDuration > 0.f)
			? FMath::Clamp(InstanceData.PhaseElapsed / InstanceData.MoveDuration, 0.f, 1.f)
			: 1.f;

		// NavMesh 대신 직접 보간 — 도착 시점이 정확해야 다음 리본 타이밍이 안 어긋난다.
		// bSweep=true라 벽은 여전히 막아준다
		InstanceData.Boss->SetActorLocation(
			FMath::Lerp(InstanceData.MoveStartLocation, InstanceData.MoveTargetLocation, Alpha), true);

		if (Alpha >= 1.f)
		{
			// 도착한 자리에서 플레이어를 다시 조준해 2차 리본
			InstanceData.BaseAimRotation = FacePlayer(InstanceData.Boss);
			SpawnRibbons(InstanceData);

			InstanceData.Phase = EEchidnaRibbonPhase::Ribbon2;
			InstanceData.PhaseElapsed = 0.f;
		}
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaRibbonPhase::Ribbon2:
	{
		if (!AreRibbonsFinished(InstanceData))
		{
			return EStateTreeRunStatus::Running;
		}

		if (AnyRibbonHit(InstanceData))
		{
			SpawnCircleZone(InstanceData);
			InstanceData.Phase = EEchidnaRibbonPhase::CircleZone;
		}
		else
		{
			// 둘 다 빗나감 — 좌측 전방으로 호 내려치기
			SpawnArcZone(InstanceData);
			InstanceData.Phase = EEchidnaRibbonPhase::ArcSlash;
		}
		InstanceData.PhaseElapsed = 0.f;
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaRibbonPhase::CircleZone:
	case EEchidnaRibbonPhase::ArcSlash:
	{
		// 장판이 스폰조차 안 됐으면(클래스 미할당 등) 기다릴 것도 없이 종료
		const bool bZoneDone = !InstanceData.CurrentZone || InstanceData.CurrentZone->IsFinished();
		if (bZoneDone || InstanceData.PhaseElapsed >= InstanceData.ZoneWaitTimeout)
		{
			InstanceData.Phase = EEchidnaRibbonPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;
	}

	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaRibbonPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaRibbonPatternDesc", "<b>Echidna Ribbon Pattern</b>");
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

	// 원 전체(0~PatrolRadius)에서 균등하게 뽑으면 중심 근처가 뽑힐 확률도 있어서 "한 발자국만 움직이고 마는"
	// 애매한 걸음이 종종 나왔음 — [MinPatrolRadius, PatrolRadius] 원형 고리에서만 뽑아 항상 어느 정도
	// 걷는 느낌이 나도록 함 (각도는 균등, 반지름은 면적 균등 분포가 되도록 제곱근 보정)
	const FVector Origin = InstanceData.Boss->GetActorLocation();
	const float MinR = FMath::Clamp(InstanceData.MinPatrolRadius, 0.f, InstanceData.PatrolRadius);
	const float RandAngle = FMath::RandRange(0.f, 2.f * PI);
	const float RandRadius = FMath::Sqrt(FMath::RandRange(MinR * MinR, InstanceData.PatrolRadius * InstanceData.PatrolRadius));
	const FVector TargetPoint = Origin + FVector(FMath::Cos(RandAngle), FMath::Sin(RandAngle), 0.f) * RandRadius;

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
