#include "EchidnaBossStateTreeUtility.h"
#include "Stat/LoADamageTypes.h"
#include "NiagaraSystem.h"
#include "StateTreeExecutionContext.h"
#include "EchidnaBoss.h"
#include "EchidnaMirrorActor.h"
#include "EchidnaFanZoneActor.h"
#include "EchidnaBigFlowerActor.h"
#include "EchidnaTetherActor.h"
#include "EchidnaHeartActor.h"
#include "EchidnaOrbActor.h"
#include "EchidnaPoopMarkActor.h"
#include "EchidnaPoopBeamActor.h"
#include "EchidnaFlytrapZoneActor.h"
#include "EchidnaLinkMirrorActor.h"
#include "EchidnaSwingZoneActor.h"
#include "EchidnaSwingChainActor.h"
#include "EchidnaButterflyActor.h"
#include "EchidnaMirrorWallActor.h"
#include "EchidnaGaugeRectActor.h"
#include "EchidnaBarrierActor.h"
#include "HexTile.h"
#include "LoAPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Texture2D.h"
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
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Raid/PatternVFXUtil.h"
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

	FName GetProcessedStateName(FStateTreeExecutionContext& Context)
	{
		const FCompactStateTreeState* State = Context.GetStateFromHandle(Context.GetCurrentlyProcessedState());
		return State ? State->Name : NAME_None;
	}

	/** 짤패턴 Task 진입 시 자기 State 이름을 기록 — Echidna Not Recent Small Pattern 조건이 연속 등장을 막는 데 씀 */
	void RecordSmallPatternEntered(FStateTreeExecutionContext& Context, AEchidnaBoss* Boss)
	{
		if (Boss)
		{
			Boss->RecordSmallPattern(GetProcessedStateName(Context));
		}
	}
}

bool FStateTreeCondition_EchidnaNotRecentSmallPattern::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FName StateName = GetProcessedStateName(Context);
	if (!InstanceData.Boss)
	{
		return true;
	}
	if (StateName.IsNone())
	{
		// Enter Condition 평가 중 자기 State를 못 찾으면 막지 않는다 — 이 로그가 보이면 연속 방지가 안 먹는 중
		UE_LOG(LogLoA, Warning, TEXT("[NotRecentSmallPattern] 평가 중인 State 이름을 못 찾음 — 연속 방지 비활성"));
		return true;
	}

	const bool bRecent = InstanceData.Boss->WasRecentSmallPattern(StateName, InstanceData.AvoidRecentCount);
	UE_LOG(LogLoA, Log, TEXT("[NotRecentSmallPattern] %s — %s"), *StateName.ToString(), bRecent ? TEXT("직전에 나옴, 제외") : TEXT("통과"));
	return !bRecent;
}

#if WITH_EDITOR
FText FStateTreeCondition_EchidnaNotRecentSmallPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("NotRecentSmallPatternDesc", "<b>Echidna Not Recent Small Pattern</b>");
}
#endif // WITH_EDITOR

bool FStateTreeCondition_BossLineThreshold::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Boss)
	{
		return false;
	}

	// 다른 큰 패턴(시간·정산·거울 카운터) 진행 중이면 대기 — Root On Tick 전이가 진행 중인 큰 패턴을 끊지 않게
	if (InstanceData.Boss->IsTimedPatternActive())
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


namespace
{
	// 큰 패턴 시작 대사창 — 플레이어 컨트롤러에 띄우고 내린다
	void ShowPatternDialogue(ALoACharacter* Player, const FText& Speaker, const FText& Line, const TSoftObjectPtr<UTexture2D>& Portrait)
	{
		if (!Player) return;
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
		{
			PC->ShowBossDialogue(Speaker, Line, Portrait.LoadSynchronous());
		}
	}

	void HidePatternDialogue(ALoACharacter* Player)
	{
		if (!Player) return;
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
		{
			PC->HideBossDialogue();
		}
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaRandomGrabPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Phase = EEchidnaRandomGrabPhase::WaitFog;
	InstanceData.PhaseElapsed = 0.f;
	InstanceData.RoundsStarted = 0;
	InstanceData.Traps.Reset();
	InstanceData.Arena = nullptr;
	InstanceData.LotusFlowers.Reset();
	InstanceData.LotusZones.Reset();
	InstanceData.PendingLotusTimes.Reset();
	InstanceData.PendingLotusLocations.Reset();
	InstanceData.bLotusScheduled = false;

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

	// 시작 대사 — FogDelay 뒤 연기가 깔릴 때 내린다
	ShowPatternDialogue(InstanceData.Player.Get(), InstanceData.DialogueSpeaker, InstanceData.DialogueLine, InstanceData.DialoguePortrait);

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

	// 연꽃 — 연기가 깔린 순간 한 번 예약된 것들을 피우고 터뜨림
	TickLotus(InstanceData, DeltaTime);

	switch (InstanceData.Phase)
	{
	case EEchidnaRandomGrabPhase::WaitFog:
		if (InstanceData.PhaseElapsed >= FMath::Max(InstanceData.FogDelay, InstanceData.DialogueDuration))
		{
			HidePatternDialogue(Player);
			if (Player)
			{
				if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
				{
					PC->SetScreenFog(true, InstanceData.FogFadeTime);
				}
			}
			InstanceData.Phase = EEchidnaRandomGrabPhase::FogIn;
			InstanceData.PhaseElapsed = 0.f;
			// 연기가 깔린 순간 연꽃을 한 번만 예약 (각 연꽃은 LotusSpawnDelay 뒤에 핀다)
			ScheduleLotus(InstanceData);
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
		// 이미 핀/예약된 연꽃이 다 터질 때까지는 끝내지 않는다
		if (InstanceData.PhaseElapsed >= InstanceData.EndDelay && !HasLiveLotus(InstanceData))
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

void FStateTreeTask_EchidnaRandomGrabPattern::ScheduleLotus(FInstanceDataType& InstanceData) const
{
	if (InstanceData.bLotusScheduled || !InstanceData.Arena) return;
	InstanceData.bLotusScheduled = true;

	const int32 MinCount = FMath::Max(0, InstanceData.LotusCountMin);
	const int32 Count = FMath::RandRange(MinCount, FMath::Max(MinCount, InstanceData.LotusCountMax));
	if (Count <= 0) return;

	// 타일을 섞어서 앞에서부터 하나씩 — 서로 다른 타일이라 맵 전체에 골고루 퍼지고, 이웃 타일끼리는 자연스럽게 뭉치기도 한다.
	// 연꽃이 타일 수보다 많으면 한 바퀴 더 돌며 같은 타일에 하나 더
	TArray<FIntPoint> Coords;
	InstanceData.Arena->TileMap.GetKeys(Coords);
	if (Coords.Num() == 0) return;
	for (int32 i = Coords.Num() - 1; i > 0; i--)
	{
		Coords.Swap(i, FMath::RandRange(0, i));
	}

	for (int32 n = 0; n < Count; n++)
	{
		FVector Top;
		if (!InstanceData.Arena->GetTileTopLocation(Coords[n % Coords.Num()], Top)) continue;

		const FVector2D Offset = FMath::RandPointInCircle(150.f);
		InstanceData.PendingLotusLocations.Add(Top + FVector(Offset.X, Offset.Y, 0.f));
		InstanceData.PendingLotusTimes.Add(FMath::FRandRange(InstanceData.LotusSpawnDelayMin, FMath::Max(InstanceData.LotusSpawnDelayMin, InstanceData.LotusSpawnDelayMax)));
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaRandomGrab] 연꽃 %d개 예약"), Count);
}

void FStateTreeTask_EchidnaRandomGrabPattern::TickLotus(FInstanceDataType& InstanceData, float DeltaTime) const
{
	// 시간이 된 연꽃 피우기 (예약된 건 마무리 단계에서도 핀다)
	for (int32 i = InstanceData.PendingLotusTimes.Num() - 1; i >= 0; i--)
	{
		InstanceData.PendingLotusTimes[i] -= DeltaTime;
		if (InstanceData.PendingLotusTimes[i] <= 0.f)
		{
			SpawnLotusAt(InstanceData, InstanceData.PendingLotusLocations[i]);
			InstanceData.PendingLotusTimes.RemoveAtSwap(i);
			InstanceData.PendingLotusLocations.RemoveAtSwap(i);
		}
	}

	// 터지는 순간 연꽃(비주얼)은 사라지고 폭발 이펙트만 남는다
	for (int32 i = InstanceData.LotusZones.Num() - 1; i >= 0; i--)
	{
		AEchidnaFanZoneActor* Zone = InstanceData.LotusZones[i];
		if (!IsValid(Zone) || Zone->HasStartedExploding())
		{
			if (InstanceData.LotusFlowers.IsValidIndex(i) && IsValid(InstanceData.LotusFlowers[i]))
			{
				InstanceData.LotusFlowers[i]->Destroy();
			}
			InstanceData.LotusZones.RemoveAtSwap(i);
			if (InstanceData.LotusFlowers.IsValidIndex(i))
			{
				InstanceData.LotusFlowers.RemoveAtSwap(i);
			}
		}
	}
}

void FStateTreeTask_EchidnaRandomGrabPattern::SpawnLotusAt(FInstanceDataType& InstanceData, const FVector& Location) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World) return;

	const FTransform Xform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location);

	// 연꽃 비주얼 — 큰 꽃 액터를 작게(꽃잎 길이 = 폭발 반지름), 바닥 잎 없이
	AEchidnaBigFlowerActor* Flower = World->SpawnActorDeferred<AEchidnaBigFlowerActor>(
		AEchidnaBigFlowerActor::StaticClass(), Xform, Boss, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Flower)
	{
		Flower->PetalLength = InstanceData.LotusRadius;
		Flower->LeafCount = 0;
		Flower->FinishSpawning(Xform);
	}

	// 폭발 — 부채꼴 장판을 360도 원으로. 예고 메시는 투명(연꽃 자체가 예고), LotusFuseTime 뒤 단발 판정 + Niagara
	AEchidnaFanZoneActor* Zone = World->SpawnActorDeferred<AEchidnaFanZoneActor>(
		AEchidnaFanZoneActor::StaticClass(), Xform, Boss, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Zone)
	{
		if (Flower) Flower->Destroy();
		return;
	}
	Zone->FanAngle = 360.f;
	Zone->FanInnerRadius = 0.f;
	Zone->FanRange = InstanceData.LotusRadius;
	Zone->RingCount = 1;
	Zone->TelegraphDuration = InstanceData.LotusFuseTime;
	Zone->TelegraphOpacity = 0.f;
	// 맞으면 넉다운 없이 짧은 경직 + 매혹 1스택
	Zone->bApplyKnockdownOnHit = false;
	Zone->bApplyStaggerOnHit = true;
	Zone->bApplyCharmGaugeOnHit = true;
	Zone->CharmGaugePerHit = 1;
	Zone->FinishSpawning(Xform);
	Zone->Activate(InstanceData.LotusDamage, Boss->GetController());

	InstanceData.LotusFlowers.Add(Flower);
	InstanceData.LotusZones.Add(Zone);
}

bool FStateTreeTask_EchidnaRandomGrabPattern::HasLiveLotus(const FInstanceDataType& InstanceData) const
{
	return InstanceData.PendingLotusTimes.Num() > 0 || InstanceData.LotusZones.Num() > 0;
}

void FStateTreeTask_EchidnaRandomGrabPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.Boss)
	{
		InstanceData.Boss->ClearActiveTimedPattern();
	}
	HidePatternDialogue(InstanceData.Player.Get());

	// 연꽃 정리 — 중간에 끊겨도 남지 않게
	for (const TObjectPtr<AEchidnaBigFlowerActor>& Flower : InstanceData.LotusFlowers)
	{
		if (IsValid(Flower)) Flower->Destroy();
	}
	for (const TObjectPtr<AEchidnaFanZoneActor>& Zone : InstanceData.LotusZones)
	{
		if (IsValid(Zone) && !Zone->HasStartedExploding()) Zone->Destroy();
	}
	InstanceData.LotusFlowers.Reset();
	InstanceData.LotusZones.Reset();
	InstanceData.PendingLotusTimes.Reset();
	InstanceData.PendingLotusLocations.Reset();

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

	// 시작 연출 — 대사창 + 시점을 넓힘. 거울은 대사가 끝난 뒤(Tick의 Vanished 단계) 등장
	if (ALoACharacter* Player = InstanceData.Player.Get())
	{
		if (InstanceData.CameraArmLength > 0.f)
		{
			Player->SetCameraZoomOverride(InstanceData.CameraArmLength);
		}
		ShowPatternDialogue(Player, InstanceData.DialogueSpeaker, InstanceData.DialogueLine, InstanceData.DialoguePortrait);
	}

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
		if (InstanceData.PhaseElapsed >= FMath::Max(InstanceData.VanishDuration, InstanceData.DialogueDuration))
		{
			if (ALoACharacter* Player = InstanceData.Player.Get())
			{
				if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
				{
					PC->HideBossDialogue();
				}
			}

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
				UGameplayStatics::ApplyDamage(*It, It->GetMaxHP() * InstanceData.FailMaxHPRatio, Boss->GetController(), Boss, ULoATrueDamageType::StaticClass());
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

	// 시작 연출 정리 — 중간에 끊겨도 대사창·시점이 남지 않게
	if (ALoACharacter* Player = InstanceData.Player.Get())
	{
		Player->ClearCameraZoomOverride();
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
		{
			PC->HideBossDialogue();
		}
	}

	if (InstanceData.Arena)
	{
		InstanceData.Arena->ClearAllLinkHighlights();
		InstanceData.Arena->SetAllTilesDangerFlash(false);

		// 다음 거울잇기(50%/100%)는 다른 자리에서 — 파란 테두리 2칸 다시 뽑기
		InstanceData.Arena->RerollMarkerTiles(true);
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

EStateTreeRunStatus FStateTreeTask_EchidnaSwingPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Phase = EEchidnaSwingPhase::Vanished;
	InstanceData.PhaseElapsed = 0.f;
	InstanceData.Zone = nullptr;
	InstanceData.Chain = nullptr;
	InstanceData.Butterflies.Reset();
	InstanceData.Arena = nullptr;

	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaSwing] EnterState 실패 — Boss 바인딩 확인"));
		return EStateTreeRunStatus::Failed;
	}

	// 재진입해도 두 번 나오지 않도록 진입 즉시 표시 (랜잡과 같은 방식)
	MarkBigPatternTriggered(Context, Boss, InstanceData.PatternName, TEXT("EchidnaSwing"));

	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		InstanceData.Arena = *It;
		break;
	}
	if (!InstanceData.Arena)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaSwing] 아레나 없음"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	Boss->GetCharacterMovement()->StopMovementImmediately();

	// 시작 대사 — DialogueDuration 동안 보스는 그 자리에 가만히, 끝나면(Tick) 사라지며 패턴 진행
	InstanceData.bIntroDone = false;
	ShowPatternDialogue(InstanceData.Player.Get(), InstanceData.DialogueSpeaker, InstanceData.DialogueLine, InstanceData.DialoguePortrait);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaSwing] 패턴 시작 — Player:%s"), InstanceData.Player.IsValid() ? TEXT("O") : TEXT("X"));
	return EStateTreeRunStatus::Running;
}

