#include "Item/RaidReward.h"
#include "Item/ItemTypes.h"
#include "Item/BuildTypes.h"

namespace
{
	/** 보상 표 한 줄 — Chance 확률로 [Min, Max] 개 */
	struct FRaidRewardRow
	{
		FName ItemId;
		int64 Min;
		int64 Max;
		float Chance;
		bool bBase;   // 기본 보상(첫 클리어 배수 적용) / 확률 보상
	};
}

namespace LoARaidReward
{
	const FName Silling(TEXT("Silling"));
	const FName Gold(TEXT("Gold"));

	TArray<FLoARewardItem> RollEchidna(bool bFirstClear)
	{
		// 평균 ±15% — 1클리어 평균: 수호석 130 / 파괴석 40 / 돌파석 46 / 골드 6,100(재련 비용) / 실링 6만(상점 화폐)
		const FRaidRewardRow Rows[] = {
			{ LoAItems::GuardianStone,    110,    150,    1.f,  true },
			{ LoAItems::DestructionStone,  34,     46,    1.f,  true },
			{ LoAItems::LeapStone,         40,     52,    1.f,  true },
			{ LoAItems::FateFragment,    3400,   4600,    1.f,  true },  // 장비 성장 — 평균 4,000
			{ LoAItems::PolishShard,       25,     35,    1.f,  true },  // 장신구 연마 — 로아는 장신구 분해, 분해가 없어 보상으로
			{ Gold,                      5200,   7000,    1.f,  true },
			{ Silling,                  51000,  69000,    1.f,  true },
			{ LoAItems::GlacierBreath,      1,      3,    0.3f, false },  // 방어구 5부위용
			{ LoAItems::LavaBreath,         1,      2,    0.15f, false }, // 무기 1부위용 — 덜 필요해서 덜 나옴
			{ FName(TEXT("Stone_Desire")),  1,      1,    0.3f, false },  // 어빌리티 스톤 (세공 전) — 2026-10-25
			{ FName(TEXT("Bracelet_Desire")), 1,    1,    0.1f, false },  // 팔찌 (효과 무작위)
		};

		TArray<FLoARewardItem> Out;
		// 장신구 1개 (6단계) — 목걸이·귀걸이·반지 중 하나. 첫 클리어 배수·더보기 대상 아님
		Out.Add({ LoAItems::GetRandomDropAccessory(), 1 });

		// 보석 1개 (7단계) — 겁화·작열 반반, Lv1~4 (40/30/20/10%). 첫 클리어 배수·더보기 대상 아님
		{
			const float Roll = FMath::FRand();
			const int32 Level = Roll < 0.4f ? 1 : (Roll < 0.7f ? 2 : (Roll < 0.9f ? 3 : 4));
			Out.Add({ LoAGems::MakeGemId(FMath::RandBool() ? EGemType::Damage : EGemType::Cooldown, Level), 1 });
		}
		// 유물 각인서 1~3권 (50%) — 각인 10종 중 하나
		if (FMath::FRand() < 0.5f)
		{
			const TArray<FEngravingDefinition>& All = LoAEngravings::GetAll();
			Out.Add({ All[FMath::RandRange(0, All.Num() - 1)].BookItemId, FMath::RandRange(1, 3) });
		}
		for (const FRaidRewardRow& Row : Rows)
		{
			if (Row.Chance < 1.f && FMath::FRand() >= Row.Chance) continue;
			int64 Count = Row.Min + static_cast<int64>(FMath::FRand() * static_cast<double>(Row.Max - Row.Min + 1));
			Count = FMath::Clamp(Count, Row.Min, Row.Max);
			if (bFirstClear && Row.bBase) Count *= FirstClearMultiplier;
			Out.Add({ Row.ItemId, Count });
		}
		return Out;
	}

	TArray<FLoARewardItem> MakeBonus(const TArray<FLoARewardItem>& Rewards)
	{
		TArray<FLoARewardItem> Out;
		for (const FLoARewardItem& Item : Rewards)
		{
			if (IsCurrency(Item.ItemId)) continue;
			const FItemDefinition* Def = LoAItems::Find(Item.ItemId);
			if (Def && (Def->IsEquipment() || Def->Category == EItemCategory::Gem || Def->Category == EItemCategory::Book)) continue;  // 장신구·보석·각인서는 더보기로 늘지 않음
			const int64 Extra = static_cast<int64>(FMath::CeilToDouble(Item.Count * BonusRatio));
			if (Extra > 0) Out.Add({ Item.ItemId, Extra });
		}
		return Out;
	}

	void Merge(TArray<FLoARewardItem>& Into, const TArray<FLoARewardItem>& Add)
	{
		for (const FLoARewardItem& Item : Add)
		{
			if (FLoARewardItem* Existing = Into.FindByPredicate([&](const FLoARewardItem& E) { return E.ItemId == Item.ItemId; }))
			{
				Existing->Count += Item.Count;
			}
			else
			{
				Into.Add(Item);
			}
		}
	}
}
