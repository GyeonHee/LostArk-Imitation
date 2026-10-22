#pragma once

#include "CoreMinimal.h"
#include "Item/ItemTypes.h"

class UEquipmentComponent;
class UInventoryComponent;

/** 한 번 재련할 때 드는 것 — 무기는 돌·돌파석·골드 1.5배 */
struct FRefineCost
{
	FName StoneId;          // 방어구 = 수호석, 무기 = 파괴석
	int32 StoneCount = 0;
	int32 LeapCount = 0;    // 돌파석
	int64 Gold = 0;
	FName BreathId;         // 방어구 = 빙하의 숨결, 무기 = 용암의 숨결
	int32 BreathCount = 0;  // GetCost = 그 단계 최대 개수, Preview = 이번에 넣는 개수
	float BreathPerUnit = 0.f;  // 숨결 1개당 확률
};

/** 다음 단계 재련 정보 — UI가 미리 보여줄 때도, 실제로 굴릴 때도 이걸 쓴다 */
struct FRefinePreview
{
	bool bValid = false;        // 장비가 있고 최대 단계가 아님
	int32 CurrentLevel = 0;
	int32 TargetLevel = 0;
	float BaseChance = 0.f;     // 확률표 기본 확률
	float FailBonus = 0.f;      // 실패 보정으로 더해진 확률
	float BreathBonus = 0.f;    // 숨결로 더해진 확률 (안 넣으면 0)
	int32 MaxBreath = 0;        // 이 단계에 넣을 수 있는 숨결 최대 개수 (확정 구간이면 0)
	float BreathBonusPerUnit = 0.f;  // 숨결 1개당 확률 — 최대 개수를 다 넣으면 기본 확률만큼
	float Chance = 0.f;         // 이번 시도 성공 확률 (장인의 기운 100%면 1)
	float MaxChance = 0.f;      // 숨결을 최대로 넣었을 때 확률
	bool bGuaranteed = false;   // 장인의 기운 100% → 확정 성공
	float ArtisanEnergy = 0.f;  // 0~1
	FRefineCost Cost;
};

enum class ERefineResult : uint8
{
	Success,
	Fail,
	NotEnoughMaterials,
	MaxLevel,
	NeedGrowth,   // 장비 성장(경험치 100%) 전
	Invalid,
};

/** 장비 성장 1회 비용 — 운명의 파편 + 실링 (로아와 같이 실링). 무기는 1.5배 */
struct FGrowthCost
{
	int32 Fragments = 0;
	int64 Silling = 0;
};

/** 재련 여러 번(장비 재련 10회) 결과 */
struct FRefineManyOutcome
{
	int32 Tries = 0;             // 실제로 굴린 횟수
	bool bSucceeded = false;     // 마지막 시도가 성공
	bool bStoppedByMaterials = false;
	float EnergyGained = 0.f;    // 실패로 쌓인 장인의 기운 합
	int32 NewLevel = 0;
	FText FailReason;
};

struct FRefineOutcome
{
	ERefineResult Result = ERefineResult::Invalid;
	int32 NewLevel = 0;
	float ChanceUsed = 0.f;
	float EnergyGained = 0.f;   // 실패 시 쌓인 장인의 기운
	float EnergyAfter = 0.f;
	bool bWasGuaranteed = false;
	FText FailReason;           // 재료 부족 등
};

/**
 * 재련 규칙 (계획서 4단계) — 직업과 무관한 게임 규칙이라 코드에 둔다(LoAEquipRules와 같은 방침). 확률표가 늘면 DataTable로 옮길 것
 *  - 성공 = 재련 단계 +1 (부위 아이템 레벨 +5, 스탯 +2%). 실패해도 단계·장비는 그대로, 재료만 소모
 *  - 실패 보정: 실패할 때마다 기본 확률의 10%씩 더함, 최대 기본의 2배
 *  - 숨결: 단계 구간마다 최대 개수·1개당 확률이 다름(로아 T4 무기 표 — +11~12 0.5%×20 … +24~25 0.02%×50). 실패 보정 상한과 별개 (실패 보정 상한과 별개). 몇 개 넣을지는 재련 확인 창에서 고른다
 *  - 장인의 기운: 실패할 때마다 "그번 성공 확률 × 46.51%"가 쌓이고, 100%가 되면 다음 시도는 확정 성공. 성공하면 기운·보정 0
 *  - 비용은 골드(실링은 상점 화폐). 시도 즉시 디스크에 저장 — 결과를 보고 게임을 꺼서 되돌리는 꼼수 방지
 */
namespace LoARefine
{
	constexpr float FailBonusPerStep = 0.1f;   // 실패 1회당 기본 확률 × 10%
	constexpr int32 MaxFailBonusSteps = 10;    // → 기본 확률의 2배까지
	constexpr float ArtisanEnergyRate = 0.4651f;
	constexpr float WeaponCostMultiplier = 1.5f;

	/** TargetLevel(1~25) 단계의 기본 확률 */
	LOA_API float GetBaseChance(int32 TargetLevel);

	/** 그 단계 1회 비용 (숨결은 최대 개수 — 몇 개 넣을지는 UI가 정한다) */
	LOA_API FRefineCost GetCost(int32 TargetLevel, bool bWeapon);

	/** 장비 인스턴스의 다음 단계 미리보기 — BreathUse개 넣을 때 (0~최대로 잘림, Cost.BreathCount = 실제로 넣는 개수) */
	LOA_API FRefinePreview Preview(const FEquipmentInstance& Instance, int32 BreathUse);

	/** 재료가 모자라면 false + 무엇이 모자란지 */
	LOA_API bool HasMaterials(const UInventoryComponent& Inventory, const FRefineCost& Cost, FText& OutReason);

	/** 착용 중인 Slot 부위를 1회 재련 — 성장 확인 → 재료 차감 → 굴림 → 단계·기운 갱신 → 스탯 재계산 → 저장 */
	LOA_API FRefineOutcome TryRefine(UEquipmentComponent& Equipment, EEquipSlot Slot, int32 BreathUse);

	/** 장비 재련 10회 — 성공하거나 재료가 떨어질 때까지 최대 MaxTries번 */
	LOA_API FRefineManyOutcome TryRefineMany(UEquipmentComponent& Equipment, EEquipSlot Slot, int32 BreathUse, int32 MaxTries = 10);

	/** 지금 재료로 몇 번 재련할 수 있는지 (10회 버튼 — 2번 이상이면 켬) */
	LOA_API int32 CountAffordableAttempts(const UInventoryComponent& Inventory, const FRefineCost& Cost);

	/** TargetLevel로 가기 전 장비 성장 비용 */
	LOA_API FGrowthCost GetGrowthCost(int32 TargetLevel, bool bWeapon);

	/** 착용 중인 Slot 부위 성장 — 파편·실링 차감 후 경험치 100%, 저장. 실패하면 OutReason */
	LOA_API bool TryGrow(UEquipmentComponent& Equipment, EEquipSlot Slot, FText& OutReason);

	/** LoARefineSim — TargetLevel 단계를 처음(기운 0)부터 성공할 때까지 Runs번 굴려 평균·최악 시도 횟수 (숨결 없이) */
	LOA_API void Simulate(int32 TargetLevel, int32 Runs, double& OutAverageTries, int32& OutWorstTries);
}
