#include "Raid/EchidnaFinalPhaseActor.h"
#include "Raid/EchidnaBoss.h"
#include "Raid/EchidnaClashPlatformActor.h"
#include "Raid/EchidnaShuffleMirrorActor.h"
#include "Raid/EchidnaJustGuardZoneActor.h"
#include "Raid/EchidnaClashQTEActor.h"
#include "Raid/HexArena.h"
#include "Skill/SkillManagerComponent.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "LoA.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEchidnaFinalPhaseActor::AEchidnaFinalPhaseActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PlatformClass = AEchidnaClashPlatformActor::StaticClass();
	MirrorClass = AEchidnaShuffleMirrorActor::StaticClass();
	JustGuardZoneClass = AEchidnaJustGuardZoneActor::StaticClass();
	ClashQTEClass = AEchidnaClashQTEActor::StaticClass();
}

void AEchidnaFinalPhaseActor::StartPhase(AEchidnaBoss* InBoss)
{
	Boss = InBoss;
	Player = Cast<ALoACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	Arena = Cast<AHexArena>(UGameplayStatics::GetActorOfClass(this, AHexArena::StaticClass()));

	// 거울을 세울 맵 가운데 — 가운데 타일 윗면. 아레나가 없으면 보스 발밑
	ArenaCenter = InBoss ? InBoss->GetActorLocation() - FVector(0.f, 0.f, InBoss->GetSimpleCollisionHalfHeight()) : GetActorLocation();
	if (AHexArena* A = Arena.Get())
	{
		A->GetTileTopLocation(FIntPoint(0, 0), ArenaCenter);
	}

	UE_LOG(LogLoA, Warning, TEXT("[FinalPhase] 0줄 발악 시작 — Arena:%s Player:%s"),
		Arena.IsValid() ? TEXT("O") : TEXT("X"), Player.IsValid() ? TEXT("O") : TEXT("X"));
	EnterStep(EEchidnaFinalPhaseStep::Cutscene);
}

void AEchidnaFinalPhaseActor::EnterStep(EEchidnaFinalPhaseStep NewStep)
{
	Step = NewStep;
	StepElapsed = 0.f;
	ALoACharacter* Char = Player.Get();
	ALoAPlayerController* PC = Char ? Cast<ALoAPlayerController>(Char->GetController()) : nullptr;

	switch (Step)
	{
	case EEchidnaFinalPhaseStep::Cutscene:
		// TODO(연출 미정): 지금은 붙잡힘 + 무적 + 줌인 + 대사만. 남아 있던 장판에 맞지 않게 무적
		if (Char)
		{
			Char->SetInvulnerable(true);
			Char->SetHeldByPattern(true);
			bPlayerHeld = true;
		}
		SetCameraZoom(CutsceneCameraArmLength);
		if (PC)
		{
			PC->ShowBossDialogue(DialogueSpeaker, DialogueLine, DialoguePortrait.LoadSynchronous());
		}
		ShowStepMessage(TEXT("[발악] 진입 연출 (미정)"));
		break;

	case EEchidnaFinalPhaseStep::MirrorShuffle:
		if (PC) PC->HideBossDialogue();
		ReleasePlayer();
		if (Char) Char->SetInvulnerable(false);
		BeginShuffle();
		break;

	case EEchidnaFinalPhaseStep::Platform:
		ShowStepMessage(TEXT("[발악] 격돌 발판에 올라 G"));
		break;

	case EEchidnaFinalPhaseStep::Clash:
		if (Platform) Platform->SetInteractable(false);
		if (Char)
		{
			// 발판 가운데에서 보스를 마주 보고 붙잡힘 — QTE 동안 이동·스킬 불가
			Char->SetInvulnerable(true);
			Char->SetHeldByPattern(true);
			bPlayerHeld = true;
			if (Platform)
			{
				FVector Loc = Platform->GetActorLocation();
				Loc.Z = Char->GetActorLocation().Z;
				Char->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (AEchidnaBoss* B = Boss.Get())
			{
				const FVector ToBoss = (B->GetActorLocation() - Char->GetActorLocation()).GetSafeNormal2D();
				if (!ToBoss.IsNearlyZero())
				{
					Char->SetActorRotation(ToBoss.Rotation());
					B->SetActorRotation((-ToBoss).Rotation());
				}
			}
		}
		SetCameraZoom(ClashCameraArmLength);
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			UClass* Class = ClashQTEClass ? ClashQTEClass.Get() : AEchidnaClashQTEActor::StaticClass();
			ClashQTE = World->SpawnActor<AEchidnaClashQTEActor>(Class, GetActorTransform(), Params);
			if (ClashQTE) ClashQTE->Activate(Char, Boss.Get());
		}
		break;

	default:
		break;
	}
}

void AEchidnaFinalPhaseActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Step == EEchidnaFinalPhaseStep::None || Step == EEchidnaFinalPhaseStep::Finished) return;

	// 보스가 사라졌거나 플레이어가 죽었으면(일반 사망 흐름이 패배 화면을 띄움) 더 진행하지 않는다
	const ALoACharacter* Char = Player.Get();
	if (!Boss.IsValid() || !Char || Char->IsDead())
	{
		Step = EEchidnaFinalPhaseStep::Finished;
		ReleasePlayer();
		SetAllMirrorsAcceptCounter(false);
		return;
	}

	StepElapsed += DeltaTime;
	switch (Step)
	{
	case EEchidnaFinalPhaseStep::Cutscene:      TickCutscene(); break;
	case EEchidnaFinalPhaseStep::MirrorShuffle: TickShuffle(DeltaTime); break;
	case EEchidnaFinalPhaseStep::Platform:      TickPlatform(); break;
	case EEchidnaFinalPhaseStep::Clash:         TickClash(); break;
	default: break;
	}
}

void AEchidnaFinalPhaseActor::TickCutscene()
{
	if (StepElapsed >= CutsceneDuration)
	{
		EnterStep(EEchidnaFinalPhaseStep::MirrorShuffle);
	}
}

// ── 거울 섞기 ────────────────────────────────────────────────

void AEchidnaFinalPhaseActor::BeginShuffle()
{
	UWorld* World = GetWorld();
	ALoACharacter* Char = Player.Get();
	if (!World) return;

	SetBossHidden(true);
	SetCameraZoom(ShuffleCameraArmLength);
	SetCameraTilt(true);
	if (Char && Char->SkillManager)
	{
		Char->SkillManager->SetCounterSkillCooldownOverride(CounterSkillCooldown);
		bCounterCooldownOverridden = true;
	}

	// 화면 위쪽 = 카메라가 바라보는 수평 방향. 거울 정면은 카메라 쪽(거울 섞기용 비스듬한 카메라 시선의 반대)
	const float CamYaw = Char ? Char->GetDefaultCameraRotation().Yaw : 0.f;
	const FVector ScreenUp = FRotator(0.f, CamYaw, 0.f).Vector();
	const FVector FaceDir = -FRotator(ShuffleCameraPitch, CamYaw, 0.f).Vector();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Class = MirrorClass ? MirrorClass.Get() : AEchidnaShuffleMirrorActor::StaticClass();

	CenterMirror = World->SpawnActor<AEchidnaShuffleMirrorActor>(Class, ArenaCenter, FRotator::ZeroRotator, Params);
	// 가운데 거울은 비워 두고 초상화가 지나갈 때만 보인다
	if (CenterMirror) CenterMirror->Setup(CenterMirrorScale, FaceDir, false, false);

	// 외곽 — 화면 위(12시)부터 시계 방향으로 고르게
	OuterMirrors.Reset();
	const int32 Count = FMath::Max(2, OuterMirrorCount);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Dir = ScreenUp.RotateAngleAxis(360.f * i / Count, FVector::UpVector);
		AEchidnaShuffleMirrorActor* Mirror = World->SpawnActor<AEchidnaShuffleMirrorActor>(Class, ArenaCenter + Dir * OuterRingRadius, FRotator::ZeroRotator, Params);
		if (!Mirror) continue;
		// 초상화 없이 가운데 거울을 마주 본다(수평 방향이라 똑바로 섬)
		Mirror->Setup(OuterMirrorScale, -Dir, true, false);
		Mirror->OnCounterHit.AddUObject(this, &AEchidnaFinalPhaseActor::OnMirrorCountered);
		OuterMirrors.Add(Mirror);
	}

	CounterSuccesses = 0;
	ShowStepMessage(TEXT("[발악] 빨간 거울의 위치를 기억하세요"));
	StartRound();
}

