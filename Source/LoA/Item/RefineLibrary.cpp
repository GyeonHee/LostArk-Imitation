#include "Item/RefineLibrary.h"
#include "Item/EquipmentComponent.h"
#include "Item/InventoryComponent.h"

namespace
{
	/** 확률표 한 구간 — 계획서 4단계 표. +1~+10은 시작 세트 아래(LoAGiveItem으로 받은 0단계 장비)용 */
	struct FRefineTier
	{
		int32 MaxTarget;   // 이 단계까지 (MaxTarget 오름차순)
		float BaseChance;
		int32 Stone;
		int32 Leap;
		int64 Gold;
		int32 Breath;           // 숨결 최대 개수
		float BreathPerUnit;    // 숨결 1개당 확률 (로아 T4 일반 재련 무기 표)
		int32 GrowthFragments;  // 장비 성장 — 운명의 파편
		int64 GrowthSilling;    // 장비 성장 — 실링
	};

	const FRefineTier Refine_Tiers[] = {
		// 골드는 계획서의 실링 비용 ÷ 100 — 졸업까지 약 48.8만 골드 ≒ 클리어 80회 × 평균 6,100
		// 성장(단계마다 1번): 파편 졸업 합 약 32만 ≒ 클리어 80회 × 평균 4,000 / 실링 약 520만 ≒ 80회 × 6.5만(레이드 실링 보상과 맞춤)
		// 숨결(최대 개수 × 1개당) = 로아 T4 일반 재련 무기 표: +11 0.5%×20 / +13 0.25%×20 / +15 0.2%×20 / +20 0.06%×25 / +22 0.04%×25 / +25 0.02%×50
		{ 10, 1.00f,  5, 0,   50,  0, 0.f,     200,   5000 },
		{ 12, 0.45f, 10, 1,  200, 20, 0.005f,  1200,  20000 },
		{ 15, 0.30f, 12, 2,  300, 20, 0.0025f, 1800,  30000 },
		{ 18, 0.15f, 15, 3,  450, 20, 0.002f,  2600,  45000 },
		{ 21, 0.10f, 18, 4,  600, 25, 0.0006f, 3600,  60000 },
		{ 23, 0.05f, 22, 6,  800, 25, 0.0004f, 5000,  80000 },
		{ 25, 0.03f, 25, 8, 1000, 50, 0.0002f, 6500, 100000 },
	};

	const FRefineTier* Refine_FindTier(int32 TargetLevel)
	{
		if (TargetLevel < 1 || TargetLevel > LoAEquipRules::MaxRefineLevel) return nullptr;
		for (const FRefineTier& Tier : Refine_Tiers)
		{
			if (TargetLevel <= Tier.MaxTarget) return &Tier;
		}
		return nullptr;
	}

	// 장인의 기운 100% 판정 — 46.51%씩 쌓는 float 합이 0.99999…에서 멈추지 않게 여유
	bool Refine_IsEnergyFull(float Energy) { return Energy >= 1.f - KINDA_SMALL_NUMBER * 10.f; }

	// 실패 보정까지 넣은 확률 (숨결 제외)
	float Refine_ChanceWithFailBonus(float Base, int32 FailSteps)
	{
		return FMath::Min(Base * (1.f + LoARefine::FailBonusPerStep * FMath::Clamp(FailSteps, 0, LoARefine::MaxFailBonusSteps)), Base * 2.f);
	}

	bool Refine_IsWeapon(const FEquipmentInstance& Instance)
	{
		const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
		return Def && Def->EquipSlot == EEquipSlot::Weapon;
	}
}

namespace LoARefine
{
	float GetBaseChance(int32 TargetLevel)
	{
		const FRefineTier* Tier = Refine_FindTier(TargetLevel);
		return Tier ? Tier->BaseChance : 0.f;
	}

