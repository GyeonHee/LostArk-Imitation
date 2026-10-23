#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Stat/StatTypes.h"
#include "CharacterDataAsset.generated.h"

/**
 * 캐릭터별 기본 스탯 데이터
 * 에디터에서 DA_Sorceress, DA_Summoner 등으로 생성하여 할당
 */
UCLASS(BlueprintType)
class LOA_API UCharacterDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// 미사용 — 최대 HP는 StatComponent가 (BaseVitality + 장비 체력) × HPPerVitality로 계산한다. DA 없이 쓸 때의 초기값만
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MaxHP = 100000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MaxMP = 5000.f;

	// 초당 MaxMP의 몇 배를 회복할지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MPRegenRate = 0.044f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MoveSpeed = 600.f;

	// [미사용] 예전 고정 공격력 — 이제 공격력은 UStatComponent가 아래 기본 스탯으로 계산한다(AttackPowerScale 참조).
	// 7,911,200 = 예전 35,000(보스 2100만 기준) × 4,746,719,168/21,000,000 — 킬타임(완벽 3분/실전 4분)을 유지하던 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Deprecated")
	float AttackPower = 7911200.f;

	// ── 기본 스탯 (공격력 = AttackPowerScale × sqrt(주 스탯 × 무기공격력 / 6)) ──

	// 직업 계열의 주 스탯 — 소서리스는 마법사라 지능
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	EPrimaryStat PrimaryStatType = EPrimaryStat::Intelligence;

	// 맨몸 스탯 — 로아 70레벨 기본 주 스탯 477(무기 공격력은 장비에서만). 시작 1640 세트(+10)를 입으면
	// 지능 284,958 / 무기 공격력 100,036 → 표시 공격력 68,928
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double BasePrimaryStat = 477.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double BaseWeaponPower = 0.0;

	// 기본 체력 — 캐릭터 + 아직 없는 장비(어빌리티 스톤·팔찌·엘릭서·카르마 등) 몫을 한 덩어리로.
	// 로아 1640 소서리스 최대 생명력 약 19만에 맞춤: (40,895 + 시작 장비 36,465 + 시작 장신구 13,116) × 2.1 = 190,000
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double BaseVitality = 40895.0;

	// 생명 활성력 계수 — 최대 생명력 = 체력 × 이 값. 로아 직업별 hpCon(Maxroll 게임 데이터): 소서리스 2.1, 버서커 2.2, 건슬링어 2.0 …
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	float HPPerVitality = 2.1f;

	// 방어 계수 — 방어력 = 장비 방어력 × 이 값. 로아 직업별 defCoefficient: 소서리스 0.95, 디스트로이어 1.15, 바드 0.9 …
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	float DefenseCoefficient = 0.95f;

	// K — 로아식 공격력(sqrt 값)을 이 프로젝트 스케일(보스 HP 47억)로 올리는 배율.
	// 1~5단계: 124.541921 (시작 치명 280 → 기대값 ×1.10024).
	// 6단계: 시작 장신구 치명 750 + 특화 750 → 치명 기대값 ×1.2685, 특화 ×1.375가 딜의 약 61%(종말·익스·천벌, 기본공격 포함 기준)에 붙어 ×1.228.
	// 124.541921 × 1.10024 / (1.2685 × 1.228) = 87.966 — 1640 시작 세트의 평균 DPS가 그대로다. 시작 특성을 바꾸면 다시 역산할 것
	// 2026-10-22 로아 장비 표 적용: 시작 표시 공격력 57,735 → 68,928이라 87.966 × 57,735 / 68,928 = 73.682 (1640 DPS 그대로)
	// 2026-10-23 장신구 주 스탯(시작 최하 등급 지능 61,427): 68,928 → 75,994라 73.682 × 68,928 / 75,994 = 66.830
	// 2026-10-23 7단계 시작 보석(Lv5 겁화 6·작열 5)·각인(유물 0단계 원한·아드·예둔·저받·타대) 기대 DPS ×2.4187 → 66.830 / 2.4187 = 27.631
	//   (아드레날린은 스킬 사용 빈도로 평균 약 2.8중첩·치명 보너스 없음으로 계산 — 실측은 허수아비 LoADpsTest로)
	// 2026-10-23 허수아비 실측 3,300만(목표 2,670만) + 시작 보석 Lv5 → Lv7(×1.1415) → 27.631 × (26.7 / 33.0) / 1.1415 = 19.585
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double AttackPowerScale = 19.585;

	// 시작 전투 특성 포인트 — 6단계부터 0 (시작 장신구 치명 750 + 특화 750이 대신 준다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Combat")
	FCombatStatPoints BaseCombatStats;

	UCharacterDataAsset();

	// 특성과 무관한 기본 치명타 적중률(0~1) — 로아처럼 0, 치명률은 치명 특성으로만 얻는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Combat")
	float BaseCritRate = 0.f;
};
