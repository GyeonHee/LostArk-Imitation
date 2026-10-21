#include "Item/ItemTypes.h"
#include "Engine/Texture2D.h"
#include "NiagaraSystem.h"

namespace LoAItems
{
	const FName SpiritRecoveryPotion(TEXT("SpiritRecoveryPotion"));

	namespace
	{
		TMap<FName, FItemDefinition> BuildRegistry()
		{
			TMap<FName, FItemDefinition> Map;

			FItemDefinition Potion;
			Potion.ItemId = SpiritRecoveryPotion;
			Potion.DisplayName = NSLOCTEXT("Item", "SpiritRecoveryPotion", "정령의 회복약");
			Potion.Category = EItemCategory::BattleItem;
			Potion.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_SpiritRecoveryPotion.T_SpiritRecoveryPotion")));
			Potion.MaxStack = 999;
			Potion.SellPrice = 1;
			Potion.bIsBattleItem = true;
			Potion.BattleEquipMax = 7;
			Potion.HealMaxHPRatio = 0.6f;
			Potion.Cooldown = 10.f;
			Potion.UseVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Heal.NS_Heal")));
			Potion.UseVFXDuration = 1.5f;
			Map.Add(Potion.ItemId, Potion);

			// ── 시작 장비 6부위 (1640 세트) ──
			// 재련 +10에서 방어구 5부위 지능 합 180,000 + 무기 무기공격력 90,000 → 맨몸(지능 20,000 / 무기공격력 10,000)과 더해
			// 지능 200,000 / 무기공격력 100,000 = 장비 도입 전 기준 스펙과 같다. 재련 0단계 값 = +10 값 ÷ 1.2
			struct FStarterPart { EEquipSlot Slot; const TCHAR* Id; FText Name; const TCHAR* Icon; double Primary; double Weapon; };
			const FStarterPart Parts[] = {
				{ EEquipSlot::Helmet,   TEXT("Equip_StarterHelmet"),   NSLOCTEXT("Item", "StarterHelmet",   "도전자의 마력 모자"),   TEXT("T_Equip_Helmet"),   30000.0, 0.0 },
				{ EEquipSlot::Shoulder, TEXT("Equip_StarterShoulder"), NSLOCTEXT("Item", "StarterShoulder", "도전자의 마력 견갑"),   TEXT("T_Equip_Shoulder"), 30000.0, 0.0 },
				{ EEquipSlot::Chest,    TEXT("Equip_StarterChest"),    NSLOCTEXT("Item", "StarterChest",    "도전자의 마력 로브"),   TEXT("T_Equip_Chest"),    30000.0, 0.0 },
				{ EEquipSlot::Pants,    TEXT("Equip_StarterPants"),    NSLOCTEXT("Item", "StarterPants",    "도전자의 마력 하의"),   TEXT("T_Equip_Pants"),    30000.0, 0.0 },
				{ EEquipSlot::Gloves,   TEXT("Equip_StarterGloves"),   NSLOCTEXT("Item", "StarterGloves",   "도전자의 마력 장갑"),   TEXT("T_Equip_Gloves"),   30000.0, 0.0 },
				{ EEquipSlot::Weapon,   TEXT("Equip_StarterWeapon"),   NSLOCTEXT("Item", "StarterWeapon",   "도전자의 마력 지팡이"), TEXT("T_Equip_Weapon"),   0.0, 75000.0 },
			};
			for (const FStarterPart& Part : Parts)
			{
				FItemDefinition Def;
				Def.ItemId = FName(Part.Id);
				Def.DisplayName = Part.Name;
				Def.Category = EItemCategory::Equipment;
				Def.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/LostArk/UI/Item/%s.%s"), Part.Icon, Part.Icon)));
				Def.MaxStack = 1;
				Def.SortOrder = static_cast<int32>(Part.Slot);
				Def.SellPrice = 0; // 장비 판매는 판매 목록이 인스턴스를 다룰 수 있게 된 뒤에
				Def.EquipSlot = Part.Slot;
				Def.BaseItemLevel = 1590; // +10 → 1640
				Def.PrimaryStat = Part.Primary;
				Def.WeaponPower = Part.Weapon;
				Map.Add(Def.ItemId, Def);
			}

			return Map;
		}
	}

	FName GetStarterEquipment(EEquipSlot Slot)
	{
		static const FName Ids[] = {
			TEXT("Equip_StarterHelmet"), TEXT("Equip_StarterShoulder"), TEXT("Equip_StarterChest"),
			TEXT("Equip_StarterPants"), TEXT("Equip_StarterGloves"), TEXT("Equip_StarterWeapon"),
		};
		const int32 Index = static_cast<int32>(Slot);
		return Index >= 0 && Index < UE_ARRAY_COUNT(Ids) ? Ids[Index] : NAME_None;
	}

	int32 GetEquipmentItemLevel(const FEquipmentInstance& Instance)
	{
		const FItemDefinition* Def = Find(Instance.ItemId);
		return Def ? Def->BaseItemLevel + Instance.RefineLevel * LoAEquipRules::ItemLevelPerRefine : 0;
	}

	double GetRefineStatMultiplier(int32 RefineLevel)
	{
		return 1.0 + LoAEquipRules::StatPerRefine * FMath::Max(0, RefineLevel);
	}

	FString FormatWithCommas(int64 Value)
	{
		const bool bNegative = Value < 0;
		const FString Digits = FString::Printf(TEXT("%lld"), bNegative ? -Value : Value);
		FString Out;
		const int32 Len = Digits.Len();
		for (int32 i = 0; i < Len; ++i)
		{
			if (i > 0 && (Len - i) % 3 == 0) Out.AppendChar(TEXT(','));
			Out.AppendChar(Digits[i]);
		}
		return bNegative ? TEXT("-") + Out : Out;
	}

	const FItemDefinition* Find(FName ItemId)
	{
		// 해시맵 — 이름으로 O(1). 처음 부를 때 한 번만 만든다
		static const TMap<FName, FItemDefinition> Registry = BuildRegistry();
		return ItemId.IsNone() ? nullptr : Registry.Find(ItemId);
	}
}
