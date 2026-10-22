#include "Item/ItemTypes.h"
#include "Engine/Texture2D.h"
#include "NiagaraSystem.h"

namespace LoAItems
{
	const FName SpiritRecoveryPotion(TEXT("SpiritRecoveryPotion"));
	const FName GuardianStone(TEXT("GuardianStone"));
	const FName DestructionStone(TEXT("DestructionStone"));
	const FName LeapStone(TEXT("LeapStone"));
	const FName GlacierBreath(TEXT("RefineBreath"));
	const FName LavaBreath(TEXT("LavaBreath"));
	const FName FateFragment(TEXT("FateFragment"));

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

			// ── 재련 재료 (4단계) — 아이콘은 PIL 임시 아이콘 ──
			struct FMaterialDef { FName Id; FText Name; const TCHAR* Icon; int32 MaxStack; int64 SellPrice; };
			const FMaterialDef Materials[] = {
				{ GuardianStone,    NSLOCTEXT("Item", "GuardianStone",    "수호석"), TEXT("T_GuardianStone"),    9999, 10 },
				{ DestructionStone, NSLOCTEXT("Item", "DestructionStone", "파괴석"), TEXT("T_DestructionStone"), 9999, 20 },
				{ LeapStone,        NSLOCTEXT("Item", "LeapStone",        "돌파석"), TEXT("T_LeapStone"),        999,  50 },
				{ GlacierBreath,    NSLOCTEXT("Item", "GlacierBreath",    "빙하의 숨결"), TEXT("T_RefineBreath"), 999,  0 },  // 방어구 재련 — 상점 구매 전용
				{ LavaBreath,       NSLOCTEXT("Item", "LavaBreath",       "용암의 숨결"), TEXT("T_LavaBreath"),   999,  0 },  // 무기 재련
				{ FateFragment,     NSLOCTEXT("Item", "FateFragment",     "운명의 파편"), TEXT("T_FateFragment"), 99999, 1 }, // 장비 성장
			};
			for (int32 i = 0; i < UE_ARRAY_COUNT(Materials); ++i)
			{
				const FMaterialDef& M = Materials[i];
				FItemDefinition Def;
				Def.ItemId = M.Id;
				Def.DisplayName = M.Name;
				Def.Category = EItemCategory::Material;
				Def.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/LostArk/UI/Item/%s.%s"), M.Icon, M.Icon)));
				Def.MaxStack = M.MaxStack;
				Def.SortOrder = i;
				Def.SellPrice = M.SellPrice;
				Map.Add(Def.ItemId, Def);
			}

			// ── 시작 장비 6부위 (1640 세트) — 스탯은 GetGearStats(로아 T4 업화 표). +10이면 방어구 지능 합 284,481 / 무기 공격력 100,036 ──
			struct FStarterPart { EEquipSlot Slot; const TCHAR* Id; FText Name; const TCHAR* Icon; };
			const FStarterPart Parts[] = {
				{ EEquipSlot::Helmet,   TEXT("Equip_StarterHelmet"),   NSLOCTEXT("Item", "StarterHelmet",   "도전자의 마력 모자"),   TEXT("T_Equip_Helmet") },
				{ EEquipSlot::Shoulder, TEXT("Equip_StarterShoulder"), NSLOCTEXT("Item", "StarterShoulder", "도전자의 마력 견갑"),   TEXT("T_Equip_Shoulder") },
				{ EEquipSlot::Chest,    TEXT("Equip_StarterChest"),    NSLOCTEXT("Item", "StarterChest",    "도전자의 마력 로브"),   TEXT("T_Equip_Chest") },
				{ EEquipSlot::Pants,    TEXT("Equip_StarterPants"),    NSLOCTEXT("Item", "StarterPants",    "도전자의 마력 하의"),   TEXT("T_Equip_Pants") },
				{ EEquipSlot::Gloves,   TEXT("Equip_StarterGloves"),   NSLOCTEXT("Item", "StarterGloves",   "도전자의 마력 장갑"),   TEXT("T_Equip_Gloves") },
				{ EEquipSlot::Weapon,   TEXT("Equip_StarterWeapon"),   NSLOCTEXT("Item", "StarterWeapon",   "도전자의 마력 지팡이"), TEXT("T_Equip_Weapon") },
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
				Def.SellPrice = 100; // 재련 단계와 무관하게 고정 — 잘못 팔았으면 LoAGiveItem으로 다시(재련 0단계)
				Def.EquipSlot = Part.Slot;
				Def.BaseItemLevel = 1590; // +10 → 1640
				Map.Add(Def.ItemId, Def);
			}