namespace
{
	// 외곽 링 반지름 R일 때 큐브 좌표 (q, r, s=-q-r) 중 |값| == R인 축 개수 — 1이면 변 가운데, 2면 꼭짓점
	int32 GetSwingRingRadius(const FIntPoint& Coord)
	{
		return AHexArena::GetHexDistance(Coord, FIntPoint(0, 0));
	}

	int32 GetSwingBorderAxisCount(const FIntPoint& Coord)
	{
		const int32 R = GetSwingRingRadius(Coord);
		const int32 Cube[3] = { Coord.X, Coord.Y, -Coord.X - Coord.Y };
		int32 Count = 0;
		for (int32 V : Cube) { if (R > 0 && FMath::Abs(V) == R) ++Count; }
		return Count;
	}

	// BossCoord가 선 변(|축| == R인 축)의 반대 변 = 같은 축이 -값인 외곽 타일 전부
	TArray<FIntPoint> GetSwingOppositeSideCoords(const AHexArena* Arena, const FIntPoint& BossCoord)
	{
		TArray<FIntPoint> Result;
		const int32 R = GetSwingRingRadius(BossCoord);
		const int32 BossCube[3] = { BossCoord.X, BossCoord.Y, -BossCoord.X - BossCoord.Y };
		int32 Axis = INDEX_NONE;
		for (int32 i = 0; i < 3; ++i) { if (R > 0 && FMath::Abs(BossCube[i]) == R) { Axis = i; break; } }
		if (!Arena || Axis == INDEX_NONE)
		{
			Result.Add(FIntPoint(-BossCoord.X, -BossCoord.Y));
			return Result;
		}
		for (const FIntPoint& Coord : Arena->GetOuterRingCoords())
		{
			const int32 Cube[3] = { Coord.X, Coord.Y, -Coord.X - Coord.Y };
			if (Cube[Axis] == -BossCube[Axis]) Result.Add(Coord);
		}
		return Result;
	}
}

