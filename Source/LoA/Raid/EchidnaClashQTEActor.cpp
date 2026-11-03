#include "Raid/EchidnaClashQTEActor.h"
#include "Raid/EchidnaBoss.h"
#include "UI/ClashQTEWidget.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	const FLinearColor ClashQtePerfectColor(0.35f, 0.75f, 1.f, 1.f);
	const FLinearColor ClashQteGoodColor(0.55f, 1.f, 0.45f, 1.f);
	const FLinearColor ClashQteMissColor(1.f, 0.25f, 0.2f, 1.f);
	const FLinearColor ClashQteGoldColor(1.f, 0.82f, 0.35f, 1.f);
}

AEchidnaClashQTEActor::AEchidnaClashQTEActor()
{
	PrimaryActorTick.bCanEverTick = true;
	CandidateKeys = { EKeys::Q, EKeys::W, EKeys::E, EKeys::R, EKeys::A, EKeys::S, EKeys::D, EKeys::F };
}

void AEchidnaClashQTEActor::Activate(ALoACharacter* InPlayer, AEchidnaBoss* InBoss)
{
	Player = InPlayer;
	Boss = InBoss;
	bActive = true;
	Elapsed = 0.f;
	Stage = -1;
	Misses = 0;

	if (APlayerController* PC = InPlayer ? Cast<APlayerController>(InPlayer->GetController()) : nullptr)
	{
		Widget = CreateWidget<UClashQTEWidget>(PC, UClashQTEWidget::StaticClass());
		if (Widget)
		{
			// 대사(7)·구역 알림보다 위, 패배/클리어(30)보다 아래
			Widget->AddToViewport(25);
			Widget->SetStageText(NSLOCTEXT("ClashQTE", "Start", "격돌!"));
			UE_LOG(LogLoA, Log, TEXT("[ClashQTE] 위젯 생성 완료"));
		}
		else
		{
			UE_LOG(LogLoA, Warning, TEXT("[ClashQTE] 위젯 생성 실패"));
		}
	}
	BeginCloseup();
	UE_LOG(LogLoA, Log, TEXT("[ClashQTE] 시작 — 타이밍 %d단계 + 연타, 판정 ±%.0fms"), StageNoteCounts.Num(), HitWindow * 1000.f);
}

void AEchidnaClashQTEActor::BeginCloseup()
{
	ALoACharacter* Char = Player.Get();
	AEchidnaBoss* B = Boss.Get();
	if (!Char || !B) return;

	// 플레이어 등 뒤에서 보스 쪽을 바라봄 — 붐은 초점에서 카메라 방향 반대로 뻗으므로 Yaw = 플레이어 → 보스
	const FVector ToBoss = B->GetActorLocation() - Char->GetActorLocation();
	const float Dist = ToBoss.Size2D();
	const float CamYaw = ToBoss.GetSafeNormal2D().Rotation().Yaw;
	Char->SetCameraRotationOverride(FRotator(CloseupCameraPitch, CamYaw, 0.f));

	CloseupArm = FMath::Clamp(Dist * CloseupFocusTowardBoss + CloseupArmPadding, CloseupArmMin, CloseupArmMax);
	Char->SetCameraZoomOverride(CloseupArm);

	if (USpringArmComponent* Boom = Char->GetCameraBoom())
	{
		OriginalBoomTargetOffset = Boom->TargetOffset;
	}
	bCloseupActive = true;

	// 플레이어는 결과가 날 때까지 저스트 가드 자세로 버팀
	if (Char->JustGuardAnim)
	{
		Char->PlayStateAnim(Char->JustGuardAnim, true, 0.15f);
		bPlayerGuardAnimPlaying = true;
	}
	UE_LOG(LogLoA, Log, TEXT("[ClashQTE] 클로즈업 — 거리 %.0f, 암 %.0f, Yaw %.0f"), Dist, CloseupArm, CamYaw);
}