void AEchidnaFinalPhaseActor::SetShuffle(EShuffle NewSub)
{
	Shuffle = NewSub;
	ShuffleElapsed = 0.f;
}

void AEchidnaFinalPhaseActor::StartRound()
{
	SwapsDone = 0;
	LastSwapPair = FIntPoint(INDEX_NONE, INDEX_NONE);
	RedIndex = OuterMirrors.Num() > 0 ? FMath::RandRange(0, OuterMirrors.Num() - 1) : INDEX_NONE;
	for (AEchidnaShuffleMirrorActor* Mirror : OuterMirrors)
	{
		if (!Mirror) continue;
		Mirror->SetRedGlow(false);
		Mirror->SetCounterGlow(false);
	}
	SetAllMirrorsAcceptCounter(false);
	SetShuffle(EShuffle::Appear);  // RevealDelay 동안 기다렸다가 빨간 거울을 켠다
	UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 거울 섞기 라운드 시작 — 빨간 거울 %d번, 성공 %d/%d"), RedIndex, CounterSuccesses, RequiredCounterSuccesses);
}

void AEchidnaFinalPhaseActor::TickShuffle(float DeltaTime)
{
	ShuffleElapsed += DeltaTime;
	AEchidnaShuffleMirrorActor* RedMirror = OuterMirrors.IsValidIndex(RedIndex) ? OuterMirrors[RedIndex].Get() : nullptr;

	switch (Shuffle)
	{
	case EShuffle::Appear:
		if (ShuffleElapsed >= RevealDelay)
		{
			if (RedMirror) RedMirror->SetRedGlow(true);
			SetShuffle(EShuffle::Reveal);
		}
		break;

	case EShuffle::Reveal:
		if (ShuffleElapsed >= RevealDuration)
		{
			if (RedMirror) RedMirror->SetRedGlow(false);
			PassesDone = 0;
			PassNet = 0;
			PassLog.Reset();
			SetShuffle(PortraitPassCount > 0 ? EShuffle::PrePass : EShuffle::PreSwap);
		}
		break;

	case EShuffle::PrePass:
		if (ShuffleElapsed >= PortraitPassDelay) DoPortraitPass();
		break;

	case EShuffle::PortraitPass:
		if (ShuffleElapsed >= PortraitPassDuration)
		{
			if (PassesDone < PortraitPassCount)
			{
				SetShuffle(EShuffle::PassGap);
			}
			else
			{
				// 합계만큼 시계 방향(+)으로 — 외곽 인덱스는 12시부터 시계 방향
				const int32 Num = OuterMirrors.Num();
				const int32 OldRed = RedIndex;
				if (Num > 0 && RedIndex != INDEX_NONE)
				{
					RedIndex = ((RedIndex + PassNet) % Num + Num) % Num;
				}
				UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 초상화 %s = %+d → 빨간 거울 %d번 → %d번"), *PassLog, PassNet, OldRed, RedIndex);
				SetShuffle(EShuffle::PreSwap);
			}
		}
		break;

	case EShuffle::PassGap:
		if (ShuffleElapsed >= PortraitPassGap) DoPortraitPass();
		break;

	case EShuffle::PreSwap:
		if (ShuffleElapsed >= FirstSwapDelay) DoSwap();
		break;

	case EShuffle::Swapping:
		if (ShuffleElapsed >= SwapDipDuration)
		{
			if (SwapsDone < SwapCount)
			{
				SetShuffle(EShuffle::SwapGap);
			}
			else
			{
				SetAllMirrorsAcceptCounter(true);
				SetShuffle(EShuffle::AwaitCounter);
				ShowStepMessage(FString::Printf(TEXT("[발악] 빨간 거울에 카운터! (%.0f초)"), CounterTimeLimit));
			}
		}
		break;

	case EShuffle::SwapGap:
		if (ShuffleElapsed >= SwapGap) DoSwap();
		break;

	case EShuffle::AwaitCounter:
		if (CounterTimeLimit > 0.f && CenterMirror)
		{
			const int32 Seconds = FMath::Max(0, FMath::CeilToInt(CounterTimeLimit - ShuffleElapsed));
			if (Seconds != ShownCountdown)
			{
				ShownCountdown = Seconds;
				CenterMirror->SetCountdown(Seconds);
			}
		}
		if (CounterTimeLimit > 0.f && ShuffleElapsed >= CounterTimeLimit)
		{
			// 시간 초과 = 오답과 같이 — 빨간 거울에서 저스트 가드 장판
			UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 카운터 시간 초과 — 빨간 거울에서 저스트 가드"));
			StartJustGuard(RedIndex);
		}
		break;

	case EShuffle::JustGuard:
		if (!JustGuardZone || JustGuardZone->IsFinished())
		{
			if (JustGuardZone)
			{
				JustGuardZone->Destroy();
				JustGuardZone = nullptr;
			}
			SetShuffle(EShuffle::RoundRest);
		}
		break;

	case EShuffle::RoundRest:
		if (ShuffleElapsed >= RoundRestDuration) StartRound();
		break;

	case EShuffle::Emerge:
		if (ShuffleElapsed >= EmergeDelay) EmergeBoss();
		break;
	}
}