	FRefineCost GetCost(int32 TargetLevel, bool bWeapon)
	{
		FRefineCost Cost;
		const FRefineTier* Tier = Refine_FindTier(TargetLevel);
		if (!Tier) return Cost;
		const float Mul = bWeapon ? WeaponCostMultiplier : 1.f;
		Cost.StoneId = bWeapon ? LoAItems::DestructionStone : LoAItems::GuardianStone;
		Cost.StoneCount = FMath::CeilToInt(Tier->Stone * Mul);
		Cost.LeapCount = FMath::CeilToInt(Tier->Leap * Mul);
		Cost.Gold = static_cast<int64>(FMath::CeilToDouble(Tier->Gold * Mul));
		Cost.BreathId = bWeapon ? LoAItems::LavaBreath : LoAItems::GlacierBreath;
		Cost.BreathCount = Tier->Breath;
		Cost.BreathPerUnit = Tier->BreathPerUnit;
		return Cost;
	}

	FRefinePreview Preview(const FEquipmentInstance& Instance, int32 BreathUse)
	{
		FRefinePreview Out;
		Out.CurrentLevel = Instance.RefineLevel;
		Out.TargetLevel = Instance.RefineLevel + 1;
		Out.ArtisanEnergy = FMath::Clamp(Instance.ArtisanEnergy, 0.f, 1.f);
		if (!LoAItems::Find(Instance.ItemId) || Out.TargetLevel > LoAEquipRules::MaxRefineLevel) return Out;

		Out.bValid = true;
		Out.BaseChance = GetBaseChance(Out.TargetLevel);
		Out.Cost = GetCost(Out.TargetLevel, Refine_IsWeapon(Instance));
		const float WithFail = Refine_ChanceWithFailBonus(Out.BaseChance, Instance.FailBonusSteps);
		Out.FailBonus = WithFail - Out.BaseChance;
		Out.bGuaranteed = Refine_IsEnergyFull(Instance.ArtisanEnergy);
		// 확정(100%) 구간·장인의 기운 100%엔 숨결이 의미 없으니 넣지 않는다
		Out.MaxBreath = (Out.BaseChance < 1.f && !Out.bGuaranteed) ? Out.Cost.BreathCount : 0;
		Out.BreathBonusPerUnit = Out.MaxBreath > 0 ? Out.Cost.BreathPerUnit : 0.f;
		const int32 Use = FMath::Clamp(BreathUse, 0, Out.MaxBreath);
		Out.BreathBonus = Out.BreathBonusPerUnit * Use;
		Out.Chance = Out.bGuaranteed ? 1.f : FMath::Min(1.f, WithFail + Out.BreathBonus);
		Out.MaxChance = Out.bGuaranteed ? 1.f : FMath::Min(1.f, WithFail + Out.BreathBonusPerUnit * Out.MaxBreath);
		Out.Cost.BreathCount = Use;
		return Out;
	}

	bool HasMaterials(const UInventoryComponent& Inventory, const FRefineCost& Cost, FText& OutReason)
	{
		auto Missing = [&OutReason](FName ItemId)
		{
			const FItemDefinition* Def = LoAItems::Find(ItemId);
			OutReason = FText::Format(NSLOCTEXT("Refine", "NotEnough", "{0}이(가) 부족합니다."), Def ? Def->DisplayName : FText::FromName(ItemId));
			return false;
		};
		if (Cost.StoneCount > 0 && Inventory.CountItem(Cost.StoneId) < Cost.StoneCount) return Missing(Cost.StoneId);
		if (Cost.LeapCount > 0 && Inventory.CountItem(LoAItems::LeapStone) < Cost.LeapCount) return Missing(LoAItems::LeapStone);
		if (Cost.BreathCount > 0 && Inventory.CountItem(Cost.BreathId) < Cost.BreathCount) return Missing(Cost.BreathId);
		if (Inventory.GetGold() < Cost.Gold)
		{
			OutReason = NSLOCTEXT("Refine", "NotEnoughGold", "골드가 부족합니다.");
			return false;
		}
		return true;
	}