bool FStateTreeTask_EchidnaSwingPattern::AppearAtRandomEdge(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	AHexArena* Arena = InstanceData.Arena;
	if (!Boss || !Arena) return false;

	// 꼭짓점 타일은 두 변에 걸쳐 "보스가 선 변"이 정해지지 않으므로 변 가운데 타일에서만 등장
	TArray<FIntPoint> Ring = Arena->GetOuterRingCoords();
	Ring.RemoveAll([](const FIntPoint& Coord) { return GetSwingBorderAxisCount(Coord) != 1; });
	if (Ring.Num() == 0) return false;
	InstanceData.BossCoord = Ring[FMath::RandRange(0, Ring.Num() - 1)];

	FVector TileTop, CenterTop;
	if (!Arena->GetTileTopLocation(InstanceData.BossCoord, TileTop) || !Arena->GetTileTopLocation(FIntPoint(0, 0), CenterTop))
	{
		return false;
	}

	// 캡슐 중심 = 타일 윗면 + 절반 높이, 아레나 중심을 바라본다
	InstanceData.BossStandLocation = TileTop + FVector(0.f, 0.f, Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector ToCenter = (CenterTop - TileTop).GetSafeNormal2D();
	Boss->SetActorLocationAndRotation(InstanceData.BossStandLocation, FRotator(0.f, ToCenter.Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Boss->GetCharacterMovement()->StopMovementImmediately();
	SetBossVanished(Boss, false);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaSwing] 보스 외곽 등장 (%d,%d)"), InstanceData.BossCoord.X, InstanceData.BossCoord.Y);
	return true;
}

void FStateTreeTask_EchidnaSwingPattern::SpawnZone(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss) return;

	// 보스 발밑(캡슐 바닥) 높이
	const FVector Feet = InstanceData.BossStandLocation - FVector(0.f, 0.f, Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	UClass* ZoneClass = InstanceData.ZoneClass ? InstanceData.ZoneClass.Get() : AEchidnaSwingZoneActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InstanceData.Zone = Boss->GetWorld()->SpawnActor<AEchidnaSwingZoneActor>(ZoneClass, Feet, FRotator::ZeroRotator, SpawnParams);
	if (InstanceData.Zone)
	{
		InstanceData.Zone->Activate(InstanceData.ZoneDamageRatio, Boss->GetController(), InstanceData.SafeRadiusOverride);
	}
}

void FStateTreeTask_EchidnaSwingPattern::StartChainPhase(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	ALoACharacter* Player = InstanceData.Player.Get();
	if (!Boss) return;

	// 연기와 함께 오염 장판 전부 활성화(빨강) — 사슬을 끊으러 가는 길에 밟으면 매혹·데미지
	if (InstanceData.Arena)
	{
		InstanceData.Arena->SetAllPoopTilesActive(true);
	}

	if (Player)
	{
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
		{
			PC->SetScreenFog(true, InstanceData.FogFadeTime);
		}

		// 사슬 — 보스가 선 변과 마주보는 반대 변의 타일 전부(꼭짓점 포함 4개)가 끊는 타일
		UClass* ChainClass = InstanceData.ChainClass ? InstanceData.ChainClass.Get() : AEchidnaSwingChainActor::StaticClass();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Boss;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InstanceData.Chain = Boss->GetWorld()->SpawnActor<AEchidnaSwingChainActor>(ChainClass, Boss->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
		if (InstanceData.Chain)
		{
			InstanceData.Chain->Activate(Boss, Player, InstanceData.Arena, GetSwingOppositeSideCoords(InstanceData.Arena, InstanceData.BossCoord));
		}
	}

	SpawnButterflies(InstanceData);
}

void FStateTreeTask_EchidnaSwingPattern::SpawnButterflies(FInstanceDataType& InstanceData) const
{
	AHexArena* Arena = InstanceData.Arena;
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Arena || !Boss || InstanceData.ButterflyCount <= 0) return;

	// 후보 = 맵 안쪽(외곽 링 제외) 타일 중 플레이어와 가까운 곳 제외
	const int32 R = Arena->SideCount - 1;
	FIntPoint PlayerCoord(INT32_MAX, INT32_MAX);
	const bool bHasPlayerCoord = InstanceData.Player.IsValid() && Arena->WorldToTileCoord(InstanceData.Player->GetActorLocation(), PlayerCoord);

	TArray<FIntPoint> Candidates;
	for (int32 q = -R; q <= R; ++q)
	{
		for (int32 r = FMath::Max(-R, -q - R); r <= FMath::Min(R, -q + R); ++r)
		{
			const FIntPoint Coord(q, r);
			if (AHexArena::GetHexDistance(Coord, FIntPoint(0, 0)) >= R) continue;
			if (bHasPlayerCoord && AHexArena::GetHexDistance(Coord, PlayerCoord) <= InstanceData.ButterflySafeDistance) continue;
			Candidates.Add(Coord);
		}
	}
	if (Candidates.Num() == 0) return;

	UClass* ButterflyClass = InstanceData.ButterflyClass ? InstanceData.ButterflyClass.Get() : AEchidnaButterflyActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < InstanceData.ButterflyCount; ++i)
	{
		// 타일이 모자라면 같은 타일에 여러 마리 — 타일 안에서 위치를 흩뿌려 겹치지 않게
		const FIntPoint Coord = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		FVector TileTop;
		if (!Arena->GetTileTopLocation(Coord, TileTop)) continue;

		const FVector2D Jitter = FMath::RandPointInCircle(Arena->TileSpacing * 0.3f);
		const FVector Location = TileTop + FVector(Jitter.X, Jitter.Y, InstanceData.ButterflyHeight);

		if (AEchidnaButterflyActor* Butterfly = Boss->GetWorld()->SpawnActor<AEchidnaButterflyActor>(ButterflyClass, Location, FRotator::ZeroRotator, SpawnParams))
		{
			Butterfly->Activate(Arena, InstanceData.ButterflyStunDuration);
			InstanceData.Butterflies.Add(Butterfly);
		}
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaSwingPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	Boss->GetCharacterMovement()->StopMovementImmediately();
	InstanceData.PhaseElapsed += DeltaTime;

	// 시작 대사가 끝나야 패턴 진행 — 그때 보스가 사라진다
	if (!InstanceData.bIntroDone)
	{
		if (InstanceData.PhaseElapsed < InstanceData.DialogueDuration)
		{
			return EStateTreeRunStatus::Running;
		}
		InstanceData.bIntroDone = true;
		InstanceData.PhaseElapsed = 0.f;
		HidePatternDialogue(InstanceData.Player.Get());
		SetBossVanished(Boss, true);
	}

	// 등장한 뒤엔 패턴 내내 그 외곽 타일 위에 고정
	if (InstanceData.Phase != EEchidnaSwingPhase::Vanished)
	{
		Boss->SetActorLocation(InstanceData.BossStandLocation);
	}

	switch (InstanceData.Phase)
	{
	case EEchidnaSwingPhase::Vanished:
		if (InstanceData.PhaseElapsed >= InstanceData.VanishDuration)
		{
			if (!AppearAtRandomEdge(InstanceData))
			{
				UE_LOG(LogLoA, Warning, TEXT("[EchidnaSwing] 보스 등장 실패"));
				return EStateTreeRunStatus::Failed;
			}
			InstanceData.Phase = EEchidnaSwingPhase::Appeared;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaSwingPhase::Appeared:
		if (InstanceData.PhaseElapsed >= InstanceData.ZoneDelay)
		{
			SpawnZone(InstanceData);
			InstanceData.Phase = EEchidnaSwingPhase::Zone;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaSwingPhase::Zone:
		// 장판 스폰에 실패했어도 패턴이 멈추지 않게 바로 다음 단계로
		if (!IsValid(InstanceData.Zone) || InstanceData.Zone->HasExploded())
		{
			StartChainPhase(InstanceData);
			InstanceData.Phase = EEchidnaSwingPhase::Chain;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaSwingPhase::Chain:
		if (!IsValid(InstanceData.Chain) || InstanceData.Chain->GetResult() != EEchidnaSwingChainResult::Pending)
		{
			InstanceData.Phase = EEchidnaSwingPhase::Ending;
			InstanceData.PhaseElapsed = 0.f;
		}
		break;

	case EEchidnaSwingPhase::Ending:
		if (InstanceData.PhaseElapsed >= InstanceData.EndDelay)
		{
			InstanceData.Phase = EEchidnaSwingPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		break;

	default:
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaSwingPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (AEchidnaBoss* Boss = InstanceData.Boss)
	{
		// 사라진 채 끊기면 영영 안 보이므로 어떤 경우든 복구
		SetBossVanished(Boss, false);
		Boss->ClearActiveTimedPattern();
	}
	HidePatternDialogue(InstanceData.Player.Get());

	if (ALoACharacter* Player = InstanceData.Player.Get())
	{
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Player->GetController()))
		{
			PC->SetScreenFog(false, InstanceData.FogFadeTime);
		}
	}

	// 오염 장판 다시 비활성(핑크) — 중간에 끊겨도
	if (InstanceData.Arena)
	{
		InstanceData.Arena->SetAllPoopTilesActive(false);
	}

	// 사슬은 EndPlay에서 노란 테두리도 같이 끈다
	if (IsValid(InstanceData.Chain))
	{
		InstanceData.Chain->Destroy();
	}
	if (IsValid(InstanceData.Zone))
	{
		InstanceData.Zone->Destroy();
	}
	for (const TObjectPtr<AEchidnaButterflyActor>& Butterfly : InstanceData.Butterflies)
	{
		if (IsValid(Butterfly))
		{
			Butterfly->Destroy();
		}
	}
	InstanceData.Butterflies.Reset();
	InstanceData.Chain = nullptr;
	InstanceData.Zone = nullptr;
	InstanceData.Arena = nullptr;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaSwingPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaSwingPatternDesc", "<b>Echidna Swing Pattern</b>");
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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
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
			Mirror->ScaleBeamWidth(InstanceData.SpokeBeamWidthScale);
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

	// 보스 위치에서 스폰 — 높이는 보스 캡슐 중심 Z(=보스 키의 절반) 그대로 사용. bSkyGuidedMode의 Tracking
	// 단계 로직이 스폰 직후부터 매 틱 X/Y를 플레이어 위치로 맞춰가므로 여기서 플레이어 위치를 미리 계산할
	// 필요 없음 — 스폰되자마자 보스에게서 플레이어 쪽으로 쫓아가기 시작함
	const FVector SpawnLocation = BossLocation;

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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	InstanceData.PlusMirrors.Reset();
	InstanceData.CrossMirrors.Reset();
	InstanceData.GuidedMirror = nullptr;
	InstanceData.Elapsed = 0.f;
	InstanceData.bGuidedSpawned = false;

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

	// 유도 거울은 여기서 스폰하지 않는다 — Tick에서 GuidedSpawnDelay가 지난 뒤 1회 스폰

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

	// 유도 거울 — 패턴 시작 후 GuidedSpawnDelay가 지나면 보스 위치에 1회 스폰, 이후 파동 전환과 무관하게 독립 진행.
	// 두 파동이 끝났으면(Done) 새로 스폰하지 않는다.
	InstanceData.Elapsed += DeltaTime;
	if (!InstanceData.bGuidedSpawned
		&& InstanceData.Phase != EEchidnaEightMirrorPhase::Done
		&& InstanceData.Elapsed >= InstanceData.GuidedSpawnDelay)
	{
		InstanceData.bGuidedSpawned = true;
		SpawnGuidedMirror(InstanceData);
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

	// 아레나 타일 그리드 안이면 바닥 있음 — 라인트레이스만 쓰면 타일 사이 틈(HexGap)에 빠져 홉이 매번 취소됐음
	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		FIntPoint Coord;
		if (It->WorldToTileCoord(Location, Coord) && It->GetTile(Coord))
		{
			return true;
		}
	}

	FHitResult Hit;
	const FVector TraceStart = Location + FVector(0.f, 0.f, 200.f);
	const FVector TraceEnd = Location - FVector(0.f, 0.f, 500.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(InstanceData.Boss);

	return World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

bool FStateTreeTask_EchidnaRetreatFanPattern::HopBackward(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss) return false;

	// 1번 장판을 쏜 방향(LockedYaw)을 바라본 채 그 정반대로 — 플레이어 위치는 안 봄
	FVector AwayFromZone = -FRotator(0.f, InstanceData.LockedYaw, 0.f).Vector();
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
		return false;
	}

	const FVector LaunchVelocity = AwayFromZone * InstanceData.HopBackStrength + FVector(0.f, 0.f, InstanceData.HopUpwardStrength);
	InstanceData.Boss->LaunchCharacter(LaunchVelocity, true, true);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaRetreatFan] 후방 홉 — Loc=%s Velocity=%s"),
		*InstanceData.Boss->GetActorLocation().ToString(), *LaunchVelocity.ToString());
	return true;
}

FRotator FStateTreeTask_EchidnaRetreatFanPattern::ComputeAimRotation(const AEchidnaBoss* Boss) const
{
	// 패턴 시작 순간의 플레이어 방향을 한 번만 계산해서 고정 기준선으로 씀(플레이어 없으면 보스 정면 fallback).
	// 1번/2번 장판 모두 이 값 + FanSideYaw만 쓰고 다시 조준하지 않으므로, 두 장판의 방향 차이는 항상
	// 2*FanSideYaw(고정값)로 일정함 — "2번이 플레이어를 다시 보면서 크게 틀어지는" 것처럼 보였던 원인은
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

AEchidnaFanZoneActor* FStateTreeTask_EchidnaRetreatFanPattern::SpawnFan(FInstanceDataType& InstanceData, float YawOffsetDeg, const FVector& Origin, float ExtraRange, float TelegraphOverride) const
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

	// 1번 = 시작 위치, 2번 = 백점프 착지 위치(공략 그림 — 물러난 자리에서 다시 쏨). 보스는 정면을 본 채 곧게 뒤로
	// 물러나므로 두 원점이 같은 정면 축 위에 있어 방향이 어긋나지 않는다
	const FVector SpawnLocation = Origin;

	// 매 캐스팅마다 플레이어 위치를 다시 조준하면 1번(왼쪽)과 2번(오른쪽)의 기준선이 서로 달라져
	// 패턴이 시작된 후 플레이어가 움직이면 겹치는 구간이 어긋나 보임 — EnterState에서 한 번만 정한
	// BaseAimRotation을 그대로 쓰고, 여기서는 좌/우 각도(YawOffsetDeg)만 더한다
	FRotator SpawnRotation = InstanceData.BaseAimRotation;
	SpawnRotation.Yaw += YawOffsetDeg;

	// 보스 방향은 정면(LockedYaw)으로 고정 — 장판 방향 따라 돌지 않음
	InstanceData.Boss->SetActorRotation(FRotator(0.f, InstanceData.LockedYaw, 0.f));

	AController* BossController = InstanceData.Boss->GetController();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	AEchidnaFanZoneActor* Fan = World->SpawnActor<AEchidnaFanZoneActor>(
		InstanceData.FanZoneClass, SpawnLocation, SpawnRotation, SpawnParams);

	if (Fan)
	{
		if (UNiagaraSystem* VFX = InstanceData.RingVFX.LoadSynchronous())
		{
			Fan->RingVFX = VFX;
		}
		if (InstanceData.RingVFXSpacing > 0.f) Fan->RingVFXSpacing = InstanceData.RingVFXSpacing;
		if (InstanceData.RingVFXScale > 0.f) Fan->RingVFXScale = InstanceData.RingVFXScale;

		Fan->NearHalfWidth = InstanceData.NearHalfWidth;
		// 2번은 물러난 거리만큼 길게 — 바깥 끝이 1번과 비슷한 곳까지 닿게
		Fan->FanRange = Fan->FanRange * InstanceData.FanRangeScale + ExtraRange;
		if (InstanceData.FanAngle > 0.f) Fan->FanAngle = InstanceData.FanAngle;
		if (TelegraphOverride > 0.f) Fan->TelegraphDuration = TelegraphOverride;

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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	InstanceData.CurrentFan = nullptr;
	InstanceData.bHoppedForCurrentCast = false;
	InstanceData.SinceFirstExplode = 0.f;
	InstanceData.FirstFan = nullptr;

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

	// 보스는 패턴 끝까지 정면을 본다. 먼저 1번째 백점프 — 착지 자리에서 1번 장판(Tick)
	InstanceData.LockedYaw = InstanceData.BaseAimRotation.Yaw;
	InstanceData.Boss->SetActorRotation(FRotator(0.f, InstanceData.LockedYaw, 0.f));
	InstanceData.bHopLaunched = HopBackward(InstanceData);
	InstanceData.HopElapsed = 0.f;
	InstanceData.Phase = EEchidnaRetreatFanPhase::Hop1;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRetreatFanPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 홉 중에도·장판 때도 정면 그대로 (AI 포커스 등 다른 회전에 끌려가지 않게 매 틱)
	InstanceData.Boss->SetActorRotation(FRotator(0.f, InstanceData.LockedYaw, 0.f));

	// 백점프 착지 판정 — 발사 직후엔 아직 IsFalling이 false라 최소 0.15초는 기다린다. 취소된 홉은 바로 착지로 봄
	InstanceData.HopElapsed += DeltaTime;
	const UCharacterMovementComponent* Move = InstanceData.Boss->GetCharacterMovement();
	const bool bLanded = !InstanceData.bHopLaunched || (InstanceData.HopElapsed >= 0.15f && !(Move && Move->IsFalling()));

	auto GetFeet = [&InstanceData]()
	{
		FVector Feet = InstanceData.Boss->GetActorLocation();
		if (const UCapsuleComponent* Capsule = InstanceData.Boss->GetCapsuleComponent())
		{
			Feet.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
		return Feet;
	};

	switch (InstanceData.Phase)
	{
	case EEchidnaRetreatFanPhase::Hop1:
	{
		if (!bLanded)
		{
			return EStateTreeRunStatus::Running;
		}

		// 1번째 장판 — 1번째 백점프 착지 자리에서 정면 기준 왼쪽으로 살짝
		InstanceData.BaseSpawnLocation = GetFeet();
		InstanceData.CurrentFan = SpawnFan(InstanceData, -InstanceData.FanSideYaw, InstanceData.BaseSpawnLocation, 0.f);
		if (!InstanceData.CurrentFan)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.bHoppedForCurrentCast = false;
		InstanceData.Phase = EEchidnaRetreatFanPhase::Casting1;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaRetreatFanPhase::Casting1:
	{
		// 1번이 터지기 시작하면 2번째 백점프
		if (!InstanceData.bHoppedForCurrentCast && InstanceData.CurrentFan && InstanceData.CurrentFan->HasStartedExploding())
		{
			InstanceData.bHopLaunched = HopBackward(InstanceData);
			InstanceData.HopElapsed = 0.f;
			InstanceData.bHoppedForCurrentCast = true;
			return EStateTreeRunStatus::Running;
		}
		if (!InstanceData.bHoppedForCurrentCast)
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.SinceFirstExplode += DeltaTime * AEchidnaBoss::GetEnrageTimeScale(InstanceData.Boss);
		if (!bLanded)
		{
			return EStateTreeRunStatus::Running;
		}

		// 2번째 장판 — 2번째 백점프 착지 자리에서 정면 기준 오른쪽으로 살짝 (가운데가 1번과 겹침).
		// 1번 폭발 시작부터 SecondFanInterval 뒤에 터지도록 남은 시간을 예고로
		const float SecondTelegraph = FMath::Max(InstanceData.SecondFanMinTelegraph, InstanceData.SecondFanInterval - InstanceData.SinceFirstExplode);
		InstanceData.FirstFan = InstanceData.CurrentFan;
		const FVector LandedFeet = GetFeet();
		const FVector Forward = InstanceData.BaseAimRotation.Vector().GetSafeNormal2D();
		const float Retreated = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(InstanceData.BaseSpawnLocation - LandedFeet, Forward)));
		InstanceData.CurrentFan = SpawnFan(InstanceData, InstanceData.FanSideYaw, LandedFeet, Retreated, SecondTelegraph);
		if (!InstanceData.CurrentFan)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.Phase = EEchidnaRetreatFanPhase::Casting2;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaRetreatFanPhase::Casting2:
	{
		// 1번이 아직 퍼지는 중일 수 있어 둘 다 끝나야 종료
		if (IsValid(InstanceData.FirstFan) && !InstanceData.FirstFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	InstanceData.CurrentFan = nullptr;
	InstanceData.InnerDonut = nullptr;
	InstanceData.AirborneZone = nullptr;
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
	InstanceData.CurrentFan = SpawnZone(InstanceData, InstanceData.SlashZoneClass, InstanceData.Slash1Yaw,
		InstanceData.SlashArcAngle, InstanceData.SlashInnerRadius, InstanceData.SlashRange,
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
		InstanceData.CurrentFan = SpawnZone(InstanceData, InstanceData.SlashZoneClass, InstanceData.Slash2Yaw,
			InstanceData.SlashArcAngle, InstanceData.SlashInnerRadius, InstanceData.SlashRange,
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
			InstanceData.SmallDonutHoleRadius, InstanceData.SmallDonutOuterRadius,
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

		// 3번이 끝나면 보스 주변 꽉 찬 원(에어본) 예고 — 터지는 순간 보스가 뜸
		InstanceData.AirborneZone = SpawnZone(InstanceData, InstanceData.FanZoneClass, 0.f, 360.f,
			0.f, InstanceData.AirborneRadius, InstanceData.AirborneTelegraphDuration, 1);
		if (!InstanceData.AirborneZone)
		{
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.Phase = EEchidnaDonutSlashPhase::AirborneTelegraph;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaDonutSlashPhase::AirborneTelegraph:
	{
		if (!InstanceData.AirborneZone || !InstanceData.AirborneZone->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		// 에어본 판정이 끝난 바로 이 시점부터 상승 시작
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
			InstanceData.BigDonutHoleRadius, InstanceData.OuterDonutOuterRadius,
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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
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

	// 하트를 쏜 방향 그대로 부채꼴 한 번 폭발 — 맞았든 안 맞았든
	FVector FanOrigin = InstanceData.Boss->GetActorLocation();
	FanOrigin.Z -= InstanceData.Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	UClass* FanClass = InstanceData.BlastFanClass ? InstanceData.BlastFanClass.Get() : AEchidnaFanZoneActor::StaticClass();
	FActorSpawnParameters FanParams;
	FanParams.Owner = InstanceData.Boss;
	FanParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AEchidnaFanZoneActor* Fan = World->SpawnActor<AEchidnaFanZoneActor>(FanClass, FanOrigin,
		FRotator(0.f, InstanceData.BaseAimRotation.Yaw, 0.f), FanParams))
	{
		Fan->FanAngle = FMath::Clamp(InstanceData.FanSpreadAngle + InstanceData.BlastExtraAngle, 1.f, 360.f);
		Fan->FanRange = InstanceData.BlastRange;
		Fan->FanInnerRadius = 0.f;
		Fan->RingCount = 1;
		Fan->TelegraphDuration = InstanceData.BlastTelegraph;
		Fan->Activate(InstanceData.BlastDamage, BossController);
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaBackstepHeartPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	RecordSmallPatternEntered(Context, InstanceData.Boss);

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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
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
	RecordSmallPatternEntered(Context, InstanceData.Boss);
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

// ── 카운터 패턴 ──────────────────────────────────────────────

EStateTreeRunStatus FStateTreeTask_EchidnaCounterPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaCounter] EnterState 실패 — Boss NULL"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Phase = EEchidnaCounterPhase::Window;
	InstanceData.Elapsed = 0.f;
	InstanceData.ArmEnds.Reset();
	InstanceData.Clones.Reset();

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	// 플레이어를 바라보고 고정
	InstanceData.LockedYaw = Boss->GetActorRotation().Yaw;
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(Boss->GetWorld(), 0))
	{
		FVector ToPlayer = PlayerChar->GetActorLocation() - Boss->GetActorLocation();
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			InstanceData.LockedYaw = ToPlayer.Rotation().Yaw;
		}
	}
	Boss->SetActorRotation(FRotator(0.f, InstanceData.LockedYaw, 0.f));

	Boss->OpenCounterWindow();
	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaCounterPattern::StartSplit(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss->GetWorld();

	AHexArena* Arena = nullptr;
	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		Arena = *It;
		break;
	}

	// 갈래 4개 — 정면·오른쪽·뒤·왼쪽. 끝이 아레나 밖이면 안쪽으로 줄임(본체가 그 끝에 나타나야 하므로)
	InstanceData.Origin = Boss->GetActorLocation();
	InstanceData.ArmEnds.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Dir = FRotator(0.f, InstanceData.LockedYaw + 90.f * i, 0.f).Vector();
		float Distance = FMath::Max(InstanceData.SplitDistance, 0.f);
		FIntPoint Coord;
		while (Arena && Distance > 0.f && !(Arena->WorldToTileCoord(InstanceData.Origin + Dir * Distance, Coord) && Arena->GetTile(Coord)))
		{
			Distance -= 50.f;
		}
		InstanceData.ArmEnds.Add(InstanceData.Origin + Dir * FMath::Max(Distance, 0.f));
	}

	// 분신 — 보스 메시를 씌운 SkeletalMeshActor, 포즈는 숨긴 본체를 따라감
	USkeletalMeshComponent* BossMesh = Boss->GetMesh();
	InstanceData.CloneMeshStart = BossMesh ? BossMesh->GetComponentLocation() : InstanceData.Origin;
	InstanceData.CloneMeshYawOffset = BossMesh ? BossMesh->GetComponentRotation().Yaw - InstanceData.LockedYaw : 0.f;
	InstanceData.Clones.Reset();
	if (BossMesh && BossMesh->GetSkeletalMeshAsset())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 i = 0; i < 4; ++i)
		{
			const FRotator Rotation(0.f, InstanceData.LockedYaw + 90.f * i + InstanceData.CloneMeshYawOffset, 0.f);
			ASkeletalMeshActor* Clone = World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), InstanceData.CloneMeshStart, Rotation, Params);
			if (!Clone) continue;

			USkeletalMeshComponent* CloneMesh = Clone->GetSkeletalMeshComponent();
			CloneMesh->SetMobility(EComponentMobility::Movable);
			CloneMesh->SetSkeletalMesh(BossMesh->GetSkeletalMeshAsset());
			CloneMesh->SetWorldScale3D(BossMesh->GetComponentScale());
			CloneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			for (int32 MatIndex = 0; MatIndex < BossMesh->GetNumMaterials(); ++MatIndex)
			{
				CloneMesh->SetMaterial(MatIndex, BossMesh->GetMaterial(MatIndex));
			}
			CloneMesh->SetLeaderPoseComponent(BossMesh);
			InstanceData.Clones.Add(Clone);
		}
	}

	// 본체는 숨기고 충돌을 끈 채 중심에 둔다 — 충돌을 끄면 Walking은 바닥을 뚫고 떨어지므로 Flying
	Boss->SetActorHiddenInGame(true);
	Boss->SetActorEnableCollision(false);
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->StopMovementImmediately();
	}

	if (!InstanceData.TrailVFX)
	{
		InstanceData.TrailVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/LostArk/Raid/Echidna/Pattern/VFX/NS_EchidnaRetreatFan.NS_EchidnaRetreatFan"));
	}
	InstanceData.TrailComps.Reset();
	InstanceData.TrailCompAges.Reset();
	InstanceData.TrailSpawnedLength.Reset();

	InstanceData.Elapsed = 0.f;
	InstanceData.bDashHitDone = false;
	InstanceData.Phase = EEchidnaCounterPhase::SplitOut;
}

void FStateTreeTask_EchidnaCounterPattern::CheckDashHit(FInstanceDataType& InstanceData, const FVector& Prev, const FVector& Cur) const
{
	if (InstanceData.bDashHitDone || !InstanceData.Boss) return;

	ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(InstanceData.Boss->GetWorld(), 0));
	if (!Player) return;

	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector Closest = FMath::ClosestPointOnSegment(PlayerLoc, Prev, Cur);
	float Radius = InstanceData.DashHitRadius;
	if (const UCapsuleComponent* Capsule = Player->GetCapsuleComponent())
	{
		Radius += Capsule->GetScaledCapsuleRadius();
	}
	if (FVector::Dist2D(PlayerLoc, Closest) > Radius || FMath::Abs(PlayerLoc.Z - Closest.Z) > 300.f)
	{
		return;
	}

	InstanceData.bDashHitDone = true;
	UGameplayStatics::ApplyDamage(Player, InstanceData.DashDamage, InstanceData.Boss->GetController(), InstanceData.Boss, UDamageType::StaticClass());

	// 넉다운은 SourceLocation 반대쪽으로 튕기므로 캐릭터 뒤쪽 지점을 넘겨 돌진 방향으로 밀려나게
	FVector DashDir = (Cur - Prev).GetSafeNormal2D();
	if (DashDir.IsNearlyZero())
	{
		DashDir = (PlayerLoc - Closest).GetSafeNormal2D();
	}
	Player->ApplyKnockdown(PlayerLoc - DashDir * 300.f);
}

void FStateTreeTask_EchidnaCounterPattern::DestroyClones(FInstanceDataType& InstanceData) const
{
	for (AActor* Clone : InstanceData.Clones)
	{
		if (IsValid(Clone))
		{
			Clone->Destroy();
		}
	}
	InstanceData.Clones.Reset();
}

void FStateTreeTask_EchidnaCounterPattern::SpawnTrailSegment(FInstanceDataType& InstanceData, int32 ArmIndex, float FromLength, float ToLength) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss || !InstanceData.TrailVFX || !InstanceData.ArmEnds.IsValidIndex(ArmIndex) || ToLength - FromLength < 1.f) return;

	const FVector ArmVec = InstanceData.ArmEnds[ArmIndex] - InstanceData.Origin;
	if (ArmVec.IsNearlyZero()) return;

	// 캡슐 중심 → 바닥
	FVector Floor = InstanceData.Origin;
	if (const UCapsuleComponent* Capsule = Boss->GetCapsuleComponent())
	{
		Floor.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}
	const FTransform ArmXform(FRotator(0.f, ArmVec.Rotation().Yaw, 0.f), Floor);

	TArray<UNiagaraComponent*> Spawned;
	PatternVFX::SpawnRectArea(Boss, InstanceData.TrailVFX, ArmXform, FromLength, ToLength,
		InstanceData.DashHitRadius, InstanceData.TrailVFXSpacing, InstanceData.TrailVFXScale, 60, &Spawned);
	for (UNiagaraComponent* Comp : Spawned)
	{
		InstanceData.TrailComps.Add(Comp);
		InstanceData.TrailCompAges.Add(0.f);
	}
}

void FStateTreeTask_EchidnaCounterPattern::TickTrail(FInstanceDataType& InstanceData, float ScaledDelta, float SplitAlpha) const
{
	// 깐 지 TrailHoldTime이 지난 이펙트는 불꽃이 올라온 상태로 멈춰 둠 (버스트형이라 그냥 두면 1초 뒤 꺼짐)
	for (int32 i = 0; i < InstanceData.TrailComps.Num(); ++i)
	{
		UNiagaraComponent* Comp = InstanceData.TrailComps[i];
		if (!IsValid(Comp) || !InstanceData.TrailCompAges.IsValidIndex(i) || InstanceData.TrailCompAges[i] < 0.f) continue;

		InstanceData.TrailCompAges[i] += ScaledDelta;
		if (InstanceData.TrailCompAges[i] >= InstanceData.TrailHoldTime)
		{
			Comp->SetPaused(true);
			InstanceData.TrailCompAges[i] = -1.f;
		}
	}

	// 분신이 나아간 만큼만 이어서 깖 — 한 자리에 한 번만
	const float Step = FMath::Max(InstanceData.TrailVFXSpacing, 30.f);
	InstanceData.TrailSpawnedLength.SetNumZeroed(InstanceData.ArmEnds.Num());
	for (int32 i = 0; i < InstanceData.ArmEnds.Num(); ++i)
	{
		const float Reached = FVector::Dist2D(InstanceData.Origin, InstanceData.ArmEnds[i]) * SplitAlpha;
		float& Spawned = InstanceData.TrailSpawnedLength[i];
		if (Reached - Spawned >= Step || (SplitAlpha >= 1.f && Reached - Spawned >= 1.f))
		{
			SpawnTrailSegment(InstanceData, i, Spawned, Reached);
			Spawned = Reached;
		}
	}
}

void FStateTreeTask_EchidnaCounterPattern::ClearTrail(FInstanceDataType& InstanceData) const
{
	// 멈춰 둔 재생을 풀고 끔 — 남은 파티클이 자연스럽게 사그라짐
	for (UNiagaraComponent* Comp : InstanceData.TrailComps)
	{
		if (IsValid(Comp))
		{
			Comp->SetPaused(false);
			Comp->Deactivate();
		}
	}
	InstanceData.TrailComps.Reset();
	InstanceData.TrailCompAges.Reset();
	InstanceData.TrailSpawnedLength.Reset();
}

void FStateTreeTask_EchidnaCounterPattern::RestoreBoss(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss) return;

	Boss->SetActorHiddenInGame(false);
	Boss->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaCounterPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss) return EStateTreeRunStatus::Failed;

	const float ScaledDelta = DeltaTime * AEchidnaBoss::GetEnrageTimeScale(Boss);

	// 전 구간 이동 입력 없음 + 방향 고정 (돌아올 땐 LockedYaw가 돌진 방향으로 바뀜)
	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	Boss->SetActorRotation(FRotator(0.f, InstanceData.LockedYaw, 0.f));

	switch (InstanceData.Phase)
	{
	case EEchidnaCounterPhase::Window:
		// 카운터 성공은 스킬 쪽(TryCounter)에서 일어나므로 여기선 그 결과(그로기)만 본다.
		// 보스 공용 CounterGroggyDuration 대신 이 패턴의 시간으로 다시 건다(이미 그로기면 시간만 갱신)
		if (Boss->IsGroggy())
		{
			Boss->StartGroggy(InstanceData.GroggyDuration);
			InstanceData.Phase = EEchidnaCounterPhase::Groggy;
			return EStateTreeRunStatus::Running;
		}

		InstanceData.Elapsed += ScaledDelta;
		if (InstanceData.Elapsed >= InstanceData.CounterWindowTime)
		{
			Boss->CloseCounterWindow();
			UE_LOG(LogLoA, Log, TEXT("[EchidnaCounter] 카운터 실패 — + 모양 분신 돌진"));
			StartSplit(InstanceData);
		}
		return EStateTreeRunStatus::Running;

	case EEchidnaCounterPhase::Groggy:
		if (!Boss->IsGroggy())
		{
			InstanceData.Phase = EEchidnaCounterPhase::Done;
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;

	case EEchidnaCounterPhase::SplitOut:
	{
		const float Duration = FMath::Max(InstanceData.SplitDuration, KINDA_SMALL_NUMBER);
		const float PrevAlpha = FMath::Clamp(InstanceData.Elapsed / Duration, 0.f, 1.f);
		InstanceData.Elapsed += ScaledDelta;
		const float Alpha = FMath::Clamp(InstanceData.Elapsed / Duration, 0.f, 1.f);

		for (int32 i = 0; i < InstanceData.ArmEnds.Num(); ++i)
		{
			const FVector Prev = FMath::Lerp(InstanceData.Origin, InstanceData.ArmEnds[i], PrevAlpha);
			const FVector Cur = FMath::Lerp(InstanceData.Origin, InstanceData.ArmEnds[i], Alpha);
			CheckDashHit(InstanceData, Prev, Cur);
			if (InstanceData.Clones.IsValidIndex(i) && IsValid(InstanceData.Clones[i]))
			{
				InstanceData.Clones[i]->SetActorLocation(InstanceData.CloneMeshStart + (Cur - InstanceData.Origin));
			}
		}
		TickTrail(InstanceData, ScaledDelta, Alpha);
		if (Alpha < 1.f)
		{
			return EStateTreeRunStatus::Running;
		}

		// 분신이 사라지고 4갈래 끝 중 한 곳에 본체가 나타나 중심을 바라봄
		DestroyClones(InstanceData);
		InstanceData.ReturnArm = FMath::RandRange(0, FMath::Max(InstanceData.ArmEnds.Num() - 1, 0));
		const FVector ReturnStart = InstanceData.ArmEnds.IsValidIndex(InstanceData.ReturnArm) ? InstanceData.ArmEnds[InstanceData.ReturnArm] : InstanceData.Origin;
		const FVector ToOrigin = InstanceData.Origin - ReturnStart;
		if (!ToOrigin.IsNearlyZero())
		{
			InstanceData.LockedYaw = ToOrigin.Rotation().Yaw;
		}
		Boss->SetActorLocation(ReturnStart);
		Boss->SetActorRotation(FRotator(0.f, InstanceData.LockedYaw, 0.f));
		Boss->SetActorHiddenInGame(false);

		InstanceData.Elapsed = 0.f;
		InstanceData.bDashHitDone = false;
		InstanceData.Phase = EEchidnaCounterPhase::ReturnWait;
		return EStateTreeRunStatus::Running;
	}

	case EEchidnaCounterPhase::ReturnWait:
		TickTrail(InstanceData, ScaledDelta, 1.f);
		InstanceData.Elapsed += ScaledDelta;
		if (InstanceData.Elapsed >= InstanceData.ReturnDelay)
		{
			InstanceData.Elapsed = 0.f;
			InstanceData.Phase = EEchidnaCounterPhase::ReturnDash;
		}
		return EStateTreeRunStatus::Running;

	case EEchidnaCounterPhase::ReturnDash:
	{
		TickTrail(InstanceData, ScaledDelta, 1.f);
		const FVector ReturnStart = InstanceData.ArmEnds.IsValidIndex(InstanceData.ReturnArm) ? InstanceData.ArmEnds[InstanceData.ReturnArm] : InstanceData.Origin;
		const float Duration = FMath::Max(InstanceData.ReturnDuration, KINDA_SMALL_NUMBER);
		const float PrevAlpha = FMath::Clamp(InstanceData.Elapsed / Duration, 0.f, 1.f);
		InstanceData.Elapsed += ScaledDelta;
		const float Alpha = FMath::Clamp(InstanceData.Elapsed / Duration, 0.f, 1.f);

		const FVector Prev = FMath::Lerp(ReturnStart, InstanceData.Origin, PrevAlpha);
		const FVector Cur = FMath::Lerp(ReturnStart, InstanceData.Origin, Alpha);
		CheckDashHit(InstanceData, Prev, Cur);
		Boss->SetActorLocation(Cur);
		if (Alpha < 1.f)
		{
			return EStateTreeRunStatus::Running;
		}

		RestoreBoss(InstanceData);
		ClearTrail(InstanceData);
		InstanceData.Phase = EEchidnaCounterPhase::Done;
		return EStateTreeRunStatus::Succeeded;
	}

	default:
		return EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaCounterPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	// 중간에 끊겨도 발광·분신이 남지 않고 본체가 숨거나 떠 있지 않게
	if (InstanceData.Boss)
	{
		InstanceData.Boss->CloseCounterWindow();

		const EEchidnaCounterPhase Phase = InstanceData.Phase;
		if (Phase == EEchidnaCounterPhase::SplitOut || Phase == EEchidnaCounterPhase::ReturnWait || Phase == EEchidnaCounterPhase::ReturnDash)
		{
			InstanceData.Boss->SetActorLocation(InstanceData.Origin);
			RestoreBoss(InstanceData);
		}
	}
	DestroyClones(InstanceData);
	ClearTrail(InstanceData);
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaCounterPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaCounterPatternDesc", "<b>Echidna Counter Pattern</b>");
}
#endif // WITH_EDITOR

// ── 거울 카운터 (217줄) ──────────────────────────────────────────────

EStateTreeRunStatus FStateTreeTask_EchidnaMirrorCounterPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Walls.Reset();
	InstanceData.Elapsed = 0.f;
	InstanceData.EndElapsed = 0.f;
	InstanceData.Arena = nullptr;
	InstanceData.bIntroDone = false;
	InstanceData.IntroStep = 0;
	InstanceData.IntroElapsed = 0.f;
	InstanceData.IntroRow = nullptr;
	InstanceData.IntroCamera = nullptr;
	InstanceData.SideColumns.Reset();
	InstanceData.Barriers.Reset();
	InstanceData.bCleanedUp = false;
	InstanceData.bGroggyStarted = false;
	InstanceData.bCounterFailed = false;

	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorCounter] EnterState 실패 — Boss 바인딩 확인"));
		return EStateTreeRunStatus::Failed;
	}

	for (TActorIterator<AHexArena> It(World); It; ++It)
	{
		InstanceData.Arena = *It;
		break;
	}
	if (!InstanceData.Arena)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorCounter] 아레나 없음"));
		return EStateTreeRunStatus::Failed;
	}

	// 발동 표시 + 큰 패턴 잠금(정산 게이지 정지, 다른 큰 패턴이 끼어들지 않음)
	MarkBigPatternTriggered(Context, Boss, InstanceData.PatternName, TEXT("EchidnaMirrorCounter"));

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	Boss->GetCharacterMovement()->StopMovementImmediately();
	SetBossVanished(Boss, true);

	// 거울이 나올 변 — 한 패턴 내내 고정. 아레나 외곽은 큰 육각형이고 꼭짓점이 축 방향 타일(Yaw 0/60/...)에 있으므로
	// 변의 바깥 법선은 Yaw 30+60k. 줄은 그 변과 평행하게 서서 반대편 변까지 간다
	InstanceData.SideIndex = InstanceData.SpawnSide >= 0 ? InstanceData.SpawnSide % 6 : FMath::RandRange(0, 5);
	InstanceData.OutwardYaw = static_cast<float>(InstanceData.Arena->GetActorRotation().Yaw) + 30.f + 60.f * InstanceData.SideIndex;

	// 시점 전환은 여기서 한 번만 — 줄이 오는 변을 바라보는 비스듬한 구도(플레이어 뒤에서 거울 줄을 마주봄). 복구는 ExitState
	if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
	{
		Player->SetCameraZoomOverride(InstanceData.CameraArmLength);
		Player->SetCameraRotationOverride(FRotator(InstanceData.CameraPitch, InstanceData.OutwardYaw, 0.f));

		if (Player->SkillManager)
		{
			Player->SkillManager->SetCounterSkillCooldownOverride(InstanceData.CounterSkillCooldown);
		}
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirrorCounter] 패턴 시작 — %d웨이브, 변 %d"), InstanceData.WaveCount, InstanceData.SideIndex);

	// 시작 시네마틱 — 끝나야 첫 줄 타이머가 돈다. 꺼져 있으면 바로 기둥만 세우고 시작
	if (InstanceData.bPlayIntro)
	{
		StartIntro(InstanceData);
	}
	else
	{
		FinishIntro(InstanceData);
	}
	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaMirrorCounterPattern::SpawnWave(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	AHexArena* Arena = InstanceData.Arena;
	if (!Boss || !Arena) return;

	FVector Center, TravelDir, StartLocation;
	float StartDistance = 0.f, EdgeDistance = 0.f, RowWidth = 0.f;
	if (!ComputeRowGeometry(InstanceData, Center, TravelDir, StartLocation, StartDistance, EdgeDistance, RowWidth)) return;

	UClass* WallClass = InstanceData.MirrorWallClass ? InstanceData.MirrorWallClass.Get() : AEchidnaMirrorWallActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEchidnaMirrorWallActor* Wall = Boss->GetWorld()->SpawnActor<AEchidnaMirrorWallActor>(
		WallClass, StartLocation, TravelDir.Rotation(), SpawnParams);
	if (!Wall)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorCounter] 거울 줄 스폰 실패"));
		return;
	}

	// 앞 줄을 한 번이라도 놓쳤으면 이 줄은 카운터 거울 없이 나온다 — 기믹 실패 확정
	if (InstanceData.bCounterFailed)
	{
		Wall->DisableCounter();
	}

	// 정사각형 앞 선 → 뒤 선(플레이어가 더 못 물러나는 투명 벽)까지. 불길은 시작부터
	float HalfWidth = 0.f, Front = 0.f, Back = 0.f;
	GetSquareExtents(InstanceData, HalfWidth, Front, Back);
	Wall->Activate(Boss, Front + Back, 0.f, Boss->GetController(), RowWidth);
	InstanceData.Walls.Add(Wall);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirrorCounter] %d번째 줄"), InstanceData.Walls.Num());
}