void AEchidnaFinalPhaseActor::DoSwap()
{
	const int32 Num = OuterMirrors.Num();
	if (Num < 2)
	{
		SwapsDone = SwapCount;
		SetShuffle(EShuffle::Swapping);
		return;
	}

	// 서로 다른 두 거울 — 바로 전 스왑과 같은 쌍은 피한다(같은 쌍이면 제자리로 돌아가 섞인 느낌이 없음)
	int32 A = 0, B = 1;
	for (int32 Try = 0; Try < 10; ++Try)
	{
		A = FMath::RandRange(0, Num - 1);
		B = (A + FMath::RandRange(1, Num - 1)) % Num;
		const FIntPoint Pair(FMath::Min(A, B), FMath::Max(A, B));
		if (Pair != LastSwapPair) break;
	}
	LastSwapPair = FIntPoint(FMath::Min(A, B), FMath::Max(A, B));

	if (OuterMirrors[A]) OuterMirrors[A]->PlayDip(SwapDipDuration);
	if (OuterMirrors[B]) OuterMirrors[B]->PlayDip(SwapDipDuration);
	// 거울은 제자리 — 빨간 거울이 둘 중 하나였으면 다른 쪽으로 옮겨 간다
	if (RedIndex == A) RedIndex = B;
	else if (RedIndex == B) RedIndex = A;

	++SwapsDone;
	SetShuffle(EShuffle::Swapping);
	UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 스왑 %d/%d — %d <-> %d, 빨간 거울 → %d번"), SwapsDone, SwapCount, A, B, RedIndex);
}

void AEchidnaFinalPhaseActor::DoPortraitPass()
{
	const bool bLeftToRight = FMath::RandBool();
	PassNet += bLeftToRight ? 1 : -1;
	PassLog += bLeftToRight ? TEXT("+") : TEXT("-");
	++PassesDone;
	if (CenterMirror) CenterMirror->PlayPortraitPass(bLeftToRight, PortraitPassDuration);
	SetShuffle(EShuffle::PortraitPass);
}

