// 에키드나 애니메이션 — 패턴 키 → 애니메이션 표 + 재생 (AEchidnaBoss의 애니메이션 부분만 따로 둔 파일)
#include "EchidnaBoss.h"
#include "Character/LoACharacterAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HexArena.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LoA.h"

namespace
{
	/**
	 * 패턴 키 → 애니메이션 (유저가 PIE에서 하나씩 보고 정한 짝 — Docs/에키드나_애니메이션_매칭표.md).
	 * 표기: 쉼표 = 차례로 이어서, 이름 뒤 * = 반복, ! = 마지막 프레임 유지(다음 키·EndPatternAnim까지), "a|b" = 무작위 하나.
	 * 아무 표시 없이 끝나는 체인은 다 돌면 대기로 돌아간다.
	 * 애니메이션의 b_root 수평 이동은 포즈에서 버리고(높이는 남음), 실제 이동은 패턴이 StartPatternAnimMove로 그 곡선을 따라 한다.
	 * 패턴의 판정 시각은 각 Task가 "단계 시작 + 타격 순간"으로 잡는다 — 타격 순간(주석의 초)은 원본 PSA의 손 속도 정점에서 뽑음.
	 * 체인 순서를 바꾸면 그 패턴 Task의 단계 번호도 같이 고칠 것
	 */
	struct FEchidnaAnimRow { const TCHAR* Key; const TCHAR* Spec; };
	const FEchidnaAnimRow GEchidnaAnimTable[] =
	{
		// ── 기본 ──
		{ TEXT("Groggy"),               TEXT("dmg_critical_start_1,dmg_critical_loop_1*") },
		{ TEXT("Groggy_End"),           TEXT("dmg_critical_end_1") },
		{ TEXT("Defeat"),               TEXT("dead_1,dead_loop_1*") },
		{ TEXT("Rise"),                 TEXT("att_battle_7_08") },
		{ TEXT("Walk"),                 TEXT("walk_normal_1*") },

		// ── 짤패턴 ──
		// 육각 리본: 1타 1_01(1.50초) → 2타 1_03(1.10초) → 회전하며 육각형(1_03 1.9~3.0초). 오른손 버전
		{ TEXT("HexRibbon"),            TEXT("att_battle_1_01,att_battle_1_03") },
		// 왼손 버전 — 2타가 반대쪽에서 나오고 육각형도 반대로 돈다(Task가 좌우를 뒤집음)
		{ TEXT("HexRibbon_Mirror"),     TEXT("att_battle_1_01,att_battle_1_02") },
		// 카운터: 기 모으기 5_01(카운터 창) → 분신 도약 5_02(5m) → 돌진 5_03(9.3m)
		{ TEXT("Counter_Window"),       TEXT("att_battle_5_01!") },
		{ TEXT("Counter_Split"),        TEXT("att_battle_5_02!") },
		{ TEXT("Counter_Dash"),         TEXT("att_battle_5_03") },
		// 백스텝 하트: 0~0.8초 4.7m 뒤로, 하트 4.60초, 부채꼴 폭발 5.80초
		{ TEXT("BackstepHeart"),        TEXT("att_battle_27") },
		// 좌우장판: 12_01 예고 → 12_02 휘두름(0.47초) + 2m 뒤로 → 12_03 휘두름(1.33초) + 1.6m 뒤로
		{ TEXT("RetreatFan"),           TEXT("att_battle_12_01,att_battle_12_02,att_battle_12_03") },
		// 리본: 던지기 3_01(2.03초) / 걸림 → 3_02(앞으로 2.3m, 내려침 2.63초) / 둘 다 빗나감 → 3_03(0.47초) / 빗나감 → 끝으로 도약 5_02
		{ TEXT("Ribbon_Throw"),         TEXT("att_battle_3_01!") },
		{ TEXT("Ribbon_Pull"),          TEXT("att_battle_3_02") },
		{ TEXT("Ribbon_Arc"),           TEXT("att_battle_3_03") },
		{ TEXT("Ribbon_Leap"),          TEXT("att_battle_5_02!") },
		// 하트 난사: 움직임 없는 자세(26_07) 유지
		{ TEXT("HeartBurst"),           TEXT("att_battle_26_07*") },
		// 두번긋고: 2_01 1타(1.53초) → 2_02 2타(1.03초) → 2_03 내려앉기(1.40초) → 2_04 도약(0.70초 이륙, 2.45초 착지)
		{ TEXT("DonutSlash"),           TEXT("att_battle_2_01,att_battle_2_02,att_battle_2_03,att_battle_2_04") },
		// 뒤로 빠지며 연꽃: 0.3~1.0초에 미끄러지며 떠올라 1.95초 착지
		{ TEXT("RetreatLotus"),         TEXT("att_battle_8_02") },
		{ TEXT("RetreatLotus_Walk"),    TEXT("walk_normal_1*") },
		// 끌고간후 장판: 13_01 줄기 → 13_02 끌기(0.30초) → 13_03 1번 장판(1.37초) → 13_04 2번 장판(0.57초)
		{ TEXT("DragFan"),              TEXT("att_battle_13_01,att_battle_13_02,att_battle_13_03,att_battle_13_04") },
		// 구체: 30_01 1번(1.73초) → 30_02 2번(1.13초) → 30_03 4m 백스텝 → 30_04 부채꼴 4개(1.93초)
		{ TEXT("ReturningOrb"),         TEXT("att_battle_30_01,att_battle_30_02,att_battle_30_03,att_battle_30_04") },
		// 장풍: 1.20초 발사
		{ TEXT("PalmBlast"),            TEXT("att_battle_28_04") },
		{ TEXT("FourMirror"),           TEXT("att_battle_25_02,att_battle_25_03*") },
		{ TEXT("FourMirror_End"),       TEXT("att_battle_25_04") },
		{ TEXT("EightMirror"),          TEXT("att_battle_29_01,att_battle_29_02*") },
		{ TEXT("EightMirror_End"),      TEXT("att_battle_29_03") },

		// ── 큰 패턴 ──
		{ TEXT("RandomGrab"),           TEXT("att_battle_26_01,att_battle_26_02*") },
		{ TEXT("RandomGrab_End"),       TEXT("att_battle_26_03") },
		{ TEXT("Swing"),                TEXT("att_battle_26_01,att_battle_26_02*") },
		{ TEXT("Swing_End"),            TEXT("att_battle_26_03") },
		// 똥장판: 32_01 끝(3.77초, 웅크림)에 빔
		{ TEXT("Poop"),                 TEXT("att_battle_32_01,att_battle_32_02*") },
		{ TEXT("Poop_End"),             TEXT("att_battle_32_03") },
		// 거울잇기: 등장 후 자리 찾는 동안 / 성공 후 그로기 자세
		{ TEXT("MirrorLink_Wait"),      TEXT("att_battle_7_08,att_battle_26_07*") },
		{ TEXT("MirrorLink_Success"),   TEXT("att_battle_26_08*") },
		// 거울 카운터 성공 그로기
		{ TEXT("MirrorGroggy"),         TEXT("att_battle_25_05,att_battle_25_06*") },
		{ TEXT("MirrorGroggy_End"),     TEXT("att_battle_25_07") },
		{ TEXT("FinalPhase"),           TEXT("evt2_sden_change_01!") },
	};