EStateTreeRunStatus FStateTreeTask_EchidnaMirrorCounterPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss) return EStateTreeRunStatus::Failed;

	Boss->GetCharacterMovement()->StopMovementImmediately();

	// 마무리 그로기 — 끝날 때까지 State에 머물러 보스가 다음 행동(패트롤·패턴)을 못 하게
	if (InstanceData.bGroggyStarted)
	{
		return Boss->IsGroggy() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
	}

	// 시네마틱이 끝나야 패턴(줄 타이머) 시작
	if (!InstanceData.bIntroDone)
	{
		if (TickIntro(InstanceData, DeltaTime))
		{
			FinishIntro(InstanceData);
		}
		return EStateTreeRunStatus::Running;
	}

	InstanceData.Elapsed += DeltaTime;

	// 뒤 벽 VFX는 한 번 재생형이라 주기적으로 다시 깐다
	InstanceData.BackWallVFXElapsed += DeltaTime;
	if (InstanceData.BackWallVFXElapsed >= InstanceData.BackWallVFXRespawnInterval)
	{
		InstanceData.BackWallVFXElapsed = 0.f;
		SpawnBackWallVFX(InstanceData);
	}

	// 카운터 못 치고 끝까지 간 줄 감지 (카운터로 쓰러진 줄은 IsCountered)
	for (const TObjectPtr<AEchidnaMirrorWallActor>& Wall : InstanceData.Walls)
	{
		if (IsValid(Wall) && Wall->IsFinished() && !Wall->IsCountered() && !InstanceData.bCounterFailed)
		{
			// 이미 나와 있는 뒷줄은 카운터 거울이 사라지고, 다음 줄은 더 안 나온다 — 남은 줄이 끝나면 패턴 종료(그로기 없음)
			InstanceData.bCounterFailed = true;
			for (const TObjectPtr<AEchidnaMirrorWallActor>& Other : InstanceData.Walls)
			{
				if (IsValid(Other) && !Other->IsFinished()) Other->RemoveCounterMirror();
			}
			UE_LOG(LogLoA, Log, TEXT("[EchidnaMirrorCounter] 카운터 실패 — 나와 있는 줄 카운터 제거, 다음 줄 없음"));
		}
	}

	// 다음 줄 — 정해진 시간마다 (앞 줄을 일찍 카운터쳤어도 당기지 않는다)
	const int32 Spawned = InstanceData.Walls.Num();
	if (!InstanceData.bCounterFailed && Spawned < InstanceData.WaveCount
		&& InstanceData.Elapsed >= InstanceData.FirstWaveDelay + InstanceData.WaveInterval * Spawned)
	{
		SpawnWave(InstanceData);
	}

	if (!InstanceData.bCounterFailed && InstanceData.Walls.Num() < InstanceData.WaveCount)
	{
		return EStateTreeRunStatus::Running;
	}

	// 전부 나왔으면 마지막 불길까지 꺼질 때까지 대기 (스스로 소멸한 줄은 끝난 것으로 본다)
	for (const TObjectPtr<AEchidnaMirrorWallActor>& Wall : InstanceData.Walls)
	{
		if (IsValid(Wall) && !Wall->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}
	}

	InstanceData.EndElapsed += DeltaTime;
	if (InstanceData.EndElapsed < InstanceData.EndDelay)
	{
		return EStateTreeRunStatus::Running;
	}

	// 그로기 조건 = 모든 줄을 카운터쳤는가. 한 줄이라도 놓치면 이후 줄은 카운터 거울이 없으므로 마지막 줄 성공 = 전부 성공이지만,
	// 결과는 줄 전부로 확인한다. 정리하면 줄이 파괴되므로 그 전에 본다
	const AEchidnaMirrorWallActor* LastWall = InstanceData.Walls.Num() > 0 ? InstanceData.Walls.Last().Get() : nullptr;
	bool bSuccess = !InstanceData.bCounterFailed && IsValid(LastWall);
	for (const TObjectPtr<AEchidnaMirrorWallActor>& Wall : InstanceData.Walls)
	{
		bSuccess &= IsValid(Wall) && Wall->IsCountered();
	}
	const FVector CounteredLocation = bSuccess ? LastWall->GetCounteredMirrorLocation() : FVector::ZeroVector;
	const float CounteredYaw = bSuccess ? LastWall->GetActorRotation().Yaw : 0.f;

	// 패턴 끝 — 줄·카메라를 먼저 정리해 보스가 나타난 상태로 그로기에 들어간다
	CleanupPattern(InstanceData);
	if (bSuccess)
	{
		// 마지막으로 카운터 맞은 거울 자리에서, 거울이 보던 방향(줄 진행 방향)을 보고 나타난다
		const FVector BossLocation = CounteredLocation + FVector(0.f, 0.f, Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Boss->SetActorLocationAndRotation(BossLocation, FRotator(0.f, CounteredYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (InstanceData.GroggyDuration > 0.f && (bSuccess || !InstanceData.bGroggyOnlyOnSuccess))
	{
		Boss->StartGroggy(InstanceData.GroggyDuration);
		InstanceData.bGroggyStarted = true;
		return EStateTreeRunStatus::Running;
	}
	return EStateTreeRunStatus::Succeeded;
}

bool FStateTreeTask_EchidnaMirrorCounterPattern::ComputeRowGeometry(const FInstanceDataType& InstanceData, FVector& OutCenter, FVector& OutTravelDir,
	FVector& OutStartLocation, float& OutStartDistance, float& OutEdgeDistance, float& OutRowWidth) const
{
	AHexArena* Arena = InstanceData.Arena;
	if (!Arena) return false;

	if (!Arena->GetTileTopLocation(FIntPoint(0, 0), OutCenter))
	{
		OutCenter = Arena->GetActorLocation();
	}

	// 모든 웨이브가 EnterState에서 정한 같은 변에서 나온다
	const FVector Outward = FRotator(0.f, InstanceData.OutwardYaw, 0.f).Vector();
	OutTravelDir = -Outward;

	// 중심 → 외곽선 거리 = 외곽 타일 줄 중심까지(R*D*√3/2) + 타일 내접원 반지름(TileSpacing/2)
	const float D = Arena->TileSpacing + Arena->HexGap;
	OutEdgeDistance = (Arena->SideCount - 1) * D * 0.8660254f + Arena->TileSpacing * 0.5f;

	// 줄은 정사각형 앞 선에서 나타나고 폭 = 정사각형 폭 (거울 7개가 맞닿게 크기를 맞춤)
	float HalfWidth = 0.f, Front = 0.f, Back = 0.f;
	GetSquareExtents(InstanceData, HalfWidth, Front, Back);
	OutStartDistance = Front;
	OutStartLocation = OutCenter + Outward * Front;
	OutRowWidth = 2.f * HalfWidth;
	return true;
}

bool FStateTreeTask_EchidnaMirrorCounterPattern::GetSquareExtents(const FInstanceDataType& InstanceData, float& OutHalfWidth, float& OutFront, float& OutBack) const
{
	const AHexArena* Arena = InstanceData.Arena;
	if (!Arena) return false;
	const float D = Arena->TileSpacing + Arena->HexGap;
	OutHalfWidth = InstanceData.SquareHalfWidthTiles * D;
	OutFront = InstanceData.SquareFrontTiles * D;
	OutBack = InstanceData.SquareBackTiles * D;
	return true;
}

void FStateTreeTask_EchidnaMirrorCounterPattern::SpawnBackWallVFX(FInstanceDataType& InstanceData) const
{
	// 뒤 벽 = Barriers[1] (SpawnSquareBarriers의 Specs 순서: 앞·뒤·오른쪽·왼쪽)
	AEchidnaBarrierActor* BackBarrier = InstanceData.Barriers.IsValidIndex(1) ? InstanceData.Barriers[1].Get() : nullptr;
	UNiagaraSystem* BackVFX = BackBarrier ? InstanceData.BackWallRayVFX.LoadSynchronous() : nullptr;
	FVector Center, TravelDir, StartLocation;
	float StartDistance = 0.f, EdgeDistance = 0.f, RowWidth = 0.f;
	float HalfWidth = 0.f, Front = 0.f, Back = 0.f;
	if (!BackVFX || !ComputeRowGeometry(InstanceData, Center, TravelDir, StartLocation, StartDistance, EdgeDistance, RowWidth)
		|| !GetSquareExtents(InstanceData, HalfWidth, Front, Back)) return;

	const FVector Right = FVector::CrossProduct(FVector::UpVector, TravelDir).GetSafeNormal();
	const float Spacing = FMath::Max(InstanceData.BackWallVFXSpacing, 20.f);
	const int32 Count = FMath::Max(2, FMath::CeilToInt(2.f * HalfWidth / Spacing) + 1);
	const FVector LineStart = Center + TravelDir * Back - Right * HalfWidth + FVector(0.f, 0.f, InstanceData.BackWallVFXHeight);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Location = LineStart + Right * (2.f * HalfWidth * i / (Count - 1));
		UNiagaraFunctionLibrary::SpawnSystemAttached(BackVFX, BackBarrier->GetRootComponent(), NAME_None, Location, TravelDir.Rotation(),
			FVector(InstanceData.BackWallVFXScale), EAttachLocation::KeepWorldPosition, true, ENCPoolMethod::None);
	}
}

void FStateTreeTask_EchidnaMirrorCounterPattern::SpawnSquareBarriers(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	FVector Center, TravelDir, StartLocation;
	float StartDistance = 0.f, EdgeDistance = 0.f, RowWidth = 0.f;
	float HalfWidth = 0.f, Front = 0.f, Back = 0.f;
	if (!World || !ComputeRowGeometry(InstanceData, Center, TravelDir, StartLocation, StartDistance, EdgeDistance, RowWidth)
		|| !GetSquareExtents(InstanceData, HalfWidth, Front, Back)) return;

	const FVector Outward = -TravelDir;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, TravelDir).GetSafeNormal();
	const FRotator Facing = TravelDir.Rotation();
	const float HalfT = InstanceData.BarrierThickness * 0.5f;
	const float HalfH = InstanceData.BarrierHeight * 0.5f;
	const float MidAlong = (Front - Back) * 0.5f;	// 정사각형 중심의 앞뒤 위치 (Outward 기준)
	const float HalfDepth = (Front + Back) * 0.5f;

	// 박스 로컬 X = 진행 방향, Y = 좌우. 벽 두께는 정사각형 바깥쪽으로 둔다
	struct FWallSpec { FVector Offset; FVector HalfExtent; };
	const FWallSpec Specs[] = {
		{ Outward * (Front + HalfT),  FVector(HalfT, HalfWidth + 2.f * HalfT, HalfH) },	// 앞
		{ -Outward * (Back + HalfT),  FVector(HalfT, HalfWidth + 2.f * HalfT, HalfH) },	// 뒤
		{ Outward * MidAlong + Right * (HalfWidth + HalfT),  FVector(HalfDepth + 2.f * HalfT, HalfT, HalfH) },	// 오른쪽
		{ Outward * MidAlong - Right * (HalfWidth + HalfT),  FVector(HalfDepth + 2.f * HalfT, HalfT, HalfH) },	// 왼쪽
	};

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FWallSpec& Spec : Specs)
	{
		if (AEchidnaBarrierActor* Barrier = World->SpawnActor<AEchidnaBarrierActor>(AEchidnaBarrierActor::StaticClass(),
			Center + Spec.Offset + FVector(0.f, 0.f, HalfH), Facing, SpawnParams))
		{
			Barrier->SetHalfExtent(Spec.HalfExtent);
			InstanceData.Barriers.Add(Barrier);
		}
	}

	// 뒤 선 투명 벽 표시 — 이후엔 Tick에서 BackWallVFXRespawnInterval마다 다시 깐다
	InstanceData.BackWallVFXElapsed = 0.f;
	SpawnBackWallVFX(InstanceData);

	// 구역 밖에 있으면 벽 바깥에 갇히므로 정사각형 가운데로 옮긴다
	if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
	{
		const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const FVector Local = Player->GetActorLocation() - Center;
		const float Along = FVector::DotProduct(Local, Outward);
		const float Side = FVector::DotProduct(Local, Right);
		if (Along > Front - Radius || Along < -Back + Radius || FMath::Abs(Side) > HalfWidth - Radius)
		{
			FVector Target = Center + Outward * MidAlong;
			Target.Z = Player->GetActorLocation().Z;
			Player->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void FStateTreeTask_EchidnaMirrorCounterPattern::StartIntro(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	FVector Center, TravelDir, StartLocation;
	float StartDistance = 0.f, EdgeDistance = 0.f, RowWidth = 0.f;
	if (!World || !ComputeRowGeometry(InstanceData, Center, TravelDir, StartLocation, StartDistance, EdgeDistance, RowWidth))
	{
		InstanceData.IntroStep = 3;	// 바로 끝내기
		return;
	}

	// 연출용 줄 — 실제 줄이 나타날 정사각형 앞 선. 땅속에 숨겨 두었다가 바닥을 뚫고 솟아오른다
	const FVector IntroRowLocation = StartLocation;
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* WallClass = InstanceData.MirrorWallClass ? InstanceData.MirrorWallClass.Get() : AEchidnaMirrorWallActor::StaticClass();
	InstanceData.IntroRow = World->SpawnActor<AEchidnaMirrorWallActor>(WallClass, IntroRowLocation, TravelDir.Rotation(), SpawnParams);
	if (InstanceData.IntroRow)
	{
		InstanceData.IntroRow->SetupDisplayRow(RowWidth);
	}

	// 줌인 카메라 — 줄 정면(진행 방향 쪽)에서 거울 가운데를 바라봄
	const FVector CamLocation = IntroRowLocation + TravelDir * InstanceData.IntroCameraDistance + FVector(0.f, 0.f, InstanceData.IntroCameraHeight);
	const FVector LookAt = IntroRowLocation + FVector(0.f, 0.f, InstanceData.IntroRow ? InstanceData.IntroRow->MirrorHeight * 0.5f : 190.f);
	InstanceData.IntroCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamLocation, (LookAt - CamLocation).Rotation(), SpawnParams);
	if (InstanceData.IntroCamera && InstanceData.IntroCamera->GetCameraComponent())
	{
		InstanceData.IntroCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}

	if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
	{
		// 연출 동안 조작 불가
		Player->SetHeldByPattern(true);
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			if (InstanceData.IntroCamera)
			{
				PC->SetViewTargetWithBlend(InstanceData.IntroCamera, InstanceData.IntroBlendInTime, VTBlend_Cubic);
			}
		}
	}

	InstanceData.IntroStep = 0;
	InstanceData.IntroElapsed = 0.f;
}

bool FStateTreeTask_EchidnaMirrorCounterPattern::TickIntro(FInstanceDataType& InstanceData, float DeltaTime) const
{
	InstanceData.IntroElapsed += DeltaTime;
	AEchidnaMirrorWallActor* Row = InstanceData.IntroRow;

	switch (InstanceData.IntroStep)
	{
	case 0:	// 줌인
		if (InstanceData.IntroElapsed >= InstanceData.IntroBlendInTime)
		{
			if (Row)
			{
				Row->PlayAppear(InstanceData.IntroAppearStagger, InstanceData.IntroRiseDuration, InstanceData.IntroSpinTurns);
			}
			InstanceData.IntroStep = 1;
			InstanceData.IntroElapsed = 0.f;
		}
		return false;

	case 1:	// 솟아오름 → 정지
	{
		const int32 Count = Row ? Row->MirrorCount : 0;
		const float AppearTime = InstanceData.IntroAppearStagger * FMath::Max(0, Count - 1) + InstanceData.IntroRiseDuration;
		if (InstanceData.IntroElapsed >= AppearTime + InstanceData.IntroHoldTime)
		{
			// 빠르게 줌아웃(플레이어 카메라로) + 동시에 거울이 오른쪽부터 차례로 사라짐
			if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(InstanceData.Boss ? InstanceData.Boss->GetWorld() : nullptr, 0)))
			{
				if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
				{
					PC->SetViewTargetWithBlend(Player, InstanceData.IntroZoomOutTime, VTBlend_EaseInOut, 2.f);
				}
			}
			if (Row)
			{
				Row->PlayExit(InstanceData.IntroExitStagger, InstanceData.IntroExitSlideDuration, InstanceData.IntroExitSlideDistance);
			}
			InstanceData.IntroStep = 2;
			InstanceData.IntroElapsed = 0.f;
		}
		return false;
	}

	case 2:	// 줌아웃 + 퇴장
		return InstanceData.IntroElapsed >= InstanceData.IntroZoomOutTime && (!Row || !Row->IsDisplayAnimating());

	default:
		return true;
	}
}