			// ── 장신구 (6단계) — 시작 세트(도전자의, 특성 고정)와 레이드 드랍(욕망의, 특성 굴림). 재련 없음, 아이템 레벨 없음 ──
			// 목걸이 2종 350~420 / 귀걸이·반지 1종 200~240 — 시작 세트 합 1,500 → 최상급 합 1,800
			struct FAccessoryDef { const TCHAR* Id; FText Name; EEquipSlot Slot; const TCHAR* Icon; int32 Count; int32 Min; int32 Max; int64 SellPrice; };
			const FAccessoryDef Accessories[] = {
				{ TEXT("Acc_StarterNecklace"), NSLOCTEXT("Item", "StarterNecklace", "도전자의 목걸이"), EEquipSlot::Necklace, TEXT("T_Acc_Necklace"), 2, 350, 350, 100 },
				{ TEXT("Acc_StarterEarring"),  NSLOCTEXT("Item", "StarterEarring",  "도전자의 귀걸이"), EEquipSlot::Earring1, TEXT("T_Acc_Earring"),  1, 200, 200, 100 },
				{ TEXT("Acc_StarterRing"),     NSLOCTEXT("Item", "StarterRing",     "도전자의 반지"),   EEquipSlot::Ring1,    TEXT("T_Acc_Ring"),     1, 200, 200, 100 },
				{ TEXT("Acc_DesireNecklace"),  NSLOCTEXT("Item", "DesireNecklace",  "욕망의 목걸이"),   EEquipSlot::Necklace, TEXT("T_Acc_Necklace"), 2, 350, 420, 3000 },
				{ TEXT("Acc_DesireEarring"),   NSLOCTEXT("Item", "DesireEarring",   "욕망의 귀걸이"),   EEquipSlot::Earring1, TEXT("T_Acc_Earring"),  1, 200, 240, 2000 },
				{ TEXT("Acc_DesireRing"),      NSLOCTEXT("Item", "DesireRing",      "욕망의 반지"),     EEquipSlot::Ring1,    TEXT("T_Acc_Ring"),     1, 200, 240, 2000 },
			};
			for (const FAccessoryDef& Acc : Accessories)
			{
				FItemDefinition Def;
				Def.ItemId = FName(Acc.Id);
				Def.DisplayName = Acc.Name;
				Def.Category = EItemCategory::Equipment;
				Def.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/LostArk/UI/Item/%s.%s"), Acc.Icon, Acc.Icon)));
				Def.MaxStack = 1;
				Def.SortOrder = static_cast<int32>(Acc.Slot);
				Def.SellPrice = Acc.SellPrice;
				Def.EquipSlot = Acc.Slot;
				Def.AccessoryStatCount = Acc.Count;
				Def.AccessoryStatMin = Acc.Min;
				Def.AccessoryStatMax = Acc.Max;
				Map.Add(Def.ItemId, Def);
			}