void AEchidnaFinalPhaseActor::OnMirrorCountered(AEchidnaShuffleMirrorActor* Mirror, AActor* Attacker)
{
	if (Step != EEchidnaFinalPhaseStep::MirrorShuffle || Shuffle != EShuffle::AwaitCounter) return;
	const int32 Index = OuterMirrors.IndexOfByKey(Mirror);
	if (Index == INDEX_NONE) return;

	SetAllMirrorsAcceptCounter(false);

	if (Index != RedIndex)
	{
		UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 오답 카운터 — %d번 (정답 %d번)"), Index, RedIndex);
		StartJustGuard(Index);
		return;
	}

	++CounterSuccesses;
	Mirror->SetCounterGlow(true);
	if (AEchidnaBoss* B = Boss.Get())
	{
		B->SpawnCounterText(Mirror->GetActorLocation() + FVector(0.f, 0.f, 300.f));
	}
	if (ALoACharacter* Char = Cast<ALoACharacter>(Attacker); Char && Char->SkillManager)
	{
		Char->SkillManager->ResetCounterSkillCooldowns();
	}
	UE_LOG(LogLoA, Warning, TEXT("[FinalPhase] 카운터 성공 %d/%d — %d번 거울"), CounterSuccesses, RequiredCounterSuccesses, Index);

	if (CounterSuccesses >= RequiredCounterSuccesses)
	{
		EmergeMirrorIndex = Index;
		SetShuffle(EShuffle::Emerge);
	}
	else
	{
		ShowStepMessage(FString::Printf(TEXT("[발악] 카운터 성공 %d/%d"), CounterSuccesses, RequiredCounterSuccesses));
		SetShuffle(EShuffle::RoundRest);
	}
}