void FStateTreeTask_EchidnaMirrorCounterPattern::FinishIntro(FInstanceDataType& InstanceData) const
{
	InstanceData.bIntroDone = true;

	if (IsValid(InstanceData.IntroRow))
	{
		InstanceData.IntroRow->Destroy();
	}
	InstanceData.IntroRow = nullptr;

	if (IsValid(InstanceData.IntroCamera))
	{
		// 블렌드 마지막 프레임에 카메라 매니저가 참조할 수 있어 바로 지우지 않는다 (레이드 인트로와 같은 방식)
		InstanceData.IntroCamera->SetLifeSpan(0.5f);
	}
	InstanceData.IntroCamera = nullptr;

	if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(InstanceData.Boss ? InstanceData.Boss->GetWorld() : nullptr, 0)))
	{
		if (InstanceData.bPlayIntro)
		{
			Player->SetHeldByPattern(false);
		}
	}

	if (InstanceData.bSpawnSideColumns)
	{
		SpawnSideColumns(InstanceData);
	}
	if (InstanceData.bBlockSquare)
	{
		SpawnSquareBarriers(InstanceData);
	}
}

void FStateTreeTask_EchidnaMirrorCounterPattern::SpawnSideColumns(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	FVector Center, TravelDir, StartLocation;
	float StartDistance = 0.f, EdgeDistance = 0.f, RowWidth = 0.f;
	if (!World || !ComputeRowGeometry(InstanceData, Center, TravelDir, StartLocation, StartDistance, EdgeDistance, RowWidth)) return;

	float HalfWidth = 0.f, Front = 0.f, Back = 0.f;
	if (!GetSquareExtents(InstanceData, HalfWidth, Front, Back)) return;

	UClass* WallClass = InstanceData.MirrorWallClass ? InstanceData.MirrorWallClass.Get() : AEchidnaMirrorWallActor::StaticClass();
	const AEchidnaMirrorWallActor* CDO = GetDefault<AEchidnaMirrorWallActor>(WallClass);

	// 기둥 거울 = 줄 거울과 같은 크기, 서로 맞닿게. 길이는 정사각형 앞 선 ~ 뒤 선을 덮도록
	const float MirrorWidth = RowWidth / FMath::Max(1, CDO->MirrorCount);
	const float Depth = Front + Back;
	const int32 Count = FMath::Max(2, FMath::CeilToInt(Depth / MirrorWidth));
	const FVector Right = FVector::CrossProduct(FVector::UpVector, TravelDir).GetSafeNormal();
	const FVector SquareMid = Center - TravelDir * ((Front - Back) * 0.5f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const float Side : { -1.f, 1.f })
	{
		// 줄 양 끝 거울 바깥 가장자리 + 간격. 기둥 거울은 안쪽(줄이 지나가는 통로)을 바라본다
		const FVector Location = SquareMid + Right * Side * (RowWidth * 0.5f + InstanceData.SideColumnGap);
		const FRotator Facing = (-Right * Side).Rotation();
		AEchidnaMirrorWallActor* Column = World->SpawnActor<AEchidnaMirrorWallActor>(WallClass, Location, Facing, SpawnParams);
		if (!Column) continue;

		// 상반신 없는 투명 유리 거울
		Column->FitMirrorSize(MirrorWidth);
		Column->SetupDisplayRow(-1.f, Count, MirrorWidth, true, true);
		// 패턴 시작과 함께 빠르게 솟아오름 (돌지 않고)
		Column->PlayAppear(0.04f, 0.4f, 0.f);
		InstanceData.SideColumns.Add(Column);
	}
}

