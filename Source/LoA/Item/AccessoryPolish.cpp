#include "Item/AccessoryPolish.h"
#include "Item/EquipmentComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/BraceletStone.h"

namespace LoAPolish
{
	namespace
	{
		using E = EPolishEffect;

		// itemRandom 5009/5109/5209 — 값은 게임 데이터(만분율 100 = 1%)를 % 단위로 옮김
		const FEffectDef Polish_Defs[] = {
			// 목걸이
			{ E::AdditionalDamage,   TEXT("추가 피해"),                     { 0.70f, 1.60f, 2.60f }, true,  true },
			{ E::DamageToEnemies,    TEXT("적에게 주는 피해"),              { 0.55f, 1.20f, 2.00f }, true,  true },
			{ E::SupportGauge,       TEXT("세레나데, 신앙, 조화 게이지 획득량"), { 1.60f, 3.60f, 6.00f }, true,  false },
			{ E::BrandPower,         TEXT("낙인력"),                        { 2.15f, 4.80f, 8.00f }, true,  false },
			// 귀걸이
			{ E::AttackPowerPercent, TEXT("공격력"),                        { 0.40f, 0.95f, 1.55f }, true,  true },
			{ E::WeaponPowerPercent, TEXT("무기 공격력"),                   { 0.80f, 1.80f, 3.00f }, true,  true },
			{ E::PartyHeal,          TEXT("파티원 회복 효과"),              { 0.95f, 2.10f, 3.50f }, true,  false },
			{ E::PartyShield,        TEXT("파티원 보호막 효과"),            { 0.95f, 2.10f, 3.50f }, true,  false },
			// 반지
			{ E::CritRate,           TEXT("치명타 적중률"),                 { 0.40f, 0.95f, 1.55f }, true,  true },
			{ E::CritDamage,         TEXT("치명타 피해"),                   { 1.10f, 2.40f, 4.00f }, true,  true },
			{ E::AllyAttackBuff,     TEXT("아군 공격력 강화 효과"),         { 1.35f, 3.00f, 5.00f }, true,  false },
			{ E::AllyDamageBuff,     TEXT("아군 피해량 강화 효과"),         { 2.00f, 4.50f, 7.50f }, true,  false },
			// 공통
			{ E::MaxHP,              TEXT("최대 생명력"),                   { 1300.f, 3250.f, 6500.f }, false, true },
			{ E::AttackPowerFlat,    TEXT("공격력"),                        { 80.f, 195.f, 390.f },     false, true },
			{ E::WeaponPowerFlat,    TEXT("무기 공격력"),                   { 195.f, 480.f, 960.f },    false, true },
			{ E::MaxMP,              TEXT("최대 마나"),                     { 6.f, 15.f, 30.f },        false, true },
			{ E::StatusDuration,     TEXT("상태이상 공격 지속시간"),        { 0.20f, 0.50f, 1.00f },    true,  false },
			{ E::CombatHPRegen,      TEXT("전투 중 생명력 회복량"),         { 10.f, 25.f, 50.f },       false, false },
		};

		// 등급 확률 — 게임 데이터 ratio 6300 / 3000 / 700
		constexpr float Polish_GradeChance[3] = { 0.63f, 0.30f, 0.07f };

		bool Polish_IsNecklace(EEquipSlot Part) { return Part == EEquipSlot::Necklace; }
		bool Polish_IsEarring(EEquipSlot Part) { return Part == EEquipSlot::Earring1 || Part == EEquipSlot::Earring2; }
		bool Polish_IsRing(EEquipSlot Part) { return Part == EEquipSlot::Ring1 || Part == EEquipSlot::Ring2; }

		FString Polish_Number(float Value, bool bPercent)
		{
			if (bPercent) return FString::Printf(TEXT("%.2f%%"), Value);
			return LoAItems::FormatWithCommas(FMath::RoundToInt64(Value));
		}
	}

	const FEffectDef& GetDef(EPolishEffect Effect)
	{
		for (const FEffectDef& Def : Polish_Defs)
		{
			if (Def.Effect == Effect) return Def;
		}
		return Polish_Defs[0];
	}

