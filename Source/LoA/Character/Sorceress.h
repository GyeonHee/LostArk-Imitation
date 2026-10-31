#pragma once

#include "CoreMinimal.h"
#include "Character/MageCharacter.h"
#include "Sorceress.generated.h"

class USorceressIdentityComponent;
class UAvatarComponent;
class UMeshComponent;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 소서리스 — 아이덴티티 = 신비한 마력 게이지 + 마력 방출(Z) / 점멸(X). 규칙은 USorceressIdentityComponent
 * 스페이스 대시는 짧은 순간이동(DT 대시 행 DashImpulse 거리) — 이동기라 추진력만 발동, 아크 패시브의 "점멸"은 X
 */
UCLASS()
class LOA_API ASorceress : public AMageCharacter
{
	GENERATED_BODY()

public:
	ASorceress();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
	TObjectPtr<USorceressIdentityComponent> Identity;

	// 아바타(의상) — 몸 메시를 숨기고 칸별 파츠를 입힌다. P창 [아바타] 탭
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Avatar")
	TObjectPtr<UAvatarComponent> Avatar;

	/** 점멸 이동 — 커서 쪽으로 최대 15m. 벽(WorldStatic)에서 멈추고 보스는 넘는다. 바닥이 없는 곳이면 앞으로 당긴다.
	 *  애니메이션(sk_arcaneshift)이 있으면 대시처럼 그 b_root 곡선대로 날아가며 빠른 구간은 안 보임 */
	bool BlinkTo(const FVector& TargetLocation);

	virtual void ExecuteDash_Implementation(const FVector& TargetLocation) override;

	/** 마력 해방 슈퍼아머 — 넉다운·경직 무시 */
	virtual void ApplyKnockdown(const FVector& SourceLocation, float HopScale = 1.f, bool bBlown = false) override;
	virtual void ApplyStagger() override;

	/** 마력 방출 (MageCharacter 공통 진입점) */
	virtual void ActivateIdentity() override;
	/** 마력 방출 — Z 키(컨트롤러)도 이걸 부를 것. 멈춰 서서 애니메이션을 틀고, 주먹을 쥘 때(MagickReleaseDelay) 실제 방출
	 *  (게이지 소모·버프·오라). 그 사이 CC·사망이면 취소(게이지 안 씀). 컴포넌트 TryRelease를 직접 부르면 애니메이션이 빠진다 */
	bool TryActivateIdentity(FText& OutFailReason);
	bool IsIdentityReleasePending() const;
	virtual float GetIdentityGaugePercent() const override;
	virtual float GetCastTimeReduction() const override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 원본 애니메이션 — 대시(스페이스) / 마력 방출(Z) / 점멸(X). 짝은 내부 이름 추정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> DashAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> IdentityReleaseAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> BlinkAnim;

	/** 대시 중 안 보이는 구간 — 진행도(b_root 이동 비율) 이 사이. sk_movingillusion은 0.2~0.3초에 거리의 약 14% → 68%를
	 *  한 번에 가는데, 그 구간만 숨기면 0.1초라 "깜빡"으로 끝나서 앞뒤로 넓혔다(약 0.1~0.47초, "까암~빡") */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float DashHideStartProgress = 0.04f;

	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float DashShowProgress = 0.85f;

	/** 점멸 중 안 보이는 구간 — sk_arcaneshift는 0.17~0.73초에 7m를 가고, 0.47~0.7초(진행도 약 19% → 97%)가 가장 빠르다 */
	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float BlinkHideStartProgress = 0.14f;

	UPROPERTY(EditDefaultsOnly, Category="Dash")
	float BlinkShowProgress = 0.95f;

	/** 마력 방출 중 몸에 붙는 오라 — NS_Potion(Mixed_Magic_VFX_Pack) 복사본을 반복으로 바꿈. 강화 = 파랑, 해방 = 보라.
	 *  애니메이션에서 주먹을 쥘 때 켜지고 강화·해방이 끝날 때(게이지가 다 꺼질 때) 꺼짐 */
	UPROPERTY(EditDefaultsOnly, Category="Identity")
	TObjectPtr<UNiagaraSystem> MagickAmplifyVFX;

	UPROPERTY(EditDefaultsOnly, Category="Identity")
	TObjectPtr<UNiagaraSystem> MagickReleaseVFX;

	/** Z를 누르고 실제 방출(게이지 소모·버프·오라)까지 — sk_arcanereinforce에서 왼손을 주먹 쥐는 순간(손가락 회전이 가장 큰 0.87~0.9초) */
	UPROPERTY(EditDefaultsOnly, Category="Identity")
	float MagickReleaseDelay = 0.88f;

	/** 오라 컴포넌트 배율(퍼지는 범위) — 입자 크기 자체는 에셋에서 원본의 1.5배로 키움 */
	UPROPERTY(EditDefaultsOnly, Category="Identity")
	float MagickVFXScale = 1.3f;

private:
	/** 애니메이션 따라 이동(대시·점멸) — Anim의 b_root 이동량(시작점 대비 거리, 끝 = 1)을 진행도로 써서 Distance만큼 민다.
	 *  bSweep이면 벽에 막히면 멈춤(대시), 아니면 통과하고 끝에서 TeleportTo로 겹침만 풂(점멸 — 목적지는 미리 검사함) */
	void StartAnimMove(UAnimSequence* Anim, const FVector& Dir, float Distance, bool bSweep, float HideStart, float Show);
	void TickDash(float DeltaSeconds);
	float GetDashAnimProgress(float Time) const;
	void EndDash();
	void SetDashHidden(bool bHide);
	bool bDashHidden = false;
	TArray<TWeakObjectPtr<UMeshComponent>> DashHiddenMeshes;

	bool bDashing = false;
	FVector DashStart = FVector::ZeroVector;
	FVector DashDir = FVector::ForwardVector;
	float DashDistance = 0.f;
	float DashElapsed = 0.f;
	float DashDuration = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> MoveAnim;
	bool bMoveSweep = true;
	float MoveHideStart = 0.f;
	float MoveShow = 0.f;

	void UpdateMagickVFX();
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> MagickVFXComp;
	uint8 MagickVFXState = 0;
	FTimerHandle MagickReleaseTimer;
	void FinishIdentityRelease();
};
