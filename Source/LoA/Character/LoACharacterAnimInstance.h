#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LoACharacterAnimInstance.generated.h"

class UAnimSequence;

/** 액션 레이어에 차례로 이어 재생할 한 단계 — 반복이 아닌 단계가 끝나면 다음 단계로, 다음이 없으면 bHoldLastFrame이면 마지막 프레임 유지, 아니면 액션 종료 */
struct FLoAAnimStep
{
	UAnimSequence* Sequence = nullptr;
	bool bLoop = false;
	bool bHoldLastFrame = false;
	float PlayRate = 1.f;
	// 이 시각(초)부터 재생 — 스킬 효과가 즉시 나가므로 애니메이션의 발사 순간 직전부터 틀 때
	float StartTime = 0.f;
	// 루트 본(b_root)의 수평 이동을 캐릭터 이동으로 옮긴다(익스플로전 뒤로 점프 등) — CC·대시처럼 코드가 이미 미는 동작은 false
	bool bRootMotion = false;
};

/**
 * AnimBP 없이 C++만으로 포즈를 만드는 플레이어용 AnimInstance — 원본 게임에서 가져온 스켈레톤(소서리스 등)에 쓴다.
 * 기본 레이어 = 대기·달리기 시퀀스를 속도로 블렌드, 그 위에 액션 레이어(넉다운·기절·사망·저스트 가드·스킬 등)를
 * 크로스페이드로 덮는다. 프록시(FLoACharacterAnimInstanceProxy)가 Evaluate에서 시퀀스를 직접 샘플링한다
 * (UAnimSingleNodeInstance와 같은 방식 — AnimGraph가 없으므로 몽타주·슬롯은 안 쓴다).
 * 캐릭터는 ALoACharacter::PlayStateAnim/StopStateAnim으로 상태가 바뀔 때만 부른다
 */
UCLASS(Transient, NotBlueprintable)
class LOA_API ULoACharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetLocomotion(UAnimSequence* InIdle, UAnimSequence* InRun, float InFullRunSpeed);

	/** 액션 단계들을 차례로 — 지금 액션은 BlendTime 동안 페이드아웃. bStopOnMove = 이동 속도가 생기면 바로 끊음(기상 등) */
	void PlayActionChain(const TArray<FLoAAnimStep>& Steps, float BlendTime = 0.15f, bool bStopOnMove = false);
	void PlayAction(UAnimSequence* Sequence, bool bLoop, float BlendTime = 0.15f, bool bStopOnMove = false);

	/** 액션을 끊고 기본(대기·달리기)으로. Only가 있으면 지금 재생 중인 시퀀스가 그것일 때만(다른 상태가 덮어쓴 뒤 늦게 오는 해제 무시) */
	void StopAction(float BlendTime = 0.2f, const UAnimSequence* Only = nullptr);

	UAnimSequence* GetCurrentActionSequence() const { return Cur.Sequence; }

	/** 이 본의 수평 이동은 포즈에서 빼고 기준 포즈 위치로 고정한다(높이는 남겨 점프가 보이게) — 원본 애니메이션의 이동은 캐릭터가 따로
	 *  움직인다. 소서리스 스켈레톤 맨 위 "Sorceress"(아마추어) 아래 b_root가 실제 이동 본이라 엔진 루트 모션으로는 못 뽑는다 */
	FName RootLockBone = TEXT("b_root");

	/** bRootMotion 액션이 지난 업데이트 동안 움직인 b_root 수평 이동(메시 컴포넌트 공간, 스케일 전) — 캐릭터 Tick이 꺼내 쓰고 비운다 */
	FVector ConsumeRootMotion();

	// ── 프록시가 PreUpdate에서 복사해 가는 상태 ──
	struct FLayer
	{
		UAnimSequence* Sequence = nullptr;
		double Time = 0.0;
		bool bLoop = false;
		float PlayRate = 1.f;
		float Weight = 0.f;
		bool bRootMotion = false;
	};

	UAnimSequence* GetIdle() const { return Idle; }
	UAnimSequence* GetRun() const { return Run; }
	double GetIdleTime() const { return IdleTime; }
	double GetRunTime() const { return RunTime; }
	float GetRunAlpha() const { return RunAlpha; }
	const FLayer& GetCurLayer() const { return Cur; }
	const FLayer& GetPrevLayer() const { return Prev; }

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	void StartStep(const FLoAAnimStep& Step);
	/** 시퀀스의 b_root 위치(메시 컴포넌트 공간) — 부모 "Sorceress" 기준 포즈로 회전 */
	FVector GetRootBoneLocation(const UAnimSequence* Sequence, double Time) const;

	FVector PendingRootMotion = FVector::ZeroVector;
	void AdvanceLayer(FLayer& Layer, float DeltaSeconds) const;

	// GC 보존 — 레이어 구조체는 원시 포인터라 여기서 붙잡는다
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Run;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> HeldSequences;

	float FullRunSpeed = 400.f;
	double IdleTime = 0.0;
	double RunTime = 0.0;
	float RunAlpha = 0.f;

	FLayer Cur;
	FLayer Prev;
	TArray<FLoAAnimStep> Queue;
	bool bCurHold = false;
	bool bStopOnMove = false;
	float CurBlendSpeed = 0.f;		// 초당 가중치 증가
	float PrevBlendSpeed = 0.f;		// 초당 가중치 감소
};
