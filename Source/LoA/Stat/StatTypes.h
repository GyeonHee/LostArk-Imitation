#pragma once

#include "CoreMinimal.h"
#include "StatTypes.generated.h"

// 로아식 주 스탯 — 직업 계열마다 하나 (전사 힘 / 암살자·헌터 민첩 / 마법사 지능). 소서리스는 지능
UENUM(BlueprintType)
enum class EPrimaryStat : uint8
{
	Strength     UMETA(DisplayName="힘"),
	Dexterity    UMETA(DisplayName="민첩"),
	Intelligence UMETA(DisplayName="지능"),
};

// 전투 특성 6종 — 필드는 6개 다 두지만 효과는 연결할 곳이 있는 것만 켠다.
// 인내(방어력)는 짤패턴 데미지가 절대값이라 "5~8대면 사망" 설계를 깨서, 숙련은 걸 곳(보스 상태이상·무력화 피해 게이지)이 없어서 효과 0
UENUM(BlueprintType)
enum class ELoACombatStat : uint8
{
	Crit           UMETA(DisplayName="치명"),
	Specialization UMETA(DisplayName="특화"),
	Swiftness      UMETA(DisplayName="신속"),
	Domination     UMETA(DisplayName="제압"),
	Endurance      UMETA(DisplayName="인내"),
	Expertise      UMETA(DisplayName="숙련"),
	MAX            UMETA(Hidden),
};

// 전투 특성 포인트 6종 묶음 — DA 시작값, 수정치 합산 결과 둘 다 이걸로 들고 다닌다
USTRUCT(BlueprintType)
struct FCombatStatPoints
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats") int32 Crit = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats") int32 Specialization = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats") int32 Swiftness = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats") int32 Domination = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats") int32 Endurance = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats") int32 Expertise = 0;

	int32 Get(ELoACombatStat Stat) const;
	void Add(ELoACombatStat Stat, int32 Amount);
};

// 수정치가 건드리는 대상 — 장비(3단계)·장신구(6단계)·보석·각인(7단계)·테스트 치트가 전부 이 목록으로 스탯을 더한다
UENUM(BlueprintType)
enum class EStatModifierTarget : uint8
{
	PrimaryStat          UMETA(DisplayName="주 스탯"),        // 캐릭터가 쓰는 주 스탯(소서리스면 지능)에 더함
	WeaponPower          UMETA(DisplayName="무기 공격력"),
	Vitality             UMETA(DisplayName="체력"),
	Crit                 UMETA(DisplayName="치명"),
	Specialization       UMETA(DisplayName="특화"),
	Swiftness            UMETA(DisplayName="신속"),
	Domination           UMETA(DisplayName="제압"),
	Endurance            UMETA(DisplayName="인내"),
	Expertise            UMETA(DisplayName="숙련"),
	CritRate             UMETA(DisplayName="치명타 적중률"),   // 0.1 = +10%p
	CritDamage           UMETA(DisplayName="치명타 피해"),     // 0.4 = +40%p
	AdditiveDamage       UMETA(DisplayName="피해 증가(합)"),   // 서로 더한 뒤 (1 + 합)으로 곱함
	MultiplicativeDamage UMETA(DisplayName="피해 증가(곱)"),   // 0.1 = ×1.1, 각자 따로 곱함(각인 등)
};

USTRUCT(BlueprintType)
struct FStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	EStatModifierTarget Target = EStatModifierTarget::PrimaryStat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	double Value = 0.0;
};

// 입력(기본 스탯·특성·수정치)에서 계산한 결과 — 스탯이 바뀔 때만 UStatComponent가 다시 만든다
USTRUCT(BlueprintType)
struct FCombatStats
{
	GENERATED_BODY()

	// ── 기본 스탯 (수정치까지 합산한 값) ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") double PrimaryStat = 0.0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") double WeaponPower = 0.0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") double Vitality = 0.0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") FCombatStatPoints CombatPoints;

	// sqrt(주 스탯 × 무기공격력 / 6) — K를 곱하기 전 값. UI에는 이걸 보여줘 로아 같은 숫자 감각을 유지한다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") double DisplayAttackPower = 0.0;

	// 실제 데미지 계산에 쓰는 공격력 = K × DisplayAttackPower (정수로 반올림)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") double AttackPower = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float MaxHP = 0.f;

	// ── 전투 특성 효과 ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float CritRate = 0.f;             // 0~1
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float CritDamage = 2.f;           // 2 = 200%
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float SpecializationMultiplier = 1.f; // 캐스팅·차지 스킬에만
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float CooldownReduction = 0.f;    // 0~MaxCooldownReduction
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float MoveSpeedMultiplier = 1.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float AttackSpeedMultiplier = 1.f; // 기본공격·캐스팅 속도
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float DominationMultiplier = 1.f; // 그로기 중인 보스에게만

	// ── 피해 버킷 (7단계 각인·보석용, 지금은 0/1) ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float AdditiveDamageBonus = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats") float MultiplicativeDamage = 1.f;
};

// 전투 특성 1포인트당 효과 — 직업과 무관한 게임 규칙이라 에셋이 아니라 코드에 둔다
// (에셋 값이 C++ 기본값을 덮어써서 "고쳤는데 안 바뀌는" 함정을 피하려는 것도 있음)
namespace LoAStatRules
{
	constexpr float CritRatePerPoint        = 0.000358f; // 치명 1 = 치명타 적중률 +0.0358%
	// 특화 1 = 캐스팅·차지 스킬 피해 +0.05%. 캐스팅 3종(종말·익스·천벌)이 딜의 약 65%라 전체로는 +0.033% — 치명 1(기대값 +0.036%)과 비슷하게
	constexpr float SpecializationPerPoint  = 0.0005f;
	constexpr float CooldownPerSwiftness    = 0.000215f; // 신속 1 = 쿨타임 -0.0215%
	constexpr float SpeedPerSwiftness       = 0.000172f; // 신속 1 = 이동·공격 속도 +0.0172%
	constexpr float DominationPerPoint      = 0.001f;    // 제압 1 = 그로기 보스 피해 +0.1% — 임시값, 2단계 미터로 다시 정함
	constexpr float MaxCooldownReduction    = 0.30f;     // 쿨감 상한 (신속 + 7단계 쿨감 보석 공유)
	constexpr float BaseCritDamage          = 2.0f;      // 기본 치명타 피해 200%
}