void AEchidnaFinalPhaseActor::StartJustGuard(int32 MirrorIndex)
{
	SetAllMirrorsAcceptCounter(false);
	AEchidnaShuffleMirrorActor* Mirror = OuterMirrors.IsValidIndex(MirrorIndex) ? OuterMirrors[MirrorIndex].Get() : nullptr;
	UWorld* World = GetWorld();
	if (!Mirror || !World)
	{
		SetShuffle(EShuffle::RoundRest);
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Class = JustGuardZoneClass ? JustGuardZoneClass.Get() : AEchidnaJustGuardZoneActor::StaticClass();
	const FVector Center = Mirror->GetActorLocation();
	JustGuardZone = World->SpawnActor<AEchidnaJustGuardZoneActor>(Class, Center, FRotator::ZeroRotator, Params);
	if (JustGuardZone)
	{
		JustGuardZone->Activate(Player.Get(), Boss.Get(), JustGuardRadius);
	}
	ShowStepMessage(TEXT("[발악] 게이지가 다 차는 순간 거울 쪽을 가리키고 G — 저스트 가드!"));
	SetShuffle(EShuffle::JustGuard);
}

void AEchidnaFinalPhaseActor::EmergeBoss()
{
	AEchidnaShuffleMirrorActor* Mirror = OuterMirrors.IsValidIndex(EmergeMirrorIndex) ? OuterMirrors[EmergeMirrorIndex].Get() : nullptr;
	const FVector MirrorLoc = Mirror ? Mirror->GetActorLocation() : ArenaCenter;
	ClearMirrors(true);

	// 보스가 그 거울 자리에서 맵 가운데를 보고 나온다 → 바로 앞(가운데 쪽)에 격돌 발판
	FVector Forward = (ArenaCenter - MirrorLoc).GetSafeNormal2D();
	if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
	if (AEchidnaBoss* B = Boss.Get())
	{
		B->SetActorLocationAndRotation(MirrorLoc + FVector(0.f, 0.f, B->GetSimpleCollisionHalfHeight()), Forward.Rotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	SetBossHidden(false);

	if (ALoACharacter* Char = Player.Get(); Char && bCounterCooldownOverridden && Char->SkillManager)
	{
		Char->SkillManager->ClearCounterSkillCooldownOverride();
		bCounterCooldownOverridden = false;
	}
	SetCameraZoom(0.f);
	SetCameraTilt(false);
	SpawnPlatform(MirrorLoc + Forward * PlatformFrontDistance);
	UE_LOG(LogLoA, Warning, TEXT("[FinalPhase] 보스 등장 — %d번 거울 자리"), EmergeMirrorIndex);
	EnterStep(EEchidnaFinalPhaseStep::Platform);
}

void AEchidnaFinalPhaseActor::SetBossHidden(bool bInHidden)
{
	AEchidnaBoss* B = Boss.Get();
	if (!B || bBossHidden == bInHidden) return;
	bBossHidden = bInHidden;

	B->SetActorHiddenInGame(bInHidden);
	B->SetActorEnableCollision(!bInHidden);
	// 충돌을 끈 동안 바닥을 뚫고 떨어지지 않게 (카운터 패턴 분신과 같은 이유)
	if (UCharacterMovementComponent* Move = B->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(bInHidden ? MOVE_Flying : MOVE_Walking);
	}
}

void AEchidnaFinalPhaseActor::SetAllMirrorsAcceptCounter(bool bAccept)
{
	// 카운터를 받는 동안 = 외곽 전부 파란 발광 + 가운데 거울 위 카운트다운
	for (AEchidnaShuffleMirrorActor* Mirror : OuterMirrors)
	{
		if (!Mirror) continue;
		Mirror->SetAcceptCounter(bAccept);
		Mirror->SetCounterReady(bAccept);
	}
	ShownCountdown = -1;
	if (CenterMirror)
	{
		CenterMirror->SetCountdown(bAccept && CounterTimeLimit > 0.f ? FMath::CeilToInt(CounterTimeLimit) : -1);
	}
}

void AEchidnaFinalPhaseActor::ClearMirrors(bool bAnimate)
{
	auto ClearOne = [this, bAnimate](AEchidnaShuffleMirrorActor* Mirror)
	{
		if (!IsValid(Mirror)) return;
		Mirror->OnCounterHit.RemoveAll(this);
		if (bAnimate) Mirror->Vanish(MirrorVanishDuration);
		else Mirror->Destroy();
	};
	for (AEchidnaShuffleMirrorActor* Mirror : OuterMirrors) ClearOne(Mirror);
	ClearOne(CenterMirror);
	OuterMirrors.Reset();
	CenterMirror = nullptr;

	if (JustGuardZone)
	{
		JustGuardZone->Destroy();
		JustGuardZone = nullptr;
	}
}

// ── 격돌 ─────────────────────────────────────────────────────

void AEchidnaFinalPhaseActor::TickPlatform()
{
	if (PlatformTimeLimit > 0.f && StepElapsed >= PlatformTimeLimit)
	{
		Finish(false, NSLOCTEXT("FinalPhase", "PlatformTimeout", "격돌에 나서지 못해 공략에 실패하였습니다."));
	}
}

void AEchidnaFinalPhaseActor::TickClash()
{
	// QTE 결과 글자가 다 보인 뒤(IsFinished) 결과를 낸다. 못 만들었으면 막히지 않게 성공 처리
	if (!ClashQTE)
	{
		Finish(true);
		return;
	}
	if (ClashQTE->IsFinished())
	{
		if (ClashQTE->WasSuccess()) Finish(true);
		else Finish(false, NSLOCTEXT("FinalPhase", "ClashFailed", "에키드나와의 격돌에서 패배하여 공략에 실패하였습니다."));
	}
}

void AEchidnaFinalPhaseActor::ForceClashResult(bool bSuccess)
{
	if (Step == EEchidnaFinalPhaseStep::None || Step == EEchidnaFinalPhaseStep::Finished) return;
	UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 격돌 결과 강제 — %s"), bSuccess ? TEXT("성공") : TEXT("실패"));
	if (bSuccess) Finish(true);
	else Finish(false, NSLOCTEXT("FinalPhase", "ClashFailed", "에키드나와의 격돌에서 패배하여 공략에 실패하였습니다."));
}

void AEchidnaFinalPhaseActor::SpawnPlatform(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 높이는 바닥(가운데 타일 윗면) — 발판 좌표가 타일 위면 그 타일 윗면
	FVector SpawnLoc(Location.X, Location.Y, ArenaCenter.Z);
	if (AHexArena* A = Arena.Get())
	{
		FIntPoint Coord;
		FVector TileTop;
		if (A->WorldToTileCoord(Location, Coord) && A->GetTileTopLocation(Coord, TileTop))
		{
			SpawnLoc.Z = TileTop.Z;
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Class = PlatformClass ? PlatformClass.Get() : AEchidnaClashPlatformActor::StaticClass();
	Platform = World->SpawnActor<AEchidnaClashPlatformActor>(Class, SpawnLoc, FRotator::ZeroRotator, Params);
	if (Platform)
	{
		Platform->OnInteracted.AddUObject(this, &AEchidnaFinalPhaseActor::OnPlatformInteracted);
	}
	UE_LOG(LogLoA, Log, TEXT("[FinalPhase] 격돌 발판 스폰 %s"), *SpawnLoc.ToCompactString());
}

void AEchidnaFinalPhaseActor::OnPlatformInteracted(APawn* Pawn)
{
	if (Step != EEchidnaFinalPhaseStep::Platform || Pawn != Player.Get()) return;
	EnterStep(EEchidnaFinalPhaseStep::Clash);
}

void AEchidnaFinalPhaseActor::Finish(bool bSuccess, const FText& FailReason)
{
	if (Step == EEchidnaFinalPhaseStep::Finished) return;
	Step = EEchidnaFinalPhaseStep::Finished;

	ClearMirrors(false);
	if (ClashQTE)
	{
		ClashQTE->Destroy();
		ClashQTE = nullptr;
	}
	SetBossHidden(false);
	if (Platform)
	{
		Platform->Destroy();
		Platform = nullptr;
	}
	SetCameraZoom(0.f);
	SetCameraTilt(false);
	if (ALoACharacter* Char = Player.Get())
	{
		if (bCounterCooldownOverridden && Char->SkillManager)
		{
			Char->SkillManager->ClearCounterSkillCooldownOverride();
			bCounterCooldownOverridden = false;
		}
		if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Char->GetController()))
		{
			PC->HideBossDialogue();
		}
	}
	// 성공이면 클리어 화면이 무적을 걸고, 실패면 패배 화면 동안 붙잡힘·무적 유지 — 어느 쪽이든 여기선 성공일 때만 풀어 준다
	if (bSuccess) ReleasePlayer();

	UE_LOG(LogLoA, Warning, TEXT("[FinalPhase] 격돌 %s"), bSuccess ? TEXT("성공 — 레이드 클리어") : TEXT("실패 — 레이드 실패"));
	if (AEchidnaBoss* B = Boss.Get())
	{
		B->CompleteFinalPhase(bSuccess, FailReason);
	}
	SetLifeSpan(0.5f);
}

void AEchidnaFinalPhaseActor::ReleasePlayer()
{
	if (!bPlayerHeld) return;
	bPlayerHeld = false;
	if (ALoACharacter* Char = Player.Get())
	{
		Char->SetHeldByPattern(false);
	}
}

void AEchidnaFinalPhaseActor::SetCameraZoom(float ArmLength)
{
	ALoACharacter* Char = Player.Get();
	if (!Char) return;
	if (ArmLength > 0.f)
	{
		Char->SetCameraZoomOverride(ArmLength);
		bCameraOverridden = true;
	}
	else if (bCameraOverridden)
	{
		Char->ClearCameraZoomOverride();
		bCameraOverridden = false;
	}
}

void AEchidnaFinalPhaseActor::SetCameraTilt(bool bTilt)
{
	ALoACharacter* Char = Player.Get();
	if (!Char || bCameraTilted == bTilt) return;
	bCameraTilted = bTilt;
	if (bTilt)
	{
		FRotator Rot = Char->GetDefaultCameraRotation();
		Rot.Pitch = ShuffleCameraPitch;
		Char->SetCameraRotationOverride(Rot);
	}
	else
	{
		Char->ClearCameraRotationOverride();
	}
}

void AEchidnaFinalPhaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMirrors(false);
	if (ClashQTE)
	{
		ClashQTE->Destroy();
		ClashQTE = nullptr;
	}
	if (Platform)
	{
		Platform->Destroy();
		Platform = nullptr;
	}
	if (ALoACharacter* Char = Player.Get(); Char && bCounterCooldownOverridden && Char->SkillManager)
	{
		Char->SkillManager->ClearCounterSkillCooldownOverride();
	}
	Super::EndPlay(EndPlayReason);
}

void AEchidnaFinalPhaseActor::ShowStepMessage(const FString& Message, float Duration) const
{
	UE_LOG(LogLoA, Log, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Orange, Message);
	}
}
