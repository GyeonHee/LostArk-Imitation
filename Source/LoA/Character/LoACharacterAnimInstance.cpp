#include "Character/LoACharacterAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** 게임 스레드 상태를 평가 직전에 복사해 두고, 워커 스레드의 Evaluate에서 시퀀스를 직접 샘플링·블렌드 */
	struct FLoACharacterAnimInstanceProxy : public FAnimInstanceProxy
	{
		FLoACharacterAnimInstanceProxy() = default;
		explicit FLoACharacterAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

		virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override
		{
			FAnimInstanceProxy::PreEvaluateAnimation(InAnimInstance);
			const ULoACharacterAnimInstance* Inst = CastChecked<ULoACharacterAnimInstance>(InAnimInstance);
			RootLockBone = Inst->RootLockBone;
			Idle = Inst->GetIdle();
			Run = Inst->GetRun();
			IdleTime = Inst->GetIdleTime();
			RunTime = Inst->GetRunTime();
			RunAlpha = Inst->GetRunAlpha();
			Cur = Inst->GetCurLayer();
			Prev = Inst->GetPrevLayer();
		}

		virtual bool Evaluate(FPoseContext& Output) override
		{
			// 기본 = 대기 ↔ 달리기
			if (Idle)
			{
				Sample(Idle, IdleTime, true, Output);
				if (Run && RunAlpha > KINDA_SMALL_NUMBER)
				{
					BlendIn(Output, Run, RunTime, true, RunAlpha);
				}
			}
			else
			{
				Output.ResetToRefPose();
			}

			// 액션 — 페이드아웃 중인 이전 액션 위에 현재 액션
			if (Prev.Sequence && Prev.Weight > KINDA_SMALL_NUMBER)
			{
				BlendIn(Output, Prev.Sequence, Prev.Time, Prev.bLoop, Prev.Weight);
			}
			if (Cur.Sequence && Cur.Weight > KINDA_SMALL_NUMBER)
			{
				BlendIn(Output, Cur.Sequence, Cur.Time, Cur.bLoop, Cur.Weight);
			}
			LockRootBone(Output);
			return true;
		}

	private:
		void LockRootBone(FPoseContext& Output)
		{
			if (RootLockBone.IsNone()) return;
			const USkeleton* PoseSkeleton = GetSkeleton();
			const int32 SkeletonIndex = PoseSkeleton ? PoseSkeleton->GetReferenceSkeleton().FindBoneIndex(RootLockBone) : INDEX_NONE;
			if (SkeletonIndex == INDEX_NONE) return;
			const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
			const FCompactPoseBoneIndex Index = Bones.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex);
			if (Index.IsValid())
			{
				// 수평만 기준 포즈로 — 높이(점프)는 애니메이션 그대로
				const FVector Ref = Bones.GetRefPoseTransform(Index).GetTranslation();
				const FVector Now = Output.Pose[Index].GetTranslation();
				Output.Pose[Index].SetTranslation(FVector(Ref.X, Ref.Y, Now.Z));
			}
		}

		static void Sample(const UAnimSequence* Sequence, double Time, bool bLoop, FPoseContext& Out)
		{
			FAnimationPoseData PoseData(Out);
			const FAnimExtractContext Context(Time, false, FDeltaTimeRecord(), bLoop);
			Sequence->GetAnimationPose(PoseData, Context);
		}

		/** Output = Output × (1 - Weight) + Sequence × Weight */
		static void BlendIn(FPoseContext& Output, const UAnimSequence* Sequence, double Time, bool bLoop, float Weight)
		{
			if (Weight >= 1.f - KINDA_SMALL_NUMBER)
			{
				Sample(Sequence, Time, bLoop, Output);
				return;
			}
			FPoseContext Other(Output);
			Sample(Sequence, Time, bLoop, Other);
			FAnimationPoseData OutData(Output);
			const FAnimationPoseData OtherData(Other);
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, OtherData, 1.f - Weight);
		}

		FName RootLockBone;
		const UAnimSequence* Idle = nullptr;
		const UAnimSequence* Run = nullptr;
		double IdleTime = 0.0;
		double RunTime = 0.0;
		float RunAlpha = 0.f;
		ULoACharacterAnimInstance::FLayer Cur;
		ULoACharacterAnimInstance::FLayer Prev;
	};
}

