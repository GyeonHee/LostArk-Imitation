#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stat/StatTypes.h"
#include "StatComponent.generated.h"

class UCharacterDataAsset;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatsChanged, const FCombatStats& /*NewStats*/);

/**
 * 캐릭터 스탯 — 기본 스탯(DA) + 수정치(장비·장신구·보석·각인·치트)를 합산해 FCombatStats를 캐시한다.
 * 입력이 바뀔 때만 RecalculateStats()로 다시 계산하고 OnStatsChanged를 쏜다(매 틱 계산하지 않음).
 * 수정치는 출처(Source) 이름 단위로 통째로 갈아끼운다 — 예) 장비 창이 "Equip_Weapon" 출처를 매번 새로 등록
 */
UCLASS(ClassGroup=(LoA), meta=(BlueprintSpawnableComponent))
class LOA_API UStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatComponent();

	/** 캐릭터 PostInitializeComponents에서 1회 — DA 기본값을 읽고 첫 계산까지 한다 */
	void InitializeFromData(const UCharacterDataAsset* Data);

	/** 출처 하나의 수정치를 통째로 교체(빈 배열이면 제거) 후 재계산 */
	void SetModifiers(FName Source, const TArray<FStatModifier>& Modifiers);

	void ClearModifiers(FName Source);

	/** 등록된 출처의 수정치 (없으면 빈 배열) */
	TArray<FStatModifier> GetModifiers(FName Source) const;

	UFUNCTION(BlueprintPure, Category="Stats")
	const FCombatStats& GetStats() const { return Stats; }

	/** 액터(또는 컨트롤러면 그 폰)의 스탯 컴포넌트 — 없으면 null */
	static const UStatComponent* Find(const AActor* Actor);

	/** 신속 배율 조회 헬퍼 — 스탯이 없으면 효과 없음(1 / 0) */
	static float GetAttackSpeedMultiplierOf(const AActor* Actor);
	static float GetCooldownReductionOf(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category="Stats")
	EPrimaryStat GetPrimaryStatType() const { return PrimaryStatType; }

	void RecalculateStats();

	/** LoAStats 콘솔용 — 현재 입력과 결과를 사람이 읽는 줄들로 */
	TArray<FString> DescribeStats() const;

	FOnStatsChanged OnStatsChanged;

private:
	// DA에서 복사한 기본값 — 에셋을 직접 들고 있지 않아 레벨 이동·DA 교체와 무관
	EPrimaryStat PrimaryStatType = EPrimaryStat::Intelligence;
	double BasePrimaryStat = 0.0;
	double BaseWeaponPower = 0.0;
	double BaseVitality = 0.0;
	float BaseMaxHP = 0.f;
	float HPPerVitality = 1.f;
	double AttackPowerScale = 1.0;
	float BaseCritRate = 0.f;
	FCombatStatPoints BaseCombatStats;

	TMap<FName, TArray<FStatModifier>> ModifiersBySource;

	UPROPERTY(VisibleAnywhere, Category="Stats")
	FCombatStats Stats;

	bool bInitialized = false;
};