	const TCHAR* const EchidnaAnimIdleName = TEXT("idle_battle_1");
	// 이동 = 걷기. run_battle_1은 발을 거의 안 떼는(2.7초에 16cm) 떠다니는 동작이라 걷는 것처럼 안 보였음
	const TCHAR* const EchidnaAnimMoveName = TEXT("walk_normal_1");
	// walk_normal_1은 제자리 걸음 — 1.1초 한 사이클에 발이 약 1m씩 두 번 → 약 180cm/s. 패트롤 속도를 여기에 맞춰야 발이 안 미끄러짐
	constexpr float EchidnaWalkAnimSpeed = 180.f;

	const TCHAR* FindEchidnaAnimTableSpec(FName Key)
	{
		for (const FEchidnaAnimRow& Row : GEchidnaAnimTable)
		{
			if (Key == FName(Row.Key)) return Row.Spec;
		}
		return nullptr;
	}
}

ULoACharacterAnimInstance* AEchidnaBoss::GetBossAnim() const
{
	return GetMesh() ? Cast<ULoACharacterAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
}

UAnimSequence* AEchidnaBoss::FindEchidnaAnim(const FString& Name)
{
	const FName Key(*Name);
	if (const TObjectPtr<UAnimSequence>* Cached = AnimCache.Find(Key))
	{
		return Cached->Get();
	}

	// 확정해서 옮긴 것(깃에 올라감) → 아직 LocalOnly에 있는 원본 순
	static const TCHAR* const Folders[] = {
		TEXT("/Game/LostArk/Raid/EchidnaModling/Echidna/Animations/"),
		TEXT("/Game/LocalOnly/Echidna/Animations/Battle/"),
		TEXT("/Game/LocalOnly/Echidna/Animations/Event/"),
	};
	UAnimSequence* Found = nullptr;
	for (const TCHAR* Folder : Folders)
	{
		const FString Path = FString::Printf(TEXT("%s%s.%s"), Folder, *Name, *Name);
		Found = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Found) break;
	}
	if (!Found)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaAnim] 애니메이션 없음: %s"), *Name);
	}
	AnimCache.Add(Key, Found);
	return Found;
}