FVector ULoACharacterAnimInstance::GetRootBoneLocation(const UAnimSequence* Sequence, double Time) const
{
	const USkeleton* Skeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	if (!Skeleton) return FVector::ZeroVector;
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(RootLockBone);
	if (BoneIndex == INDEX_NONE) return FVector::ZeroVector;

	FTransform Local;
	Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(BoneIndex), FAnimExtractContext(Time), false);
	const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
	return ParentIndex != INDEX_NONE ? RefSkeleton.GetRefBonePose()[ParentIndex].TransformPosition(Local.GetTranslation()) : Local.GetTranslation();
}

FVector ULoACharacterAnimInstance::ConsumeRootMotion()
{
	const FVector Result = PendingRootMotion;
	PendingRootMotion = FVector::ZeroVector;
	return Result;
}

FAnimInstanceProxy* ULoACharacterAnimInstance::CreateAnimInstanceProxy()
{
	return new FLoACharacterAnimInstanceProxy(this);
}

void ULoACharacterAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FLoACharacterAnimInstanceProxy*>(InProxy);
}

void ULoACharacterAnimInstance::SetLocomotion(UAnimSequence* InIdle, UAnimSequence* InRun, float InFullRunSpeed)
{
	Idle = InIdle;
	Run = InRun;
	FullRunSpeed = FMath::Max(1.f, InFullRunSpeed);
}

void ULoACharacterAnimInstance::PlayAction(UAnimSequence* Sequence, bool bLoop, float BlendTime, bool bInStopOnMove)
{
	FLoAAnimStep Step;
	Step.Sequence = Sequence;
	Step.bLoop = bLoop;
	PlayActionChain({ Step }, BlendTime, bInStopOnMove);
}

void ULoACharacterAnimInstance::PlayActionChain(const TArray<FLoAAnimStep>& Steps, float BlendTime, bool bInStopOnMove)
{
	if (Steps.Num() == 0 || !Steps[0].Sequence) return;

	// 지금 액션(가중치 그대로)을 이전 레이어로 넘겨 페이드아웃 — 새 액션은 0에서 페이드인
	if (Cur.Sequence && Cur.Weight > Prev.Weight)
	{
		Prev = Cur;
	}
	const float Speed = 1.f / FMath::Max(0.01f, BlendTime);
	PrevBlendSpeed = Speed;
	CurBlendSpeed = Speed;
	Cur = FLayer();

	HeldSequences.Reset();
	for (const FLoAAnimStep& Step : Steps)
	{
		if (Step.Sequence) HeldSequences.Add(Step.Sequence);
	}
	if (Prev.Sequence) HeldSequences.Add(Prev.Sequence);

	Queue = Steps;
	bStopOnMove = bInStopOnMove;
	ActionChainTime = 0.0;
	StartStep(Queue[0]);
	Queue.RemoveAt(0);
}

void ULoACharacterAnimInstance::StartStep(const FLoAAnimStep& Step)
{
	const float KeepWeight = Cur.Weight;
	Cur.Sequence = Step.Sequence;
	Cur.Time = Step.Sequence ? FMath::Clamp(static_cast<double>(Step.StartTime), 0.0, static_cast<double>(Step.Sequence->GetPlayLength())) : 0.0;
	Cur.bLoop = Step.bLoop;
	Cur.PlayRate = Step.PlayRate;
	Cur.bRootMotion = Step.bRootMotion;
	Cur.Weight = KeepWeight;	// 같은 체인의 다음 단계는 이어서(가중치 유지)
	bCurHold = Step.bHoldLastFrame;
}

void ULoACharacterAnimInstance::StopAction(float BlendTime, const UAnimSequence* Only)
{
	if (!Cur.Sequence) return;
	if (Only && Cur.Sequence != Only) return;

	Prev = Cur;
	PrevBlendSpeed = 1.f / FMath::Max(0.01f, BlendTime);
	Cur = FLayer();
	Queue.Reset();
	bStopOnMove = false;
}