			return Map;
		}
	}

	TArray<FStatRoll> RollAccessoryStats(const FItemDefinition& Def)
	{
		TArray<FStatRoll> Out;
		TArray<ELoACombatStat> Pool = { ELoACombatStat::Crit, ELoACombatStat::Specialization, ELoACombatStat::Swiftness, ELoACombatStat::Domination };
		for (int32 i = 0; i < Def.AccessoryStatCount && Pool.Num() > 0; ++i)
		{
			const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
			FStatRoll& Roll = Out.AddDefaulted_GetRef();
			Roll.Stat = Pool[Pick];
			Roll.Value = FMath::RandRange(Def.AccessoryStatMin, FMath::Max(Def.AccessoryStatMin, Def.AccessoryStatMax));
			Pool.RemoveAtSwap(Pick);
		}
		return Out;
	}

	FString GetEquipmentBadge(const FEquipmentInstance& Instance)
	{
		if (Instance.StatRolls.Num() == 0) return FString::Printf(TEXT("+%d"), Instance.RefineLevel);
		FString Badge;
		for (const FStatRoll& Roll : Instance.StatRolls)
		{
			Badge += StaticEnum<ELoACombatStat>()->GetDisplayNameTextByValue(static_cast<int64>(Roll.Stat)).ToString().Left(1);
		}
		return Badge;
	}

	FName GetStarterAccessory(EEquipSlot Slot)
	{
		switch (Slot)
		{
		case EEquipSlot::Necklace: return TEXT("Acc_StarterNecklace");
		case EEquipSlot::Earring1:
		case EEquipSlot::Earring2: return TEXT("Acc_StarterEarring");
		case EEquipSlot::Ring1:
		case EEquipSlot::Ring2:    return TEXT("Acc_StarterRing");
		default:                   return NAME_None;
		}
	}

	TArray<FStatRoll> GetStarterAccessoryStats(EEquipSlot Slot)
	{
		// 시작 세트 = 치명 750 + 특화 750 (로아 소서리스 흔한 "치특"). 예전 DA의 치명 280을 대신하며 K를 다시 맞춤
		auto One = [](ELoACombatStat Stat, int32 Value) { FStatRoll R; R.Stat = Stat; R.Value = Value; return R; };
		switch (Slot)
		{
		case EEquipSlot::Necklace: return { One(ELoACombatStat::Crit, 350), One(ELoACombatStat::Specialization, 350) };
		case EEquipSlot::Earring1: return { One(ELoACombatStat::Crit, 200) };
		case EEquipSlot::Earring2: return { One(ELoACombatStat::Specialization, 200) };
		case EEquipSlot::Ring1:    return { One(ELoACombatStat::Crit, 200) };
		case EEquipSlot::Ring2:    return { One(ELoACombatStat::Specialization, 200) };
		default:                   return {};
		}
	}

	FName GetRandomDropAccessory()
	{
		static const FName Ids[] = { TEXT("Acc_DesireNecklace"), TEXT("Acc_DesireEarring"), TEXT("Acc_DesireRing") };
		return Ids[FMath::RandRange(0, UE_ARRAY_COUNT(Ids) - 1)];
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

	namespace
	{
		struct FGearStatRow { double PhysDefense; double MagicDefense; double PrimaryStat; double Vitality; };
		constexpr int32 GearLevelCount = LoAEquipRules::MaxRefineLevel + 1;

	// 로스트아크 T4 운명의 업화 장비(1590 + 5/단계) 재련 단계별 기본 효과 — Maxroll 게임 데이터(laplanner/game/stats.json, 2026-09-30) itemLevel 표에서 옮김
	// 방어구: { 물리 방어력, 마법 방어력, 주 스탯(힘·민첩·지능 — 직업 주 스탯에 들어감), 체력 } / 무기: 무기 공격력. 방어력은 직업 방어 계수 곱하기 전 값
	const FGearStatRow Gear_Helmet[GearLevelCount] = {
		{ 5031, 5590, 34806, 5114 }, { 5167, 5741, 38669, 5505 }, { 5304, 5894, 42565, 5897 }, { 5443, 6048, 46495, 6290 },
		{ 5583, 6203, 50459, 6685 }, { 5670, 6300, 51572, 6782 }, { 5759, 6399, 52722, 6881 }, { 5850, 6499, 53911, 6981 },
		{ 5941, 6601, 55139, 7083 }, { 6035, 6705, 56409, 7187 }, { 6130, 6811, 57721, 7293 }, { 6226, 6918, 59077, 7401 },
		{ 6324, 7027, 60478, 7510 }, { 6428, 7143, 61991, 7626 }, { 6534, 7260, 63556, 7745 }, { 6642, 7380, 65176, 7865 },
		{ 6752, 7502, 66852, 7988 }, { 6863, 7626, 68588, 8113 }, { 6977, 7752, 70384, 8240 }, { 7092, 7880, 72243, 8369 },
		{ 7209, 8010, 74167, 8501 }, { 7329, 8143, 76158, 8635 }, { 7450, 8278, 78219, 8771 }, { 7574, 8416, 80352, 8910 },
		{ 7700, 8555, 82560, 9052 }, { 7828, 8697, 84845, 9196 },
	};
	const FGearStatRow Gear_Shoulder[GearLevelCount] = {
		{ 5590, 5031, 37043, 4432 }, { 5741, 5167, 41155, 4771 }, { 5894, 5304, 45302, 5110 }, { 6048, 5443, 49483, 5452 },
		{ 6203, 5583, 53703, 5794 }, { 6300, 5670, 54887, 5878 }, { 6399, 5759, 56111, 5964 }, { 6499, 5850, 57376, 6051 },
		{ 6601, 5941, 58684, 6139 }, { 6705, 6035, 60035, 6229 }, { 6811, 6130, 61431, 6321 }, { 6918, 6226, 62875, 6414 },
		{ 7027, 6324, 64366, 6509 }, { 7143, 6428, 65976, 6610 }, { 7260, 6534, 67642, 6712 }, { 7380, 6642, 69366, 6816 },
		{ 7502, 6752, 71150, 6923 }, { 7626, 6863, 72997, 7031 }, { 7752, 6977, 74909, 7141 }, { 7880, 7092, 76887, 7253 },
		{ 8010, 7209, 78935, 7367 }, { 8143, 7329, 81054, 7484 }, { 8278, 7450, 83248, 7602 }, { 8416, 7574, 85518, 7722 },
		{ 8555, 7700, 87868, 7845 }, { 8697, 7828, 90300, 7970 },
	};
	const FGearStatRow Gear_Chest[GearLevelCount] = {
		{ 6708, 6149, 27845, 6819 }, { 6889, 6315, 30936, 7339 }, { 7072, 6483, 34053, 7862 }, { 7257, 6652, 37197, 8387 },
		{ 7444, 6823, 40368, 8914 }, { 7560, 6930, 41258, 9043 }, { 7679, 7039, 42178, 9175 }, { 7799, 7149, 43129, 9308 },
		{ 7922, 7262, 44112, 9444 }, { 8046, 7376, 45128, 9583 }, { 8173, 7492, 46177, 9724 }, { 8301, 7610, 47262, 9867 },
		{ 8432, 7730, 48383, 10013 }, { 8571, 7857, 49593, 10168 }, { 8712, 7986, 50845, 10326 }, { 8856, 8118, 52141, 10487 },
		{ 9002, 8252, 53483, 10650 }, { 9151, 8388, 54871, 10817 }, { 9302, 8527, 56308, 10986 }, { 9456, 8668, 57795, 11159 },
		{ 9612, 8811, 59334, 11334 }, { 9772, 8957, 60927, 11513 }, { 9934, 9106, 62576, 11695 }, { 10099, 9257, 64283, 11880 },
		{ 10266, 9411, 66049, 12069 }, { 10437, 9567, 67877, 12261 },
	};
	const FGearStatRow Gear_Pants[GearLevelCount] = {
		{ 6149, 6708, 30082, 5796 }, { 6315, 6889, 33421, 6238 }, { 6483, 7072, 36788, 6683 }, { 6652, 7257, 40185, 7129 },
		{ 6823, 7444, 43611, 7577 }, { 6930, 7560, 44573, 7687 }, { 7039, 7679, 45567, 7798 }, { 7149, 7799, 46594, 7912 },
		{ 7262, 7922, 47656, 8028 }, { 7376, 8046, 48753, 8146 }, { 7492, 8173, 49887, 8265 }, { 7610, 8301, 51059, 8387 },
		{ 7730, 8432, 52270, 8511 }, { 7857, 8571, 53577, 8643 }, { 7986, 8712, 54930, 8777 }, { 8118, 8856, 56330, 8914 },
		{ 8252, 9002, 57779, 9053 }, { 8388, 9151, 59279, 9194 }, { 8527, 9302, 60831, 9338 }, { 8668, 9456, 62438, 9485 },
		{ 8811, 9612, 64101, 9634 }, { 8957, 9772, 65822, 9786 }, { 9106, 9934, 67603, 9941 }, { 9257, 10099, 69447, 10098 },
		{ 9411, 10266, 71355, 10259 }, { 9567, 10437, 73330, 10422 },
	};
	const FGearStatRow Gear_Gloves[GearLevelCount] = {
		{ 4472, 4472, 41767, 3410 }, { 4593, 4593, 46403, 3670 }, { 4715, 4715, 51079, 3931 }, { 4838, 4838, 55794, 4194 },
		{ 4963, 4963, 60552, 4457 }, { 5040, 5040, 61887, 4522 }, { 5119, 5119, 63267, 4588 }, { 5200, 5200, 64693, 4654 },
		{ 5281, 5281, 66167, 4722 }, { 5364, 5364, 67691, 4792 }, { 5449, 5449, 69265, 4862 }, { 5534, 5534, 70892, 4934 },
		{ 5622, 5622, 72574, 5007 }, { 5714, 5714, 74389, 5084 }, { 5808, 5808, 76267, 5163 }, { 5904, 5904, 78211, 5244 },
		{ 6002, 6002, 80223, 5325 }, { 6101, 6101, 82306, 5409 }, { 6201, 6201, 84461, 5493 }, { 6304, 6304, 86692, 5580 },
		{ 6408, 6408, 89000, 5667 }, { 6515, 6515, 91390, 5757 }, { 6623, 6623, 93863, 5848 }, { 6733, 6733, 96423, 5940 },
		{ 6844, 6844, 99072, 6035 }, { 6958, 6958, 101815, 6131 },
	};
	const double Gear_WeaponPower[GearLevelCount] = {
		60362, 67051, 73796, 80599, 87463, 89390, 91381, 93439, 95566,
		97764, 100036, 102384, 104811, 107429, 110139, 112944, 115847, 118851,
		121961, 125180, 128511, 131959, 135527, 139221, 143044, 147000,
	};
	}

	FGearStats GetGearStats(EEquipSlot Slot, int32 RefineLevel)
	{
		FGearStats Out;
		const int32 L = FMath::Clamp(RefineLevel, 0, GearLevelCount - 1);
		const FGearStatRow* Table = nullptr;
		switch (Slot)
		{
		case EEquipSlot::Weapon:
			Out.WeaponPower = Gear_WeaponPower[L];
			return Out;
		case EEquipSlot::Helmet:   Table = Gear_Helmet; break;
		case EEquipSlot::Shoulder: Table = Gear_Shoulder; break;
		case EEquipSlot::Chest:    Table = Gear_Chest; break;
		case EEquipSlot::Pants:    Table = Gear_Pants; break;
		case EEquipSlot::Gloves:   Table = Gear_Gloves; break;
		default: return Out;
		}
		Out.PhysDefense = Table[L].PhysDefense;
		Out.MagicDefense = Table[L].MagicDefense;
		Out.PrimaryStat = Table[L].PrimaryStat;
		Out.Vitality = Table[L].Vitality;
		return Out;
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

namespace LoAEquipRules
{
	bool CanEquipInSlot(EEquipSlot DefSlot, EEquipSlot Slot)
	{
		if (DefSlot == Slot) return true;
		if (DefSlot == EEquipSlot::Earring1 && Slot == EEquipSlot::Earring2) return true;
		if (DefSlot == EEquipSlot::Ring1 && Slot == EEquipSlot::Ring2) return true;
		return false;
	}
}