void AEchidnaClashQTEActor::TickCloseup(float DeltaTime)
{
	if (!bCloseupActive) return;
	ALoACharacter* Char = Player.Get();
	AEchidnaBoss* B = Boss.Get();
	USpringArmComponent* Boom = Char ? Char->GetCameraBoom() : nullptr;
	if (!Boom || !B) return;

	// TargetOffset은 월드 기준 — 플레이어에서 보스 쪽 초점까지 + 초점 높이
	FVector Desired = (B->GetActorLocation() - Char->GetActorLocation()) * CloseupFocusTowardBoss;
	Desired.Z = CloseupFocusHeight;
	Boom->TargetOffset = FMath::VInterpTo(Boom->TargetOffset, Desired, DeltaTime, CloseupFocusInterpSpeed);
}

void AEchidnaClashQTEActor::EndCloseup()
{
	if (!bCloseupActive) return;
	bCloseupActive = false;
	if (ALoACharacter* Char = Player.Get())
	{
		if (USpringArmComponent* Boom = Char->GetCameraBoom())
		{
			Boom->TargetOffset = OriginalBoomTargetOffset;
		}
		Char->ClearCameraRotationOverride();
		Char->ClearCameraZoomOverride();
	}
}

void AEchidnaClashQTEActor::PlayBossClash(FName Key)
{
	if (AEchidnaBoss* B = Boss.Get())
	{
		B->PlayPatternAnim(Key, 0.15f);
	}
}

void AEchidnaClashQTEActor::StopPlayerGuard()
{
	if (!bPlayerGuardAnimPlaying) return;
	bPlayerGuardAnimPlaying = false;
	if (ALoACharacter* Char = Player.Get())
	{
		Char->StopStateAnim(Char->JustGuardAnim);
	}
}

FKey AEchidnaClashQTEActor::PickRandomKey(const FKey& Exclude) const
{
	TArray<FKey> Pool;
	for (const FKey& Key : CandidateKeys)
	{
		if (Key != Exclude) Pool.Add(Key);
	}
	return Pool.Num() > 0 ? Pool[FMath::RandRange(0, Pool.Num() - 1)] : EKeys::Q;
}

void AEchidnaClashQTEActor::BeginStage(int32 StageIndex)
{
	Stage = StageIndex;
	bWaitingNextStage = false;
	const int32 TotalStages = StageNoteCounts.Num() + 1;

	if (Stage < StageNoteCounts.Num())
	{
		// 타이밍 — 노트가 한 줄로 뜨고 왼쪽부터 차례로 판정
		Notes.Reset();
		TArray<FKey> Keys;
		const int32 Count = FMath::Max(1, StageNoteCounts[Stage]);
		for (int32 i = 0; i < Count; ++i)
		{
			FQteNote Note;
			// 바로 앞 노트와 같은 키는 피해서 "같은 키 두 번"으로 헷갈리지 않게
			Note.Key = PickRandomKey(i > 0 ? Notes[i - 1].Key : EKeys::Invalid);
			Note.HitTime = Elapsed + NoteApproachTime + i * NoteInterval;
			Notes.Add(Note);
			Keys.Add(Note.Key);
		}
		// 보스: 1단계 06→07 / 2단계 02→03 / 3단계 02→03(끝 동작 없이 연타로 이어짐)
		PlayBossClash(FName(*FString::Printf(TEXT("Clash_%d"), FMath::Min(Stage + 1, 3))));
		if (Widget)
		{
			Widget->SetStageText(FText::Format(NSLOCTEXT("ClashQTE", "Stage", "격돌 {0} / {1}"), Stage + 1, TotalStages));
			Widget->ShowNotes(Keys);
		}
	}
	else
	{
		// 연타 — 서로 다른 키 두 개를 번갈아
		MashKeyList[0] = PickRandomKey();
		MashKeyList[1] = PickRandomKey(MashKeyList[0]);
		MashNext = 0;
		MashGauge = MashStartGauge;
		MashStart = Elapsed;
		PlayBossClash(TEXT("Clash_Mash"));
		if (Widget)
		{
			Widget->SetStageText(FText::Format(NSLOCTEXT("ClashQTE", "Mash", "격돌 {0} / {0} — 번갈아 연타!"), TotalStages));
			Widget->ShowMash(MashKeyList[0], MashKeyList[1]);
			Widget->SetMash(MashGauge, MashNext, MashTimeLimit);
		}
	}
}

void AEchidnaClashQTEActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bActive || bFinished) return;
	Elapsed += DeltaTime;
	TickCloseup(DeltaTime);

	if (bResolved)
	{
		if (Elapsed - ResolvedAt >= ResultLingerTime)
		{
			bFinished = true;
			if (Widget) Widget->HideAll();
		}
		return;
	}

	if (Stage < 0)
	{
		if (Elapsed >= StartDelay) BeginStage(0);
		return;
	}
	if (bWaitingNextStage)
	{
		if (Elapsed >= NextStageAt) BeginStage(Stage + 1);
		return;
	}

	if (Stage < StageNoteCounts.Num()) TickTimingStage();
	else TickMashStage(DeltaTime);
}

void AEchidnaClashQTEActor::TickTimingStage()
{
	bool bAllDone = true;
	for (int32 i = 0; i < Notes.Num(); ++i)
	{
		FQteNote& Note = Notes[i];
		if (Note.Result != 0) continue;
		bAllDone = false;

		if (Widget)
		{
			Widget->SetNoteApproach(i, 1.f - (Note.HitTime - Elapsed) / FMath::Max(0.01f, NoteApproachTime));
		}
		// 판정 창을 지나침
		if (Elapsed > Note.HitTime + HitWindow)
		{
			ResolveNote(i, 3);
			if (bResolved) return;
		}
	}

	if (bAllDone)
	{
		bWaitingNextStage = true;
		float Gap = StageGap;
		// 다음이 또 타이밍 단계면 보스가 이번 단계 끝 동작(08·09)을 다 하고 넘어감. 연타 직전(3단계)은 끝 동작 없이 03을 버팀
		if (Stage + 1 < StageNoteCounts.Num())
		{
			const FName EndKey(*FString::Printf(TEXT("Clash_%d_End"), Stage + 1));
			if (AEchidnaBoss* B = Boss.Get(); B && !B->GetPatternAnimSpec(EndKey).IsEmpty())
			{
				B->PlayPatternAnim(EndKey, 0.15f);
				Gap = FMath::Max(Gap, B->GetPatternAnimLength(EndKey));
			}
		}
		NextStageAt = Elapsed + Gap;
	}
}

void AEchidnaClashQTEActor::TickMashStage(float DeltaTime)
{
	MashGauge = FMath::Max(0.f, MashGauge - MashDecayPerSecond * DeltaTime);
	const float TimeLeft = MashTimeLimit - (Elapsed - MashStart);
	if (Widget) Widget->SetMash(MashGauge, MashNext, TimeLeft);

	if (ALoACharacter* Char = Player.Get(); Char && bCloseupActive)
	{
		Char->SetCameraZoomOverride(FMath::Lerp(CloseupArm, CloseupArm * MashCameraArmScaleEnd, MashGauge));
	}
	if (TimeLeft <= 0.f)
	{
		Resolve(false);
	}
}