	FRefineOutcome TryRefine(UEquipmentComponent& Equipment, EEquipSlot Slot, int32 BreathUse)
	{
		FRefineOutcome Out;
		AActor* Owner = Equipment.GetOwner();
		UInventoryComponent* Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
		const FEquipmentInstance* Worn = Equipment.GetEquipped(Slot);
		FEquipmentInstance* Instance = (Inventory && Worn) ? Inventory->FindEquipmentMutable(Worn->InstanceId) : nullptr;
		if (!Instance) return Out;

		const FRefinePreview P = Preview(*Instance, BreathUse);
		Out.NewLevel = Instance->RefineLevel;
		if (!P.bValid)
		{
			Out.Result = ERefineResult::MaxLevel;
			Out.FailReason = NSLOCTEXT("Refine", "MaxLevel", "더 이상 재련할 수 없습니다.");
			return Out;
		}
		if (!Instance->bGrowthDone)
		{
			Out.Result = ERefineResult::NeedGrowth;
			Out.FailReason = NSLOCTEXT("Refine", "NeedGrowth", "장비 성장 완료 후, 재련할 수 있습니다.");
			return Out;
		}
		if (!HasMaterials(*Inventory, P.Cost, Out.FailReason))
		{
			Out.Result = ERefineResult::NotEnoughMaterials;
			return Out;
		}

		// 재료 차감 — 위에서 다 있는 걸 확인했으니 부분 차감은 없다
		Inventory->RemoveItem(P.Cost.StoneId, P.Cost.StoneCount);
		if (P.Cost.LeapCount > 0) Inventory->RemoveItem(LoAItems::LeapStone, P.Cost.LeapCount);
		if (P.Cost.BreathCount > 0) Inventory->RemoveItem(P.Cost.BreathId, P.Cost.BreathCount);
		Inventory->SpendGold(P.Cost.Gold);

		Out.ChanceUsed = P.Chance;
		Out.bWasGuaranteed = P.bGuaranteed;
		if (P.bGuaranteed || FMath::FRand() < P.Chance)
		{
			Instance->RefineLevel = P.TargetLevel;
			Instance->ArtisanEnergy = 0.f;
			Instance->FailBonusSteps = 0;
			Instance->bGrowthDone = false;  // 다음 단계는 다시 성장부터
			Out.Result = ERefineResult::Success;
		}
		else
		{
			Out.EnergyGained = P.Chance * ArtisanEnergyRate;
			Instance->ArtisanEnergy = FMath::Min(1.f, Instance->ArtisanEnergy + Out.EnergyGained);
			Instance->FailBonusSteps = FMath::Min(Instance->FailBonusSteps + 1, MaxFailBonusSteps);
			Out.Result = ERefineResult::Fail;
		}
		Out.NewLevel = Instance->RefineLevel;
		Out.EnergyAfter = Instance->ArtisanEnergy;

		const FGuid Id = Instance->InstanceId;
		UE_LOG(LogTemp, Log, TEXT("[Refine] %s +%d → %s (확률 %.2f%%%s, 숨결 %d개, 장인의 기운 %.2f%%)"),
			*Instance->ItemId.ToString(), P.CurrentLevel, Out.Result == ERefineResult::Success ? TEXT("성공") : TEXT("실패"),
			P.Chance * 100.f, P.bGuaranteed ? TEXT(" 확정") : TEXT(""), P.Cost.BreathCount, Out.EnergyAfter * 100.f);

		// Instance 포인터는 여기까지 — 아래 브로드캐스트가 맵을 건드릴 수 있다
		Inventory->NotifyEquipmentChanged(Id);
		Equipment.RefreshEquipped();
		Equipment.SaveNow();
		return Out;
	}

	FRefineManyOutcome TryRefineMany(UEquipmentComponent& Equipment, EEquipSlot Slot, int32 BreathUse, int32 MaxTries)
	{
		FRefineManyOutcome Out;
		for (int32 i = 0; i < MaxTries; ++i)
		{
			const FRefineOutcome One = TryRefine(Equipment, Slot, BreathUse);
			Out.NewLevel = One.NewLevel;
			if (One.Result == ERefineResult::Success)
			{
				++Out.Tries;
				Out.bSucceeded = true;
				break;
			}
			if (One.Result == ERefineResult::Fail)
			{
				++Out.Tries;
				Out.EnergyGained += One.EnergyGained;
				continue;
			}
			// 재료 부족·성장 전·최대 단계 — 첫 시도부터면 그 이유를, 중간이면 "재료가 떨어져 멈춤"
			Out.bStoppedByMaterials = Out.Tries > 0;
			Out.FailReason = One.FailReason;
			break;
		}
		return Out;
	}