bool AEchidnaBoss::BuildSteps(const FString& Spec, TArray<FLoAAnimStep>& OutSteps)
{
	OutSteps.Reset();
	TArray<FString> Tokens;
	Spec.ParseIntoArray(Tokens, TEXT(","), true);
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		FLoAAnimStep Step;
		if (Token.EndsWith(TEXT("*")))
		{
			Step.bLoop = true;
			Token.LeftChopInline(1);
		}
		else if (Token.EndsWith(TEXT("!")))
		{
			Step.bHoldLastFrame = true;
			Token.LeftChopInline(1);
		}

		TArray<FString> Choices;
		Token.ParseIntoArray(Choices, TEXT("|"), true);
		if (Choices.Num() == 0) continue;
		Step.Sequence = FindEchidnaAnim(Choices[FMath::RandRange(0, Choices.Num() - 1)].TrimStartAndEnd());
		if (Step.Sequence)
		{
			OutSteps.Add(Step);
		}
	}
	return OutSteps.Num() > 0;
}

void AEchidnaBoss::SetupAnimation()
{
	USkeletalMeshComponent* BossMesh = GetMesh();
	if (!BossMesh || !BossMesh->GetSkeletalMeshAsset()) return;

	UAnimSequence* Idle = FindEchidnaAnim(EchidnaAnimIdleName);
	if (!Idle)
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaAnim] 대기 애니메이션이 없어 애니메이션을 끔 (LocalOnly 폴더가 없는 PC?)"));
		return;
	}

	BossMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	BossMesh->SetAnimInstanceClass(ULoACharacterAnimInstance::StaticClass());
	// 숨긴 본체를 분신(LeaderPose)이 따라가므로 안 보일 때도 본을 갱신해야 한다(카운터 분신 돌진)
	BossMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	if (ULoACharacterAnimInstance* Anim = GetBossAnim())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxWalkSpeed = EchidnaWalkAnimSpeed;	// BP 값(600)보다 우선 — 걷기 애니메이션 보폭에 맞춤
		}
		Anim->SetLocomotion(Idle, FindEchidnaAnim(EchidnaAnimMoveName), EchidnaWalkAnimSpeed);
	}
}

void AEchidnaBoss::PlayPatternAnim(FName Key, float BlendTime, float PlayRate)
{
	const FString Spec = GetPatternAnimSpec(Key);
	if (Spec.IsEmpty())
	{
		UE_LOG(LogLoA, Warning, TEXT("[EchidnaAnim] 표에 없는 키: %s"), *Key.ToString());
		return;
	}
	UE_LOG(LogLoA, Log, TEXT("[EchidnaAnim] %s → %s"), *Key.ToString(), *Spec);
	PlaySpecAnim(Spec, BlendTime, PlayRate);
}

