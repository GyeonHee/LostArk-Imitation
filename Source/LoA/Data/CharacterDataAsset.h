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

	// 맨몸 스탯 — 시작 장비 1640 세트(재련 +10: 방어구 지능 합 180,000 / 무기공격력 90,000)를 입으면
	// 지능 200,000 / 무기공격력 100,000 = 장비 도입 전 기준 스펙(공격력 7,190,431)과 같아진다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double BasePrimaryStat = 20000.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double BaseWeaponPower = 10000.0;

	// 최대 HP = MaxHP + 체력 × HPPerVitality. 졸업까지 최대 HP +15%까지만 오르게 할 것(짤패턴 피격 허용 횟수 유지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double BaseVitality = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	float HPPerVitality = 1.f;

	// K — 로아식 공격력(sqrt 값, 시작 스펙 57,735)을 이 프로젝트 스케일(보스 HP 47억)로 올리는 배율.
	// 124.541921 × 57,735 = 공격력 7,190,431. 시작 치명 280(치명률 10.024%, 치명피해 200%)의 기대값 ×1.10024를 곱하면
	// 예전 고정 공격력 7,911,200과 같아 평균 DPS가 그대로다. 전투 특성을 바꾸면 이 값도 다시 역산할 것
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Base")
	double AttackPowerScale = 124.541921;

	// 시작 전투 특성 포인트 (생성자에서 치명 280) — 6단계에서 시작 장신구로 옮긴다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Combat")
	FCombatStatPoints BaseCombatStats;

	UCharacterDataAsset();

	// 특성과 무관한 기본 치명타 적중률(0~1) — 로아처럼 0, 치명률은 치명 특성으로만 얻는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats|Combat")
	float BaseCritRate = 0.f;
};