	int32 CountAffordableAttempts(const UInventoryComponent& Inventory, const FRefineCost& Cost)
	{
		int32 Count = MAX_int32;
		auto Limit = [&Count](int64 Have, int64 Need) { if (Need > 0) Count = static_cast<int32>(FMath::Min<int64>(Count, Have / Need)); };
		Limit(Inventory.CountItem(Cost.StoneId), Cost.StoneCount);
		Limit(Inventory.CountItem(LoAItems::LeapStone), Cost.LeapCount);
		Limit(Inventory.GetGold(), Cost.Gold);
		Limit(Inventory.CountItem(Cost.BreathId), Cost.BreathCount);
		return Count == MAX_int32 ? 0 : Count;
	}

	FGrowthCost GetGrowthCost(int32 TargetLevel, bool bWeapon)
	{
		FGrowthCost Cost;
		const FRefineTier* Tier = Refine_FindTier(TargetLevel);
		if (!Tier) return Cost;
		const float Mul = bWeapon ? WeaponCostMultiplier : 1.f;
		Cost.Fragments = FMath::CeilToInt(Tier->GrowthFragments * Mul);
		Cost.Silling = static_cast<int64>(FMath::CeilToDouble(Tier->GrowthSilling * Mul));
		return Cost;
	}

	bool TryGrow(UEquipmentComponent& Equipment, EEquipSlot Slot, FText& OutReason)
	{
		AActor* Owner = Equipment.GetOwner();
		UInventoryComponent* Inventory = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
		const FEquipmentInstance* Worn = Equipment.GetEquipped(Slot);
		FEquipmentInstance* Instance = (Inventory && Worn) ? Inventory->FindEquipmentMutable(Worn->InstanceId) : nullptr;
		if (!Instance || Instance->bGrowthDone) return false;
		const int32 Target = Instance->RefineLevel + 1;
		if (Target > LoAEquipRules::MaxRefineLevel)
		{
			OutReason = NSLOCTEXT("Refine", "MaxLevel", "더 이상 재련할 수 없습니다.");
			return false;
		}

		const FGrowthCost Cost = GetGrowthCost(Target, Refine_IsWeapon(*Instance));
		if (Inventory->CountItem(LoAItems::FateFragment) < Cost.Fragments)
		{
			OutReason = NSLOCTEXT("Refine", "NotEnoughFragments", "운명의 파편이 부족합니다.");
			return false;
		}
		if (Inventory->GetSilling() < Cost.Silling)
		{
			OutReason = NSLOCTEXT("Refine", "NotEnoughSillingGrowth", "실링이 부족합니다.");
			return false;
		}
		Inventory->RemoveItem(LoAItems::FateFragment, Cost.Fragments);
		Inventory->SpendSilling(Cost.Silling);
		Instance->bGrowthDone = true;
		const FGuid Id = Instance->InstanceId;
		UE_LOG(LogTemp, Log, TEXT("[Refine] %s 성장 완료 (+%d → +%d 준비, 파편 %d, 실링 %lld)"),
			*Instance->ItemId.ToString(), Target - 1, Target, Cost.Fragments, Cost.Silling);

		Inventory->NotifyEquipmentChanged(Id);
		Equipment.RefreshEquipped();
		Equipment.SaveNow();
		return true;
	}

	void Simulate(int32 TargetLevel, int32 Runs, double& OutAverageTries, int32& OutWorstTries)
	{
		OutAverageTries = 0.0;
		OutWorstTries = 0;
		const float Base = GetBaseChance(TargetLevel);
		if (Base <= 0.f || Runs <= 0) return;

		int64 Total = 0;
		for (int32 r = 0; r < Runs; ++r)
		{
			float Energy = 0.f;
			int32 FailSteps = 0;
			int32 Tries = 0;
			while (true)
			{
				++Tries;
				if (Refine_IsEnergyFull(Energy)) break;
				const float Chance = Refine_ChanceWithFailBonus(Base, FailSteps);
				if (FMath::FRand() < Chance) break;
				Energy = FMath::Min(1.f, Energy + Chance * ArtisanEnergyRate);
				FailSteps = FMath::Min(FailSteps + 1, MaxFailBonusSteps);
			}
			Total += Tries;
			OutWorstTries = FMath::Max(OutWorstTries, Tries);
		}
		OutAverageTries = static_cast<double>(Total) / Runs;
	}
}