void FStateTreeTask_EchidnaMirrorCounterPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 그로기 단계에서 끝났으면 이미 정리됨 — 중간에 끊겼을 때만 여기서 정리
	CleanupPattern(InstanceData);

	// 큰 패턴 잠금은 그로기까지 끝난 뒤에 푼다 (그로기 도중 다른 큰 패턴이 끼어들지 않게)
	if (AEchidnaBoss* Boss = InstanceData.Boss)
	{
		Boss->ClearActiveTimedPattern();
	}
	InstanceData.Arena = nullptr;
}

int32 FStateTreeTask_EchidnaMirrorCounterPattern::CleanupPattern(FInstanceDataType& InstanceData) const
{
	if (InstanceData.bCleanedUp) return -1;
	InstanceData.bCleanedUp = true;

	AEchidnaBoss* Boss = InstanceData.Boss;

	// 줄은 스스로 소멸하지 않으므로 여기서 결과를 센 뒤 파괴 (중간에 끊겨도 남은 줄·불길 정리)
	int32 Countered = 0;
	for (const TObjectPtr<AEchidnaMirrorWallActor>& Wall : InstanceData.Walls)
	{
		if (IsValid(Wall))
		{
			if (Wall->IsCountered()) ++Countered;
			Wall->Destroy();
		}
	}
	InstanceData.Walls.Reset();

	// 시네마틱·기둥 정리 — 중간에 끊겨도 카메라·거울·붙잡힘이 남지 않게
	if (IsValid(InstanceData.IntroRow)) InstanceData.IntroRow->Destroy();
	InstanceData.IntroRow = nullptr;
	for (const TObjectPtr<AEchidnaMirrorWallActor>& Column : InstanceData.SideColumns)
	{
		if (IsValid(Column)) Column->Destroy();
	}
	InstanceData.SideColumns.Reset();
	for (const TObjectPtr<AEchidnaBarrierActor>& Barrier : InstanceData.Barriers)
	{
		if (IsValid(Barrier)) Barrier->Destroy();
	}
	InstanceData.Barriers.Reset();
	if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(Boss ? Boss->GetWorld() : nullptr, 0)))
	{
		if (!InstanceData.bIntroDone)
		{
			Player->SetHeldByPattern(false);
		}
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			if (InstanceData.IntroCamera && PC->GetViewTarget() == InstanceData.IntroCamera)
			{
				PC->SetViewTargetWithBlend(Player, 0.2f);
			}
		}
	}
	if (IsValid(InstanceData.IntroCamera))
	{
		InstanceData.IntroCamera->SetLifeSpan(0.5f);
	}
	InstanceData.IntroCamera = nullptr;

	if (Boss)
	{
		SetBossVanished(Boss, false);

		if (ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(Boss->GetWorld(), 0)))
		{
			Player->ClearCameraZoomOverride();
			Player->ClearCameraRotationOverride();

			if (Player->SkillManager)
			{
				Player->SkillManager->ClearCounterSkillCooldownOverride();
			}
		}
	}

	const int32 Total = InstanceData.WaveCount;
	if (Countered >= Total)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorCounter] 파훼 성공 — 카운터 %d / %d"), Countered, Total);
	}
	else
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaMirrorCounter] 파훼 실패 — 카운터 %d / %d"), Countered, Total);
	}
	return Countered;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaMirrorCounterPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaMirrorCounterPatternDesc", "<b>Echidna Mirror Counter Pattern</b>");
}
#endif // WITH_EDITOR

bool FStateTreeTask_EchidnaRetreatLotusPattern::IsOnArena(const FInstanceDataType& InstanceData, const FVector& Location) const
{
	FIntPoint Coord;
	return InstanceData.Arena && InstanceData.Arena->WorldToTileCoord(Location, Coord) && InstanceData.Arena->GetTile(Coord);
}