	TArray<EPolishEffect> GetEffectsFor(EEquipSlot Part)
	{
		TArray<EPolishEffect> Out;
		if (Polish_IsNecklace(Part))     Out = { E::AdditionalDamage, E::DamageToEnemies, E::SupportGauge, E::BrandPower };
		else if (Polish_IsEarring(Part)) Out = { E::AttackPowerPercent, E::WeaponPowerPercent, E::PartyHeal, E::PartyShield };
		else if (Polish_IsRing(Part))    Out = { E::CritRate, E::CritDamage, E::AllyAttackBuff, E::AllyDamageBuff };
		else return Out;
		Out.Append({ E::MaxHP, E::AttackPowerFlat, E::WeaponPowerFlat, E::MaxMP, E::StatusDuration, E::CombatHPRegen });
		return Out;
	}

	FString DescribeValue(EPolishEffect Effect, int32 Grade)
	{
		const FEffectDef& Def = GetDef(Effect);
		return FString::Printf(TEXT("%s +%s"), Def.Name, *Polish_Number(Def.Values[FMath::Clamp(Grade, 0, 2)], Def.bPercent));
	}

	FString Describe(const FPolishEffectRoll& Roll)
	{
		return DescribeValue(Roll.Effect, Roll.Grade);
	}

	FLinearColor GetGradeColor(int32 Grade)
	{
		switch (Grade)
		{
		case 2:  return FLinearColor(1.f, 0.84f, 0.2f, 1.f);   // 상 노랑
		case 1:  return FLinearColor(0.85f, 0.45f, 1.f, 1.f);  // 중 보라
		default: return FLinearColor(0.35f, 0.75f, 1.f, 1.f);  // 하 파랑
		}
	}

	const TCHAR* GetGradeName(int32 Grade)
	{
		return Grade >= 2 ? TEXT("상") : (Grade == 1 ? TEXT("중") : TEXT("하"));
	}

	float GetGradeChance(int32 Grade)
	{
		return Polish_GradeChance[FMath::Clamp(Grade, 0, 2)];
	}

	FCost GetCost(int32 NextLevel)
	{
		// 1단계는 로아 연마 창 그대로(파편 10 · 600골드), 2·3단계는 같은 비율로 늘린 추정값
		FCost Cost;
		const int32 Step = FMath::Clamp(NextLevel, 1, MaxLevel);
		Cost.Shards = 10 * Step;
		Cost.Gold = 600 * Step;
		return Cost;
	}

	void AppendModifiers(const FEquipmentInstance& Instance, TArray<FStatModifier>& OutMods)
	{
		auto Add = [&OutMods](EStatModifierTarget Target, double Value)
		{
			FStatModifier& Mod = OutMods.AddDefaulted_GetRef();
			Mod.Target = Target;
			Mod.Value = Value;
		};
		for (const FPolishEffectRoll& Roll : Instance.PolishEffects)
		{
			const FEffectDef& Def = GetDef(Roll.Effect);
			if (!Def.bHasEffect) continue;
			const double V = Def.Values[FMath::Clamp(Roll.Grade, 0, 2)];
			const double Ratio = V / 100.0;
			switch (Roll.Effect)
			{
			case E::AdditionalDamage:   Add(EStatModifierTarget::AdditiveDamage, Ratio); break;        // 추가 피해 — 달인 중첩과 같은 덩어리
			case E::DamageToEnemies:    Add(EStatModifierTarget::MultiplicativeDamage, Ratio); break;  // 적에게 주는 피해 — 따로 곱함
			case E::AttackPowerPercent: Add(EStatModifierTarget::AttackPowerPercent, Ratio); break;
			case E::WeaponPowerPercent: Add(EStatModifierTarget::WeaponPowerPercent, Ratio); break;
			case E::CritRate:           Add(EStatModifierTarget::CritRate, Ratio); break;
			case E::CritDamage:         Add(EStatModifierTarget::CritDamage, Ratio); break;
			case E::MaxHP:              Add(EStatModifierTarget::MaxHPFlat, V); break;
			case E::AttackPowerFlat:    Add(EStatModifierTarget::AttackPowerFlat, V); break;
			case E::WeaponPowerFlat:    Add(EStatModifierTarget::WeaponPower, V); break;
			case E::MaxMP:              Add(EStatModifierTarget::MaxMPFlat, V); break;
			default: break;
			}
		}
	}

	FPolishEffectRoll Roll(EEquipSlot Part, const TArray<FPolishEffectRoll>& Existing)
	{
		// 종류는 남은 것 중 균등(게임 데이터의 종류별 비중이 전부 같다), 등급은 63 / 30 / 7
		TArray<EPolishEffect> Candidates = GetEffectsFor(Part);
		Candidates.RemoveAll([&Existing](EPolishEffect Effect)
		{
			return Existing.ContainsByPredicate([Effect](const FPolishEffectRoll& R) { return R.Effect == Effect; });
		});
		FPolishEffectRoll Result;
		if (Candidates.Num() == 0) return Result;
		Result.Effect = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		const float Pick = FMath::FRand();
		Result.Grade = Pick < Polish_GradeChance[2] ? 2 : (Pick < Polish_GradeChance[2] + Polish_GradeChance[1] ? 1 : 0);
		return Result;
	}