bool AEchidnaBoss::PlaySpecAnim(const FString& Spec, float BlendTime, float PlayRate)
{
	// 새 키 = 시간 0부터 — 이전 키 기준으로 걸어 둔 이동은 끝낸다
	StopPatternAnimMove();
	PatternAnimStartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bPatternAnimOnInstance = false;

	ULoACharacterAnimInstance* Anim = GetBossAnim();
	TArray<FLoAAnimStep> Steps;
	if (!Anim || !BuildSteps(Spec, Steps)) return false;
	// 배속 — GetPatternAnimTime은 실제 흐른 시간이라 배속을 쓰는 패턴은 타격 시각도 배속으로 나눠 잡을 것
	for (FLoAAnimStep& Step : Steps)
	{
		Step.PlayRate = FMath::Max(0.01f, PlayRate);
	}
	Anim->PlayActionChain(Steps, BlendTime);
	bPatternAnimOnInstance = true;
	return true;
}

float AEchidnaBoss::GetPatternAnimTime() const
{
	if (bPatternAnimOnInstance)
	{
		if (const ULoACharacterAnimInstance* Anim = GetBossAnim())
		{
			return static_cast<float>(Anim->GetActionChainTime());
		}
	}
	const UWorld* World = GetWorld();
	return World ? static_cast<float>((World->GetTimeSeconds() - PatternAnimStartWorldTime) * CustomTimeDilation) : 0.f;
}

UAnimSequence* AEchidnaBoss::GetPatternAnimStep(FName Key, int32 Step)
{
	TArray<FString> Tokens;
	GetPatternAnimSpec(Key).ParseIntoArray(Tokens, TEXT(","), true);
	if (!Tokens.IsValidIndex(Step)) return nullptr;

	FString Token = Tokens[Step].TrimStartAndEnd();
	while (Token.EndsWith(TEXT("*")) || Token.EndsWith(TEXT("!")))
	{
		Token.LeftChopInline(1);
	}
	TArray<FString> Choices;
	Token.ParseIntoArray(Choices, TEXT("|"), true);
	return Choices.Num() > 0 ? FindEchidnaAnim(Choices[0].TrimStartAndEnd()) : nullptr;
}

float AEchidnaBoss::GetPatternAnimStepStart(FName Key, int32 Step)
{
	float Start = 0.f;
	for (int32 i = 0; i < Step; ++i)
	{
		if (const UAnimSequence* Sequence = GetPatternAnimStep(Key, i))
		{
			Start += Sequence->GetPlayLength();
		}
	}
	return Start;
}

float AEchidnaBoss::GetPatternAnimLength(FName Key)
{
	TArray<FString> Tokens;
	GetPatternAnimSpec(Key).ParseIntoArray(Tokens, TEXT(","), true);
	float Length = 0.f;
	for (int32 i = 0; i < Tokens.Num(); ++i)
	{
		if (Tokens[i].TrimStartAndEnd().EndsWith(TEXT("*"))) break;	// 반복부터는 끝이 없음
		if (const UAnimSequence* Sequence = GetPatternAnimStep(Key, i))
		{
			Length += Sequence->GetPlayLength();
		}
	}
	return Length;
}

namespace
{
	bool GetEchidnaRootBoneLocation(const UAnimSequence* Sequence, float Time, FVector& OutLocation)
	{
		const USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
		if (!Skeleton) return false;
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(TEXT("b_root"));
		if (BoneIndex == INDEX_NONE) return false;

		// 메시 컴포넌트 공간으로 — b_root의 로컬 이동은 부모(아마추어 Echidna_ao)의 배율·회전 기준이라 그대로 쓰면
		// 거리가 수 cm로 나와 뒤로 빠지기가 거의 안 움직였다. 부모 사슬을 곱해 올라간다
		const FAnimExtractContext Context(static_cast<double>(Time));
		FTransform Accum = FTransform::Identity;
		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = RefSkeleton.GetParentIndex(Index))
		{
			FTransform Local;
			Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), Context, false);
			Accum = Accum * Local;
		}
		OutLocation = Accum.GetTranslation();
		return true;
	}
}