bool FStateTreeTask_EchidnaRetreatLotusPattern::StartCycle(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World) return false;

	// 사이클마다 그 순간의 플레이어 방향으로 다시 조준하고, 사이클 동안은 고정
	InstanceData.CycleYaw = Boss->GetActorRotation().Yaw;
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		FVector ToPlayer = PlayerChar->GetActorLocation() - Boss->GetActorLocation();
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			InstanceData.CycleYaw = ToPlayer.Rotation().Yaw;
		}
	}
	const FRotator Facing(0.f, InstanceData.CycleYaw, 0.f);
	Boss->SetActorRotation(Facing);

	// 도착 지점 — 아레나 밖으로 나가면 안쪽으로 줄인다
	InstanceData.SlideStart = Boss->GetActorLocation();
	const FVector Back = -Facing.Vector();
	float Distance = FMath::Max(InstanceData.SlideDistance, 0.f);
	while (Distance > 0.f && !IsOnArena(InstanceData, InstanceData.SlideStart + Back * Distance))
	{
		Distance -= 50.f;
	}
	Distance = FMath::Max(Distance, 0.f);
	InstanceData.SlideEnd = InstanceData.SlideStart + Back * Distance;

	// 부채꼴 — 도착 지점(발밑)에서 정면으로. 예고 시간 = 슬라이드 시간이라 도착하는 순간 터진다
	FVector FanOrigin = InstanceData.SlideEnd;
	if (const UCapsuleComponent* Capsule = Boss->GetCapsuleComponent())
	{
		FanOrigin.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	UClass* FanClass = InstanceData.FanZoneClass ? InstanceData.FanZoneClass.Get() : AEchidnaFanZoneActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEchidnaFanZoneActor* Fan = World->SpawnActor<AEchidnaFanZoneActor>(FanClass, FanOrigin, Facing, SpawnParams);
	if (!Fan)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRetreatLotus] 부채꼴 SpawnActor 실패"));
		return false;
	}
	Fan->FanAngle = InstanceData.FanAngle;
	Fan->FanRange = InstanceData.FanRange;
	Fan->FanInnerRadius = 0.f;
	Fan->RingCount = FMath::Max(InstanceData.FanRingCount, 1);
	Fan->RingInterval = 0.08f;
	Fan->TelegraphDuration = InstanceData.SlideDuration;
	if (!InstanceData.FanZoneClass)
	{
		// 맞으면 경직 + 매혹 1스택 (그림 9번)
		Fan->bApplyKnockdownOnHit = false;
		Fan->bApplyStaggerOnHit = true;
		Fan->bApplyCharmGaugeOnHit = true;
		Fan->CharmGaugePerHit = 1;
	}
	Fan->Activate(InstanceData.FanDamage, Boss->GetController());
	InstanceData.CurrentFan = Fan;

	// 미끄러지는 동안은 위치를 직접 보간 — 중력·바닥 보정이 끼어들지 않게 Flying
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	if (UCharacterMovementComponent* Movement = Boss->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
	}

	InstanceData.PhaseElapsed = 0.f;
	InstanceData.LotusSpawnedThisCycle = 0;
	InstanceData.Phase = EEchidnaRetreatLotusPhase::Sliding;

	UE_LOG(LogLoA, Log, TEXT("[EchidnaRetreatLotus] %d번째 사이클 — Yaw=%.0f 슬라이드 %.0fcm"),
		InstanceData.CycleIndex + 1, InstanceData.CycleYaw, Distance);
	return true;
}

void FStateTreeTask_EchidnaRetreatLotusPattern::SpawnLotusNearBoss(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World || !InstanceData.Arena) return;

	// 보스 주변 랜덤 위치 — 아레나 타일 위가 나올 때까지 몇 번 다시 뽑는다
	FVector Location = FVector::ZeroVector;
	bool bFound = false;
	for (int32 Try = 0; Try < 8 && !bFound; ++Try)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::FRandRange(InstanceData.LotusMinDistance, FMath::Max(InstanceData.LotusMinDistance, InstanceData.LotusMaxDistance));
		const FVector Candidate = Boss->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Dist;

		FIntPoint Coord;
		FVector Top;
		if (InstanceData.Arena->WorldToTileCoord(Candidate, Coord) && InstanceData.Arena->GetTileTopLocation(Coord, Top))
		{
			Location = FVector(Candidate.X, Candidate.Y, Top.Z);
			bFound = true;
		}
	}
	if (!bFound) return;

	const FTransform Xform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location);

	// 연꽃 비주얼 — 랜잡의 작은 연꽃과 같은 방식(큰 꽃 액터를 작게, 바닥 잎 없이)
	AEchidnaBigFlowerActor* Flower = World->SpawnActorDeferred<AEchidnaBigFlowerActor>(
		AEchidnaBigFlowerActor::StaticClass(), Xform, Boss, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Flower)
	{
		Flower->PetalLength = InstanceData.LotusRadius;
		Flower->LeafCount = 0;
		Flower->FinishSpawning(Xform);
	}

	// 폭발 — 360도 원형 장판, 연꽃 자체가 예고라 메시는 투명
	AEchidnaFanZoneActor* Zone = World->SpawnActorDeferred<AEchidnaFanZoneActor>(
		AEchidnaFanZoneActor::StaticClass(), Xform, Boss, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Zone)
	{
		if (Flower) Flower->Destroy();
		return;
	}
	Zone->FanAngle = 360.f;
	Zone->FanInnerRadius = 0.f;
	Zone->FanRange = InstanceData.LotusRadius;
	Zone->RingCount = 1;
	Zone->TelegraphDuration = InstanceData.LotusFuseTime;
	Zone->TelegraphOpacity = 0.f;
	Zone->bApplyKnockdownOnHit = false;
	Zone->bApplyStaggerOnHit = true;
	Zone->bApplyCharmGaugeOnHit = true;
	Zone->CharmGaugePerHit = 1;
	Zone->FinishSpawning(Xform);
	Zone->Activate(InstanceData.LotusDamage, Boss->GetController());

	InstanceData.LotusFlowers.Add(Flower);
	InstanceData.LotusZones.Add(Zone);
}

void FStateTreeTask_EchidnaRetreatLotusPattern::TickLotus(FInstanceDataType& InstanceData) const
{
	// 터지는 순간 연꽃(비주얼)은 사라지고 폭발 이펙트만 남는다
	for (int32 i = InstanceData.LotusZones.Num() - 1; i >= 0; i--)
	{
		AEchidnaFanZoneActor* Zone = InstanceData.LotusZones[i];
		if (!IsValid(Zone) || Zone->HasStartedExploding())
		{
			if (InstanceData.LotusFlowers.IsValidIndex(i) && IsValid(InstanceData.LotusFlowers[i]))
			{
				InstanceData.LotusFlowers[i]->Destroy();
			}
			InstanceData.LotusZones.RemoveAtSwap(i);
			if (InstanceData.LotusFlowers.IsValidIndex(i))
			{
				InstanceData.LotusFlowers.RemoveAtSwap(i);
			}
		}
	}
}

void FStateTreeTask_EchidnaRetreatLotusPattern::EndSlide(FInstanceDataType& InstanceData) const
{
	if (!InstanceData.Boss) return;
	if (UCharacterMovementComponent* Movement = InstanceData.Boss->GetCharacterMovement())
	{
		if (Movement->MovementMode == MOVE_Flying)
		{
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

EStateTreeRunStatus FStateTreeTask_EchidnaRetreatLotusPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	InstanceData.CycleIndex = 0;
	InstanceData.CurrentFan = nullptr;
	InstanceData.LotusFlowers.Reset();
	InstanceData.LotusZones.Reset();

	if (!InstanceData.Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaRetreatLotus] EnterState 실패 — Boss 바인딩 없음"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Arena = nullptr;
	for (TActorIterator<AHexArena> It(InstanceData.Boss->GetWorld()); It; ++It)
	{
		InstanceData.Arena = *It;
		break;
	}

	return StartCycle(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FStateTreeTask_EchidnaRetreatLotusPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 광폭화 배율 — 부채꼴 예고(액터 타이머는 CustomTimeDilation으로 나눔)와 슬라이드가 같은 비율로 빨라져야 도착 순간에 터진다
	const float ScaledDelta = DeltaTime * AEchidnaBoss::GetEnrageTimeScale(Boss);

	Boss->SetActorRotation(FRotator(0.f, InstanceData.CycleYaw, 0.f));
	TickLotus(InstanceData);

	switch (InstanceData.Phase)
	{
	case EEchidnaRetreatLotusPhase::Sliding:
	{
		InstanceData.PhaseElapsed += ScaledDelta;
		const float Alpha = FMath::Clamp(InstanceData.PhaseElapsed / FMath::Max(InstanceData.SlideDuration, 0.01f), 0.f, 1.f);

		// 부드럽게 출발·감속 + 가운데서 살짝 떠오름
		FVector Loc = FMath::Lerp(InstanceData.SlideStart, InstanceData.SlideEnd, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f));
		Loc.Z += FMath::Sin(Alpha * PI) * InstanceData.SlideHoverHeight;
		Boss->SetActorLocation(Loc);

		// 연꽃은 슬라이드 시간에 고르게 — i번째는 (i + 0.5) / N 지점에서
		const int32 LotusCount = FMath::Max(InstanceData.LotusPerCycle, 0);
		while (InstanceData.LotusSpawnedThisCycle < LotusCount
			&& Alpha >= (InstanceData.LotusSpawnedThisCycle + 0.5f) / LotusCount)
		{
			SpawnLotusNearBoss(InstanceData);
			InstanceData.LotusSpawnedThisCycle++;
		}

		if (Alpha < 1.f)
		{
			return EStateTreeRunStatus::Running;
		}

		Boss->SetActorLocation(InstanceData.SlideEnd);
		EndSlide(InstanceData);
		InstanceData.Phase = EEchidnaRetreatLotusPhase::Exploding;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaRetreatLotusPhase::Exploding:
	{
		if (IsValid(InstanceData.CurrentFan) && !InstanceData.CurrentFan->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}

		InstanceData.CycleIndex++;
		if (InstanceData.CycleIndex >= InstanceData.RepeatCount)
		{
			InstanceData.Phase = EEchidnaRetreatLotusPhase::Done;
			return EStateTreeRunStatus::Running;
		}

		// 다음 사이클 전 숨 고르기 — 플레이어 쪽으로 몇 걸음(아레나 밖이면 줄임)
		InstanceData.RestStart = Boss->GetActorLocation();
		InstanceData.RestEnd = InstanceData.RestStart;
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(Boss->GetWorld(), 0))
		{
			FVector ToPlayer = PlayerChar->GetActorLocation() - InstanceData.RestStart;
			ToPlayer.Z = 0.f;
			const float Room = ToPlayer.Size() - InstanceData.RestWalkStopDistance;
			if (Room > 0.f && !ToPlayer.IsNearlyZero())
			{
				InstanceData.CycleYaw = ToPlayer.Rotation().Yaw;
				const FVector Dir = ToPlayer.GetSafeNormal();
				float Walk = FMath::Min(InstanceData.RestWalkDistance, Room);
				while (Walk > 0.f && !IsOnArena(InstanceData, InstanceData.RestStart + Dir * Walk))
				{
					Walk -= 50.f;
				}
				InstanceData.RestEnd = InstanceData.RestStart + Dir * FMath::Max(Walk, 0.f);
			}
		}
		InstanceData.PhaseElapsed = 0.f;
		InstanceData.Phase = EEchidnaRetreatLotusPhase::Resting;
		return EStateTreeRunStatus::Running;
	}
	case EEchidnaRetreatLotusPhase::Resting:
	{
		InstanceData.PhaseElapsed += ScaledDelta;
		const float Alpha = InstanceData.CycleRestDuration > 0.f
			? FMath::Clamp(InstanceData.PhaseElapsed / InstanceData.CycleRestDuration, 0.f, 1.f) : 1.f;

		// 일정한 속도로 걷기 — 쉬는 시간 전체에 걸쳐
		FVector Loc = FMath::Lerp(InstanceData.RestStart, InstanceData.RestEnd, Alpha);
		Loc.Z = Boss->GetActorLocation().Z;
		Boss->SetActorLocation(Loc, true);

		if (Alpha < 1.f)
		{
			return EStateTreeRunStatus::Running;
		}
		return StartCycle(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}
	case EEchidnaRetreatLotusPhase::Done:
	default:
		// 마지막 연꽃까지 다 터져야 끝
		return InstanceData.LotusZones.Num() > 0 ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
	}
}

void FStateTreeTask_EchidnaRetreatLotusPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 중간에 끊겨도 보스가 뜬 채로 남거나 연꽃·예고가 남지 않게
	EndSlide(InstanceData);
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	for (const TObjectPtr<AEchidnaBigFlowerActor>& Flower : InstanceData.LotusFlowers)
	{
		if (IsValid(Flower)) Flower->Destroy();
	}
	for (const TObjectPtr<AEchidnaFanZoneActor>& Zone : InstanceData.LotusZones)
	{
		if (IsValid(Zone) && !Zone->HasStartedExploding()) Zone->Destroy();
	}
	if (IsValid(InstanceData.CurrentFan) && !InstanceData.CurrentFan->HasStartedExploding())
	{
		InstanceData.CurrentFan->Destroy();
	}
	InstanceData.LotusFlowers.Reset();
	InstanceData.LotusZones.Reset();
	InstanceData.CurrentFan = nullptr;
	InstanceData.Arena = nullptr;
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaRetreatLotusPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaRetreatLotusPatternDesc", "<b>Echidna Retreat Lotus Pattern</b>");
}
#endif // WITH_EDITOR

// ── 육각 리본 ──────────────────────────────────────────────

void FStateTreeTask_EchidnaHexRibbonPattern::SpawnRibbon(FInstanceDataType& InstanceData, const FVector& Start, const FVector& Direction, float Length,
	float Telegraph, float Damage, bool bKnockdown) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss ? Boss->GetWorld() : nullptr;
	if (!World || Length <= 1.f || Direction.IsNearlyZero()) return;

	UClass* Class = InstanceData.RibbonClass ? InstanceData.RibbonClass.Get() : AEchidnaTetherActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEchidnaTetherActor* Ribbon = World->SpawnActor<AEchidnaTetherActor>(Class, Start, FRotator(0.f, Direction.Rotation().Yaw, 0.f), SpawnParams);
	if (!Ribbon)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaHexRibbon] 리본 스폰 실패 (Class=%s)"), *GetNameSafe(Class));
		return;
	}

	// Activate() 전에 대입 — 메시 크기·판정이 이 값으로 잡힘
	Ribbon->TetherRange = Length;
	Ribbon->TetherHalfWidth = InstanceData.RibbonHalfWidth;
	Ribbon->SnapDelay = Telegraph;
	Ribbon->bApplyPullOnHit = false;
	Ribbon->bApplyStunOnHit = false;
	Ribbon->CharmGaugeAmount = 0;
	Ribbon->HitDamage = Damage;
	Ribbon->bApplyKnockdownOnHit = bKnockdown;
	Ribbon->bApplyStaggerOnHit = !bKnockdown;
	Ribbon->StrikeVFX = InstanceData.StrikeVFX;
	Ribbon->Activate(Boss->GetActorLocation(), 0.f, Boss->GetController());
	InstanceData.Ribbons.Add(Ribbon);
}