bool AEchidnaClashQTEActor::HandleKey(const FKey& Key)
{
	if (!IsAcceptingInput()) return false;
	const bool bCandidate = CandidateKeys.Contains(Key);
	// 시작 전·단계 사이엔 후보 키만 먹고(스킬 방지) 판정은 안 함
	if (Stage < 0 || bWaitingNextStage) return bCandidate;

	if (Stage < StageNoteCounts.Num())
	{
		if (!bCandidate) return false;
		// 아직 판정 안 된 가장 앞 노트
		const int32 Index = Notes.IndexOfByPredicate([](const FQteNote& N) { return N.Result == 0; });
		if (Index == INDEX_NONE) return true;
		const FQteNote& Note = Notes[Index];
		const float Delta = Elapsed - Note.HitTime;
		if (Delta < -EarlyIgnore) return true;  // 너무 일러서 없던 입력으로

		const bool bRightKey = Key == Note.Key;
		const float AbsDelta = FMath::Abs(Delta);
		UE_LOG(LogLoA, Log, TEXT("[ClashQTE] %s 입력 — 노트 %s, %+.0fms"), *Key.ToString(), *Note.Key.ToString(), Delta * 1000.f);
		if (bRightKey && AbsDelta <= HitWindow)
		{
			ResolveNote(Index, AbsDelta <= PerfectWindow ? 1 : 2);
		}
		else
		{
			ResolveNote(Index, 3);
		}
		return true;
	}

	// 연타
	if (!bCandidate) return false;
	if (Key == MashKeyList[MashNext])
	{
		MashGauge = FMath::Min(1.f, MashGauge + MashGainPerPress);
		MashNext = 1 - MashNext;
		if (Widget)
		{
			Widget->PulseMash();
			Widget->SetMash(MashGauge, MashNext, MashTimeLimit - (Elapsed - MashStart));
		}
		if (MashGauge >= 1.f)
		{
			Resolve(true);
		}
	}
	else
	{
		MashGauge = FMath::Max(0.f, MashGauge - MashWrongKeyPenalty);
	}
	return true;
}

void AEchidnaClashQTEActor::ResolveNote(int32 Index, int32 Result)
{
	if (!Notes.IsValidIndex(Index) || Notes[Index].Result != 0) return;
	Notes[Index].Result = Result;
	if (Widget) Widget->SetNoteResult(Index, Result);
	ShowJudge(Result);

	if (Result == 3 && ++Misses > MaxMisses)
	{
		Resolve(false);
	}
}

void AEchidnaClashQTEActor::ShowJudge(int32 Result)
{
	const FText Text = Result == 1 ? FText::FromString(TEXT("PERFECT"))
		: Result == 2 ? FText::FromString(TEXT("GOOD"))
		: FText::FromString(TEXT("MISS"));
	const FLinearColor Color = Result == 1 ? ClashQtePerfectColor : Result == 2 ? ClashQteGoodColor : ClashQteMissColor;
	if (Widget) Widget->ShowJudge(Text, Color);
}

void AEchidnaClashQTEActor::Resolve(bool bInSuccess)
{
	if (bResolved) return;
	bResolved = true;
	bSuccess = bInSuccess;
	ResolvedAt = Elapsed;

	if (Widget)
	{
		Widget->ShowJudge(bSuccess ? NSLOCTEXT("ClashQTE", "Win", "격돌 성공!") : NSLOCTEXT("ClashQTE", "Lose", "격돌 실패"),
			bSuccess ? ClashQteGoldColor : ClashQteMissColor);
	}
	if (AEchidnaBoss* B = Boss.Get())
	{
		if (bSuccess)
		{
			B->SpawnLabelText(B->GetActorLocation() + FVector(0.f, 0.f, B->CounterTextHeight), FText::FromString(TEXT("Break!")),
				ClashQteGoldColor, B->CounterTextScale * 1.3f);
			// 격돌에서 밀린 보스가 쓰러짐 — 처치 처리(HandleDefeated)는 이미 쓰러지는 중이면 다시 안 튼다
			B->PlayPatternAnim(TEXT("Defeat"), 0.15f);
		}
		else
		{
			B->StopPatternAnim(0.3f);	// 버티던 동작을 풀고 대기 자세
		}
	}
	StopPlayerGuard();
	UE_LOG(LogLoA, Warning, TEXT("[ClashQTE] %s (단계 %d, 놓침 %d)"), bSuccess ? TEXT("성공") : TEXT("실패"), Stage + 1, Misses);
}

void AEchidnaClashQTEActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPlayerGuard();
	EndCloseup();
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