float AEchidnaBoss::GetAnimRootProgress(const UAnimSequence* Sequence, float Time)
{
	const float Length = Sequence ? Sequence->GetPlayLength() : 0.f;
	if (Length <= 0.f) return 1.f;
	const float ClampedTime = FMath::Clamp(Time, 0.f, Length);

	FVector Start, End, Now;
	if (!GetEchidnaRootBoneLocation(Sequence, 0.f, Start) || !GetEchidnaRootBoneLocation(Sequence, Length, End)
		|| !GetEchidnaRootBoneLocation(Sequence, ClampedTime, Now))
	{
		return ClampedTime / Length;
	}
	// 수평만 — 점프 높이는 진행도에 안 섞는다. 축 방향과 무관하게 "시작에서 얼마나 멀어졌나"
	const float Total = FVector::Dist2D(Start, End);
	if (Total < 1.f) return ClampedTime / Length;
	return FMath::Clamp(static_cast<float>(FVector::Dist2D(Start, Now)) / Total, 0.f, 1.f);
}

float AEchidnaBoss::GetAnimRootDistance(const UAnimSequence* Sequence) const
{
	FVector Start, End;
	if (!Sequence || !GetEchidnaRootBoneLocation(Sequence, 0.f, Start) || !GetEchidnaRootBoneLocation(Sequence, Sequence->GetPlayLength(), End))
	{
		return 0.f;
	}
	const float MeshScale = GetMesh() ? static_cast<float>(GetMesh()->GetComponentScale().X) : 1.f;
	return static_cast<float>(FVector::Dist2D(Start, End)) * MeshScale;
}

void AEchidnaBoss::StartPatternAnimMove(FName Key, int32 Step, const FVector& Dir, float Distance)
{
	UAnimSequence* Sequence = GetPatternAnimStep(Key, Step);
	const FVector FlatDir = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
	if (FlatDir.IsNearlyZero()) return;

	PatternMoveSequence = Sequence;
	PatternMoveStepStart = GetPatternAnimStepStart(Key, Step);
	PatternMoveDistance = Distance >= 0.f ? Distance : GetAnimRootDistance(Sequence);
	PatternMoveStart = GetActorLocation();
	PatternMoveDir = FlatDir;
	bPatternAnimMoving = PatternMoveDistance > 1.f;
	SetActorTickEnabled(bPatternAnimMoving);
	UE_LOG(LogLoA, Log, TEXT("[EchidnaAnim] 이동 %s %d단계(%s) — %.0fcm, %.2f초부터"), *Key.ToString(), Step,
		*GetNameSafe(Sequence), PatternMoveDistance, PatternMoveStepStart);
}

void AEchidnaBoss::StopPatternAnimMove()
{
	bPatternAnimMoving = false;
	PatternMoveSequence = nullptr;
	SetActorTickEnabled(false);
}

void AEchidnaBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPatternAnimMoving)
	{
		TickPatternAnimMove();
	}
}