void FStateTreeTask_EchidnaHexRibbonPattern::SpawnStep(FInstanceDataType& InstanceData, int32 Step) const
{
	const FRotator Facing(0.f, InstanceData.BaseYaw, 0.f);
	const FVector Forward = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);

	// 1·2번 시작점 — 팔을 앞으로 나란히 했을 때의 오른손
	const FVector RightHand = InstanceData.Center + Forward * InstanceData.HandForwardOffset + Right * InstanceData.HandSideOffset;
	if (Step == 0)
	{
		// 1번 — 오른손에서 정면으로 세로
		SpawnRibbon(InstanceData, RightHand, Forward, InstanceData.Line1Length, InstanceData.LineTelegraph, InstanceData.LineDamage, false);
		return;
	}
	if (Step == 1)
	{
		// 2번 — 오른손에서 왼쪽으로 보스 앞을 가로
		SpawnRibbon(InstanceData, RightHand, -Right, InstanceData.Line2Length, InstanceData.LineTelegraph, InstanceData.LineDamage, false);
		return;
	}

	// 3~8번 — 육각형 변 (꼭짓점 Edge → Edge+1) + 끝 꼭짓점에서 바깥으로
	const int32 Edge = Step - 2;
	auto VertexDir = [&InstanceData](int32 Index)
	{
		return FRotator(0.f, InstanceData.BaseYaw + GetHexVertexYaw(Index), 0.f).Vector();
	};
	const FVector From = InstanceData.Center + VertexDir(Edge) * InstanceData.HexRadius;
	const FVector EndDir = VertexDir(Edge + 1);
	const FVector To = InstanceData.Center + EndDir * InstanceData.HexRadius;

	SpawnRibbon(InstanceData, From, To - From, FVector::Dist2D(From, To), InstanceData.HexTelegraph, InstanceData.HexDamage, true);
	SpawnRibbon(InstanceData, To, EndDir, InstanceData.SpikeLength, InstanceData.HexTelegraph, InstanceData.HexDamage, true);
}

EStateTreeRunStatus FStateTreeTask_EchidnaHexRibbonPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	InstanceData.Ribbons.Reset();
	InstanceData.Elapsed = 0.f;
	InstanceData.NextSpawnTime = 0.f;
	InstanceData.NextStep = 0;

	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaHexRibbon] EnterState 실패 — Boss 바인딩 없음"));
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}

	if (!InstanceData.StrikeVFX)
	{
		InstanceData.StrikeVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/LostArk/Raid/Echidna/Pattern/VFX/NS_EchidnaRetreatFan.NS_EchidnaRetreatFan"));
	}

	// 플레이어를 조준하지 않음 — 지금 보고 있는 방향 그대로
	InstanceData.BaseYaw = Boss->GetActorRotation().Yaw;
	InstanceData.Center = Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Boss->GetCapsuleComponent())
	{
		InstanceData.Center.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EchidnaHexRibbonPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss) return EStateTreeRunStatus::Failed;

	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	Boss->SetActorRotation(FRotator(0.f, InstanceData.BaseYaw, 0.f));

	// 리본 예고(액터 타이머는 CustomTimeDilation으로 나눔)와 같은 비율로 간격도 빨라지게
	InstanceData.Elapsed += DeltaTime * AEchidnaBoss::GetEnrageTimeScale(Boss);

	constexpr int32 TotalSteps = 8;
	while (InstanceData.NextStep < TotalSteps && InstanceData.Elapsed >= InstanceData.NextSpawnTime)
	{
		SpawnStep(InstanceData, InstanceData.NextStep);
		// 1번 → 2번 → 육각형 시작은 LineInterval, 육각형 변끼리는 HexEdgeInterval
		InstanceData.NextSpawnTime += (InstanceData.NextStep <= 1) ? InstanceData.LineInterval : InstanceData.HexEdgeInterval;
		++InstanceData.NextStep;
	}
	if (InstanceData.NextStep < TotalSteps)
	{
		return EStateTreeRunStatus::Running;
	}

	// 마지막 리본까지 판정이 끝나면 종료 (판정 뒤 스스로 사라진 리본은 끝난 것으로 봄)
	for (const TObjectPtr<AEchidnaTetherActor>& Ribbon : InstanceData.Ribbons)
	{
		if (IsValid(Ribbon) && !Ribbon->IsFinished())
		{
			return EStateTreeRunStatus::Running;
		}
	}
	return EStateTreeRunStatus::Succeeded;
}

void FStateTreeTask_EchidnaHexRibbonPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	// 중간에 끊기면 아직 안 터진 리본은 치움
	for (const TObjectPtr<AEchidnaTetherActor>& Ribbon : InstanceData.Ribbons)
	{
		if (IsValid(Ribbon) && !Ribbon->IsFinished())
		{
			Ribbon->Destroy();
		}
	}
	InstanceData.Ribbons.Reset();
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaHexRibbonPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaHexRibbonPatternDesc", "<b>Echidna Hex Ribbon Pattern</b>");
}
#endif // WITH_EDITOR

// ── 장풍 ──────────────────────────────────────────────

EStateTreeRunStatus FStateTreeTask_EchidnaPalmBlastPattern::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	RecordSmallPatternEntered(Context, InstanceData.Boss);
	InstanceData.Elapsed = 0.f;
	InstanceData.bFired = false;

	if (!InstanceData.Boss)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaPalmBlast] EnterState 실패 — Boss 바인딩 없음"));
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.AIController)
	{
		InstanceData.AIController->StopMovement();
	}
	if (!InstanceData.BlastVFX)
	{
		InstanceData.BlastVFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Free_Magic/VFX_Niagara/NS_Gust.NS_Gust"));
	}

	ClearTelegraph(InstanceData);
	UClass* TelegraphClass = InstanceData.TelegraphClass ? InstanceData.TelegraphClass.Get() : AEchidnaGaugeRectActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InstanceData.Boss;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InstanceData.Telegraph = InstanceData.Boss->GetWorld()->SpawnActor<AEchidnaGaugeRectActor>(TelegraphClass, FTransform::Identity, SpawnParams);
	if (InstanceData.Telegraph)
	{
		InstanceData.Telegraph->Init(InstanceData.BlastLength, InstanceData.BlastHalfWidth);
	}
	UpdateTelegraph(InstanceData);
	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaPalmBlastPattern::UpdateTelegraph(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss || !InstanceData.Telegraph) return;

	const FVector Forward = Boss->GetActorForwardVector().GetSafeNormal2D();
	FVector Origin = Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Boss->GetCapsuleComponent())
	{
		Origin.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}
	InstanceData.Telegraph->SetActorLocationAndRotation(Origin + Forward * InstanceData.BlastStartOffset, FRotator(0.f, Forward.Rotation().Yaw, 0.f));
	InstanceData.Telegraph->SetProgress(InstanceData.TrackDuration > 0.f ? InstanceData.Elapsed / InstanceData.TrackDuration : 1.f);
}

void FStateTreeTask_EchidnaPalmBlastPattern::ClearTelegraph(FInstanceDataType& InstanceData) const
{
	if (IsValid(InstanceData.Telegraph))
	{
		InstanceData.Telegraph->Destroy();
	}
	InstanceData.Telegraph = nullptr;
}

void FStateTreeTask_EchidnaPalmBlastPattern::Fire(FInstanceDataType& InstanceData) const
{
	AEchidnaBoss* Boss = InstanceData.Boss;
	UWorld* World = Boss->GetWorld();
	const FVector Forward = Boss->GetActorForwardVector().GetSafeNormal2D();
	FVector Origin = Boss->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Boss->GetCapsuleComponent())
	{
		Origin.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}
	const FVector Start = Origin + Forward * InstanceData.BlastStartOffset;

	if (InstanceData.BlastVFX)
	{
		if (UNiagaraComponent* VFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, InstanceData.BlastVFX, Start,
			FRotator(0.f, Forward.Rotation().Yaw, 0.f), FVector(InstanceData.BlastVFXScale)))
		{
			// NS_Gust 원래 색(청록)의 밝기만 남기고 색조를 바꿈 — 원래 알파 0.5(연기) 등이 반투명해 보여서 알파는 1 이상으로
			struct FGustColor { const TCHAR* Name; float Intensity; float Alpha; };
			static const FGustColor GustColors[] = {
				{ TEXT("Color_Circle"), 10.f, 2.f }, { TEXT("Color_Ray"), 2.f, 1.f }, { TEXT("Color_Smoke"), 1.f, 0.5f },
				{ TEXT("Color_Sparks1"), 100.f, 1.f }, { TEXT("Color_Spiral1"), 1.f, 1.f }, { TEXT("Color_Spiral2"), 1.f, 1.f },
			};
			for (const FGustColor& Entry : GustColors)
			{
				FLinearColor Color = InstanceData.BlastVFXTint * (Entry.Intensity * InstanceData.BlastVFXBrightness);
				Color.A = FMath::Max(Entry.Alpha, 1.f);
				VFX->SetVariableLinearColor(FName(Entry.Name), Color);
			}
		}
	}

	ALoACharacter* Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	if (!Player) return;

	const FVector ToPlayer = Player->GetActorLocation() - Start;
	const float Along = FVector::DotProduct(ToPlayer, Forward);
	const float Side = FMath::Abs(FVector::CrossProduct(Forward, ToPlayer).Z);
	if (Along < 0.f || Along > InstanceData.BlastLength || Side > InstanceData.BlastHalfWidth) return;

	UGameplayStatics::ApplyDamage(Player, InstanceData.BlastDamage, Boss->GetController(), Boss, UDamageType::StaticClass());

	// 맵 밖으로 안 나가게 — 진행 방향으로 아레나 타일이 이어지는 거리를 재서 그 안에 착지하도록 HopScale을 줄인다.
	// 튕겨나가는 거리 ≈ 수평 힘 × HopScale × 정착 시간 × √HopScale (정착 전까지 공중) = Strength·Settle·H^1.5
	float HopScale = InstanceData.KnockdownHopScale;
	AHexArena* Arena = nullptr;
	for (TActorIterator<AHexArena> It(World); It; ++It) { Arena = *It; break; }
	if (Arena)
	{
		const float Step = 25.f;
		const float Margin = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 60.f;
		const float BaseDistance = Player->KnockdownHopStrength * Player->KnockdownHopSettleTime;
		const float MaxCheck = BaseDistance * FMath::Pow(FMath::Max(HopScale, 1.f), 1.5f) + Margin + Step;
		float Reach = 0.f;
		for (float D = Step; D <= MaxCheck; D += Step)
		{
			FIntPoint Coord;
			if (!Arena->WorldToTileCoord(Player->GetActorLocation() + Forward * D, Coord) || !Arena->GetTile(Coord)) break;
			Reach = D;
		}
		const float Allowed = FMath::Max(0.f, Reach - Margin);
		if (BaseDistance > 0.f)
		{
			HopScale = FMath::Min(HopScale, FMath::Pow(Allowed / BaseDistance, 2.f / 3.f));
		}
	}

	// 보스 쪽이 아니라 장풍 진행 방향으로 날아가게 — 정면 축 뒤쪽 점을 원점으로
	Player->ApplyKnockdown(Player->GetActorLocation() - Forward * 300.f, HopScale);
}

EStateTreeRunStatus FStateTreeTask_EchidnaPalmBlastPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEchidnaBoss* Boss = InstanceData.Boss;
	if (!Boss) return EStateTreeRunStatus::Failed;

	if (UCharacterMovementComponent* Move = Boss->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}

	const float ScaledDelta = DeltaTime * AEchidnaBoss::GetEnrageTimeScale(Boss);
	InstanceData.Elapsed += ScaledDelta;

	if (!InstanceData.bFired)
	{
		// 가까이 따라붙은 뒤엔 TrackTurnRate로만 따라 돎(플레이어가 빨리 돌면 조금씩 뒤처짐), 크게 벌어져 있으면 빨리 따라잡음
		if (const ACharacter* Player = UGameplayStatics::GetPlayerCharacter(Boss->GetWorld(), 0))
		{
			const FVector ToPlayer = Player->GetActorLocation() - Boss->GetActorLocation();
			if (!ToPlayer.IsNearlyZero(1.f))
			{
				const float CurrentYaw = Boss->GetActorRotation().Yaw;
				const float Delta = FMath::FindDeltaAngleDegrees(CurrentYaw, ToPlayer.Rotation().Yaw);
				const float CatchUpAlpha = FMath::SmoothStep(InstanceData.TrackCatchUpStartAngle,
					FMath::Max(InstanceData.TrackCatchUpStartAngle + 1.f, InstanceData.TrackCatchUpFullAngle), FMath::Abs(Delta));
				const float TurnRate = FMath::Max(InstanceData.TrackTurnRate, FMath::Lerp(InstanceData.TrackTurnRate, InstanceData.TrackCatchUpRate, CatchUpAlpha));
				const float MaxStep = TurnRate * ScaledDelta;
				Boss->SetActorRotation(FRotator(0.f, CurrentYaw + FMath::Clamp(Delta, -MaxStep, MaxStep), 0.f));
			}
		}
		UpdateTelegraph(InstanceData);

		if (InstanceData.Elapsed < InstanceData.TrackDuration)
		{
			return EStateTreeRunStatus::Running;
		}
		InstanceData.bFired = true;
		InstanceData.Elapsed = 0.f;
		ClearTelegraph(InstanceData);
		Fire(InstanceData);
	}

	return InstanceData.Elapsed >= InstanceData.RecoverDuration ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FStateTreeTask_EchidnaPalmBlastPattern::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ClearTelegraph(Context.GetInstanceData(*this));
}

#if WITH_EDITOR
FText FStateTreeTask_EchidnaPalmBlastPattern::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return LOCTEXT("EchidnaPalmBlastPatternDesc", "<b>Echidna Palm Blast Pattern</b>");
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