	EResult TryPolish(UInventoryComponent& Inventory, UEquipmentComponent* Equipment, const FGuid& Id, FPolishEffectRoll& OutRoll)
	{
		FEquipmentInstance* Instance = Inventory.FindEquipmentMutable(Id);
		const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
		if (!Instance || !Def || !Def->IsAccessory()) return EResult::NoAccessory;
		if (Instance->PolishLevel >= MaxLevel) return EResult::MaxLevel;

		const FCost Cost = GetCost(Instance->PolishLevel + 1);
		if (Inventory.CountItem(LoAItems::PolishShard) < Cost.Shards) return EResult::NotEnoughShards;
		if (Inventory.GetGold() < Cost.Gold) return EResult::NotEnoughGold;
		Inventory.RemoveItem(LoAItems::PolishShard, Cost.Shards);
		Inventory.SpendGold(Cost.Gold);

		OutRoll = Roll(Def->EquipSlot, Instance->PolishEffects);
		Instance->PolishEffects.Add(OutRoll);
		Instance->PolishLevel = Instance->PolishEffects.Num();
		UE_LOG(LogTemp, Log, TEXT("[Polish] %s 연마 %d단계 — %s (%s)"), *Instance->ItemId.ToString(), Instance->PolishLevel,
			*Describe(OutRoll), GetGradeName(OutRoll.Grade));

		// 소지품 칸·창 갱신 + 착용 중이면 기본 효과·깨달음 포인트·연마 효과 다시 적용 + 즉시 저장
		LoAGearList::CommitChange(&Inventory, Equipment, Id);
		return EResult::Success;
	}

	TArray<FGuid> CollectPolishable(const UInventoryComponent* Inventory)
	{
		TArray<FGuid> Out;
		if (!Inventory) return Out;
		for (int32 i = 0; i < UInventoryComponent::Capacity; ++i)
		{
			if (!Inventory->IsValidIndex(i)) break;
			const FInventorySlot& InvSlot = Inventory->GetSlot(i);
			const FEquipmentInstance* Instance = InvSlot.IsEquipment() ? Inventory->FindEquipment(InvSlot.EquipmentId) : nullptr;
			const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
			if (Def && Def->IsAccessory() && Instance->PolishLevel < MaxLevel) Out.AddUnique(InvSlot.EquipmentId);
		}
		return Out;
	}

	FText GetResultText(EResult Result)
	{
		switch (Result)
		{
		case EResult::NoAccessory:     return NSLOCTEXT("Polish", "NoAccessory", "연마할 장신구가 없습니다.");
		case EResult::MaxLevel:        return NSLOCTEXT("Polish", "MaxLevel", "더 이상 연마할 수 없습니다.");
		case EResult::NotEnoughShards: return NSLOCTEXT("Polish", "NotEnoughShards", "영롱한 보석 파편이 부족합니다.");
		case EResult::NotEnoughGold:   return NSLOCTEXT("Polish", "NotEnoughGold", "골드가 부족합니다.");
		default:                       return FText::GetEmpty();
		}
	}

	void ApplyPresetFullPolish(FEquipmentInstance& Instance)
	{
		const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
		if (!Def || !Def->IsAccessory()) return;
		auto Make = [](EPolishEffect Effect, int32 Grade)
		{
			FPolishEffectRoll R;
			R.Effect = Effect;
			R.Grade = Grade;
			return R;
		};
		Instance.PolishEffects.Reset();
		if (Polish_IsNecklace(Def->EquipSlot))
		{
			Instance.PolishEffects = { Make(E::DamageToEnemies, 1), Make(E::AdditionalDamage, 1), Make(E::AttackPowerFlat, 0) };
		}
		else if (Polish_IsEarring(Def->EquipSlot))
		{
			Instance.PolishEffects = { Make(E::AttackPowerPercent, 1), Make(E::WeaponPowerPercent, 1), Make(E::WeaponPowerFlat, 0) };
		}
		else
		{
			Instance.PolishEffects = { Make(E::CritRate, 1), Make(E::CritDamage, 1), Make(E::MaxHP, 0) };
		}
		Instance.PolishLevel = Instance.PolishEffects.Num();
	}
}