void AEchidnaBoss::TickPatternAnimMove()
{
	const float LocalTime = GetPatternAnimTime() - PatternMoveStepStart;
	if (LocalTime < 0.f) return;	// 아직 앞 단계

	// 애니메이션이 없으면(LocalOnly 없는 PC) 1초에 걸쳐 같은 거리
	const float Length = PatternMoveSequence ? PatternMoveSequence->GetPlayLength() : 1.f;
	const float Progress = PatternMoveSequence ? GetAnimRootProgress(PatternMoveSequence, LocalTime) : FMath::Clamp(LocalTime, 0.f, 1.f);

	FVector Target = PatternMoveStart + PatternMoveDir * (PatternMoveDistance * Progress);
	Target.Z = GetActorLocation().Z;

	// 발밑에 바닥이 없으면 거기서 멈춤 — 맵 끝에서 뒤로 빠지다 떨어지지 않게.
	// 아레나 타일 그리드 안이면 바닥 있음(라인트레이스만 쓰면 타일 사이 틈 HexGap에 빠져 멈춘다)
	bool bBlocked = false;
	if (UWorld* World = GetWorld())
	{
		bool bOnArena = false;
		for (TActorIterator<AHexArena> It(World); It; ++It)
		{
			FIntPoint Coord;
			bOnArena = It->WorldToTileCoord(Target, Coord) && It->GetTile(Coord);
			break;
		}
		if (!bOnArena)
		{
			const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(EchidnaAnimMoveGround), false, this);
			FHitResult GroundHit;
			bBlocked = !World->LineTraceSingleByChannel(GroundHit, Target, Target - FVector(0.f, 0.f, HalfHeight + 300.f), ECC_Visibility, Params);
		}
	}
	// CMC의 SafeMove — 바닥에 붙어 선 캡슐을 SetActorLocation(sweep)으로 밀면 첫 프레임부터 바닥에 막혀 제자리였다
	// (소서리스 기상 구르기와 같은 함정). 벽·플레이어에 막히면 면을 따라 미끄러지고, 이동은 계속 시도
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!bBlocked && Move)
	{
		FVector Delta = Target - GetActorLocation();
		Delta.Z = 0.f;
		if (!Delta.IsNearlyZero())
		{
			FHitResult Hit;
			Move->SafeMoveUpdatedComponent(Delta, GetActorQuat(), true, Hit);
			if (Hit.IsValidBlockingHit())
			{
				FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
				Slide.Z = 0.f;
				if (!Slide.IsNearlyZero())
				{
					Move->SafeMoveUpdatedComponent(Slide, GetActorQuat(), true, Hit);
				}
			}
		}
	}

	if (bBlocked || LocalTime >= Length || Progress >= 1.f)
	{
		StopPatternAnimMove();
	}
}

bool AEchidnaBoss::IsPatternActionPlaying() const
{
	const ULoACharacterAnimInstance* Anim = GetBossAnim();
	return Anim && Anim->GetCurrentActionSequence() != nullptr;
}

void AEchidnaBoss::EndPatternAnim(float BlendTime)
{
	// 패턴이 중간에 끊기면 걸어 둔 이동도 그 자리에서 멈춤
	StopPatternAnimMove();
	// 그로기 동작은 그로기가 끝날 때 EndGroggy가 정리
	if (bGroggy) return;
	if (ULoACharacterAnimInstance* Anim = GetBossAnim(); Anim && !Anim->IsActionSelfEnding())
	{
		Anim->StopAction(BlendTime);
	}
}

void AEchidnaBoss::StopPatternAnim(float BlendTime)
{
	StopPatternAnimMove();
	if (ULoACharacterAnimInstance* Anim = GetBossAnim())
	{
		Anim->StopAction(BlendTime);
	}
}

void AEchidnaBoss::PlayGroggyAnim(FName Key)
{
	GroggyAnimKey = Key;
	PlayPatternAnim(Key, 0.1f);
}

void AEchidnaBoss::SetPatternAnimOverride(FName Key, const FString& Spec)
{
	if (Spec.IsEmpty() || Spec == TEXT("-"))
	{
		AnimOverrides.Remove(Key);
	}
	else
	{
		AnimOverrides.Add(Key, Spec);
	}
}

FString AEchidnaBoss::GetPatternAnimSpec(FName Key) const
{
	if (const FString* Override = AnimOverrides.Find(Key))
	{
		return *Override;
	}
	const TCHAR* Spec = FindEchidnaAnimTableSpec(Key);
	return Spec ? FString(Spec) : FString();
}

TArray<FName> AEchidnaBoss::GetPatternAnimKeys()
{
	TArray<FName> Keys;
	for (const FEchidnaAnimRow& Row : GEchidnaAnimTable)
	{
		Keys.Add(FName(Row.Key));
	}
	return Keys;
}