bool ULoACharacterAnimInstance::IsActionSelfEnding() const
{
	if (!Cur.Sequence) return true;
	if (Cur.bLoop) return false;
	if (Queue.Num() == 0) return !bCurHold;
	const FLoAAnimStep& Last = Queue.Last();
	for (const FLoAAnimStep& Step : Queue)
	{
		if (Step.bLoop) return false;
	}
	return !Last.bHoldLastFrame;
}

void ULoACharacterAnimInstance::AdvanceLayer(FLayer& Layer, float DeltaSeconds) const
{
	if (!Layer.Sequence) return;
	const double Length = Layer.Sequence->GetPlayLength();
	Layer.Time += DeltaSeconds * Layer.PlayRate;
	if (Layer.bLoop && Length > 0.0)
	{
		Layer.Time = FMath::Fmod(Layer.Time, Length);
	}
	else
	{
		Layer.Time = FMath::Min(Layer.Time, Length);
	}
}

void ULoACharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APawn* Pawn = TryGetPawnOwner();
	const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;

	// 기본 레이어 — 대기·달리기 시간은 따로 흐르고, 달리기 비중은 속도로 부드럽게
	if (Idle)
	{
		const double Length = Idle->GetPlayLength();
		IdleTime = Length > 0.0 ? FMath::Fmod(IdleTime + DeltaSeconds, Length) : 0.0;
	}
	if (Run)
	{
		const double Length = Run->GetPlayLength();
		RunTime = Length > 0.0 ? FMath::Fmod(RunTime + DeltaSeconds, Length) : 0.0;
	}
	RunAlpha = FMath::FInterpTo(RunAlpha, FMath::Clamp(Speed / FullRunSpeed, 0.f, 1.f), DeltaSeconds, 12.f);

	// 기상처럼 이동으로 끊어도 되는 액션
	if (bStopOnMove && Cur.Sequence && Speed > 30.f)
	{
		StopAction(0.15f);
	}

	// 액션 레이어
	ActionChainTime += DeltaSeconds;
	if (Cur.Sequence)
	{
		Cur.Weight = FMath::Min(1.f, Cur.Weight + CurBlendSpeed * DeltaSeconds);
		const double PrevTime = Cur.Time;
		// 이번 업데이트에 단계 끝을 넘긴 시간 — 다음 단계로 이어 줘야 체인 시간과 어긋나지 않는다
		const double Overflow = Cur.bLoop ? 0.0 : PrevTime + DeltaSeconds * Cur.PlayRate - Cur.Sequence->GetPlayLength();
		AdvanceLayer(Cur, DeltaSeconds);
		// 반복이 한 바퀴 돌아 시간이 줄었으면 그 프레임은 건너뛴다
		if (Cur.bRootMotion && Cur.Time > PrevTime)
		{
			const FVector Delta = GetRootBoneLocation(Cur.Sequence, Cur.Time) - GetRootBoneLocation(Cur.Sequence, PrevTime);
			PendingRootMotion += FVector(Delta.X, Delta.Y, 0.f) * Cur.Weight;
		}
		if (!Cur.bLoop && Cur.Time >= Cur.Sequence->GetPlayLength())
		{
			if (Queue.Num() > 0)
			{
				StartStep(Queue[0]);
				Queue.RemoveAt(0);
				if (Overflow > 0.0)
				{
					AdvanceLayer(Cur, static_cast<float>(Overflow / FMath::Max(0.01f, Cur.PlayRate)));
				}
			}
			else if (!bCurHold)
			{
				StopAction(0.2f);
			}
		}
	}
	if (Prev.Sequence)
	{
		Prev.Weight = FMath::Max(0.f, Prev.Weight - PrevBlendSpeed * DeltaSeconds);
		AdvanceLayer(Prev, DeltaSeconds);
		if (Prev.Weight <= 0.f)
		{
			Prev = FLayer();
		}
	}
}
