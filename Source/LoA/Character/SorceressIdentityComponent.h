#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SorceressIdentityComponent.generated.h"

class UArkPassiveComponent;

// 마력 방출 상태 — 게이지 30~99%면 마력 강화, 100%면 마력 해방
enum class ESorceressMagickState : uint8
{
	None,
	Amplify,
	Release,
};

// 소서리스 아이덴티티 수치. 로아 값(나무위키 소서리스/스킬)이 있는 건 그대로, 자료에 없는 건 "추정"
namespace LoASorceressIdentity
{
	constexpr float MaxGauge                 = 100.f;
	constexpr float MinRelease               = 30.f;    // 마력 방출 최소 게이지
	// 추정 — 계수 1만큼 맞히면 +1.8%. 시작 세트(Σ 계수/쿨 ≈ 3.1)로 약 18초에 가득 → 해방 8초, 점화 사이클 약 26초
	constexpr float GaugePerCoefficient      = 1.8f;    // 아래 표에 없는 스킬(기본공격)만 — 타격 계수 × 1.8
	constexpr float GaugeGainPerSpecialization = 0.00136f; // 특화 1포인트당 수급 +0.136% — 특화 1800이면 ×3.45(유저 제공 표의 "특화 1800 + 풍요 룬" 실전값 ÷ 기본값 평균)
	// 추정 — 지속시간 = 소모한 게이지 × 0.08초 (해방 8초). 30%에 눌러도 짧게만 켜져서 모아 쓰는 게 낫다
	constexpr float SecondsPerGauge          = 0.08f;
	constexpr float ReleaseCooldown          = 2.f;
	constexpr float AmplifySkillDamage       = 0.10f;   // 속성 스킬 피해 (소서리스 스킬은 전부 속성 — 기본공격 제외)
	constexpr float ReleaseSkillDamage       = 0.20f;   // 강화의 2배
	constexpr float CastTimeReduction        = 1.f;     // 강화·해방 중 캐스팅 스킬 시전 시간 1초 감소(정량 감소 — "1초로 단축"이 아님, 유저 확인)
	constexpr float MinCastTime              = 0.1f;    // 감소 후 하한 — 0초가 되어 바 없이 즉발이 되지 않게
	constexpr float ReleaseCooldownReduction = 0.25f;   // 해방 발동 즉시 스킬(Q~F)의 남은 쿨타임 감소 — 점화를 배우면 점화 값(33/41/50%)으로 대체
	constexpr float BlinkCost                = 30.f;    // PVE 3000 / 10000
	constexpr float BlinkCooldown            = 1.f;
	constexpr float BlinkDistance            = 1500.f;  // 15m
	constexpr float BlinkReleaseCooldownScale = 0.5f;   // 해방 중 점멸 쿨타임 50%
	constexpr float MagickCycleAttackSpeed   = 0.08f;   // 마나 순환 — 해방이 끝나면 공격 속도 (30초)
	constexpr float MagickCycleDuration      = 30.f;
}

/**
 * 소서리스 아이덴티티 — 신비한 마력 게이지 + 마력 방출(Z) + 점멸(X). ASorceress가 만든다.
 *  - 게이지: 보스·허수아비를 맞힐 때 스킬별 수급량(풀히트 기준, SorceressIdentity_SkillGauge 표) × 이 타격의 몫(타격 계수 ÷ 스킬 계수)
 *    × 특화 배율. 표에 없는 기본공격은 계수 × 1.8. 강화·해방 중엔 안 찬다(ULoADamageLibrary::NotifyIdentityHit)
 *  - 마력 방출: 게이지 전부를 써서 강화(30~99%) 또는 해방(100%). 지속시간은 게이지 비례
 *  - 강화: 속성 피해 +10%, 캐스팅 1초 감소 / 해방: 속성 피해 +20%, 캐스팅 1초 감소, 발동 시 남은 쿨 25% 감소(점화면 점화 값),
 *    슈퍼아머(넉다운·경직 무시 — 기절·끌려감·붙잡힘·매혹은 받는다). 화염 부위파괴 +1·번개 무력화 증가는 부위파괴·무력화 시스템이 없어 생략
 *  - 점멸: 게이지 30%로 15m 순간이동(보스는 넘고 벽은 못 넘음). 아크 패시브 환류의 기운·마력 충전·응집되는 마력은 여기서 발동
 *  - 아크 패시브 깨달음: 점화(해방 시 남은 쿨 감소)·점화의 불씨·발화·마나 순환·점멸 폭발·해방 봉인 / 환류·응집되는 마력이면 마력 방출 불가
 * 효과는 UStatComponent 수정치 출처 "Identity"로 건다. 게이지는 저장하지 않는다(레벨마다 0)
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API USorceressIdentityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USorceressIdentityComponent();

	float GetGauge() const { return Gauge; }
	ESorceressMagickState GetState() const { return State; }
	/** 강화·해방 남은 시간 / 전체 시간 (상태가 없으면 0) */
	float GetStateRemaining() const;
	float GetStateDuration() const { return StateDuration; }
	float GetReleaseCooldownRemaining() const;
	float GetBlinkCooldownRemaining() const;
	float GetBlinkCooldownTotal() const { return BlinkCooldownTotal; }

	/** 환류·응집되는 마력을 배웠으면 마력 방출을 쓸 수 없다 */
	bool IsReleaseBlocked() const;
	/** 점멸에 드는 게이지 — 점멸 폭발이면 0, 환류의 기운이면 감소 */
	float GetBlinkCost() const;
	/** 해방 중 슈퍼아머 (넉다운·경직 무시) */
	bool IsSuperArmor() const { return State == ESorceressMagickState::Release; }

	/** 보스·허수아비 피격 — 그 타격의 계수(ULoADamageLibrary::NotifyIdentityHit가 계산) */
	void AddGaugeFromHit(float HitCoefficient, FName SkillRowName);

	/** 마력 방출(Z). 실패하면 이유(조용히 무시할 땐 빈 글자) */
	bool TryRelease(FText& OutFailReason);
	/** TryRelease가 지금 성공할지만 본다(게이지·쿨·환류) — 애니메이션을 먼저 틀고 주먹 쥘 때 실제 방출할 때 */
	bool CanRelease(FText& OutFailReason) const;

	/** 점멸(X) — 이동은 ASorceress::BlinkTo가 한다 */
	bool TryBlink(const FVector& TargetLocation, FText& OutFailReason);

	/** 테스트 — 게이지 설정 (LoAIdentity 명령) */
	void SetGaugeForTest(float NewGauge);

	/** 사망·레벨 이동 — 상태를 끄고 게이지 0 */
	void ResetIdentity();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	float Gauge = 0.f;
	ESorceressMagickState State = ESorceressMagickState::None;
	float StateEndTime = 0.f;
	float StateDuration = 0.f;
	float ReleaseReadyTime = 0.f;
	float BlinkReadyTime = 0.f;
	float BlinkCooldownTotal = LoASorceressIdentity::BlinkCooldown;
	bool bMagickCycleSpeed = false;

	FTimerHandle StateTimer;
	FTimerHandle MagickCycleTimer;

	float Now() const;
	int32 ArkLevel(const TCHAR* NodeId) const;
	float ArkValue(const TCHAR* NodeId, int32 Index) const;  // 표의 % 값 (레벨 반영, 안 배웠으면 0)
	UArkPassiveComponent* GetArkPassive() const;
	void EndState();
	void ApplyModifiers();
	void NotifySkillUsed();
};
