#include "Item/BraceletStone.h"
#include "Item/BuildTypes.h"
#include "Item/InventoryComponent.h"
#include "Item/EquipmentComponent.h"

// ═══ 팔찌 ════════════════════════════════════════════════════════════════════

namespace LoABracelet
{
	namespace
	{
		using B = EBraceletEffect;

		// itemRandom 213300023#5 — 특수 효과 수치는 % 단위(1.5 = 1.5%) 또는 고정값
		const FEffectDef Bracelet_Defs[] = {
			{ B::PrimaryStat,      TEXT("주 스탯"),                   { 0.f, 0.f, 0.f }, false, true },
			{ B::Vitality,         TEXT("체력"),                      { 0.f, 0.f, 0.f }, false, true },
			{ B::Crit,             TEXT("치명"),                      { 0.f, 0.f, 0.f }, false, true },
			{ B::Specialization,   TEXT("특화"),                      { 0.f, 0.f, 0.f }, false, true },
			{ B::Domination,       TEXT("제압"),                      { 0.f, 0.f, 0.f }, false, true },
			{ B::Swiftness,        TEXT("신속"),                      { 0.f, 0.f, 0.f }, false, true },
			{ B::Endurance,        TEXT("인내"),                      { 0.f, 0.f, 0.f }, false, true },
			{ B::Expertise,        TEXT("숙련"),                      { 0.f, 0.f, 0.f }, false, false },
			{ B::DamageToEnemies,  TEXT("적에게 주는 피해"),          { 1.5f, 2.f, 2.5f }, true, true },
			{ B::AdditionalDamage, TEXT("추가 피해"),                 { 2.5f, 3.f, 3.5f }, true, true },
			{ B::CritRate,         TEXT("치명타 적중률"),             { 2.6f, 3.4f, 4.2f }, true, true },
			{ B::CritDamage,       TEXT("치명타 피해"),               { 5.2f, 6.8f, 8.4f }, true, true },
			{ B::WeaponPower,      TEXT("무기 공격력"),               { 6300.f, 7200.f, 8100.f }, false, true },
			{ B::OnHitWeaponPower, TEXT("적중 시 무기 공격력"),       { 1000.f, 1160.f, 1320.f }, false, true },
			{ B::StaggeredDamage,  TEXT("적에게 주는 피해"),          { 1.5f, 2.f, 2.5f }, true, true },
			{ B::NonDirectional,   TEXT("비방향성 공격 피해"),        { 2.f, 2.5f, 3.f }, true, true },
			{ B::BackAttack,       TEXT("백어택 피해"),               { 2.f, 2.5f, 3.f }, true, false },
			{ B::HeadAttack,       TEXT("헤드어택 피해"),             { 2.f, 2.5f, 3.f }, true, false },
			{ B::PhysDefense,      TEXT("물리 방어력"),               { 4000.f, 5000.f, 6000.f }, false, true },
			{ B::MagicDefense,     TEXT("마법 방어력"),               { 4000.f, 5000.f, 6000.f }, false, true },
			{ B::MaxHP,            TEXT("최대 생명력"),               { 8400.f, 11200.f, 14000.f }, false, true },
			{ B::CombatHPRegen,    TEXT("전투 중 생명력 회복량"),     { 80.f, 100.f, 130.f }, false, false },
			{ B::AllyAttackBuff,   TEXT("아군 공격력 강화 효과"),     { 3.f, 4.f, 5.f }, true, false },
			{ B::AllyDamageBuff,   TEXT("아군 피해량 강화 효과"),     { 4.5f, 6.f, 7.5f }, true, false },
			{ B::ChallengeDamage,  TEXT("도전 이하 몬스터 피해"),     { 3.f, 4.f, 5.f }, true, false },
			{ B::ChallengeDefense, TEXT("도전 이하 몬스터 받는 피해 감소"), { 4.f, 6.f, 8.f }, true, false },
			{ B::CCImmunity,       TEXT("적중 시 마비·밀려남 면역"),   { 90.f, 80.f, 70.f }, false, false },
		};

		// 특수 효과 굴림 비중 — 게임 데이터의 종류별 합 (흔한 효과 4,620 / 공격 효과 1,200 / 희귀 550), 등급은 6 : 3 : 1
		struct FSpecialWeight { EBraceletEffect Effect; int32 Weight; };
		const FSpecialWeight Bracelet_SpecialWeights[] = {
			{ B::ChallengeDamage, 4620 }, { B::ChallengeDefense, 4620 }, { B::PhysDefense, 4620 }, { B::MagicDefense, 4620 },
			{ B::MaxHP, 4620 }, { B::CombatHPRegen, 4620 }, { B::CCImmunity, 4620 },
			{ B::DamageToEnemies, 1200 }, { B::AdditionalDamage, 1200 }, { B::BackAttack, 1200 }, { B::HeadAttack, 1200 },
			{ B::NonDirectional, 1200 }, { B::AllyAttackBuff, 1200 }, { B::AllyDamageBuff, 1200 },
			{ B::CritRate, 1200 }, { B::CritDamage, 1200 }, { B::WeaponPower, 1200 },
			{ B::StaggeredDamage, 550 }, { B::OnHitWeaponPower, 550 },
		};

		constexpr int32 Bracelet_OnHitStacks = 6;  // 적중 시 효과 — 상시 최대 중첩으로 계산

		FBraceletEffectRoll Bracelet_Make(EBraceletEffect Effect, int32 Value, int32 Grade)
		{
			FBraceletEffectRoll R;
			R.Effect = Effect;
			R.Value = Value;
			R.Grade = Grade;
			return R;
		}

		int32 Bracelet_RollGrade()
		{
			const float Pick = FMath::FRand();
			return Pick < 0.1f ? 2 : (Pick < 0.4f ? 1 : 0);
		}
	}

	const FEffectDef& GetDef(EBraceletEffect Effect)
	{
		for (const FEffectDef& Def : Bracelet_Defs)
		{
			if (Def.Effect == Effect) return Def;
		}
		return Bracelet_Defs[0];
	}

	bool IsBaseEffect(EBraceletEffect Effect) { return Effect == B::PrimaryStat || Effect == B::Vitality; }
	bool IsCombatStat(EBraceletEffect Effect) { return Effect >= B::Crit && Effect <= B::Expertise; }

	void GetValueRange(EBraceletEffect Effect, int32& OutMin, int32& OutMax)
	{
		// 게임 데이터: value = 최대, deviation만큼 아래까지 (주 스탯 12,800 / 6,400, 체력 5,000 / 2,000, 전투 특성 100 / 59)
		if (Effect == B::PrimaryStat)   { OutMin = 6400; OutMax = 12800; }
		else if (Effect == B::Vitality) { OutMin = 3000; OutMax = 5000; }
		else if (IsCombatStat(Effect))  { OutMin = 41;   OutMax = 100; }
		else                            { OutMin = 0;    OutMax = 0; }
	}

	int32 GetGrade(const FBraceletEffectRoll& Roll)
	{
		if (!IsBaseEffect(Roll.Effect) && !IsCombatStat(Roll.Effect)) return FMath::Clamp(Roll.Grade, 0, 2);
		int32 Min = 0, Max = 0;
		GetValueRange(Roll.Effect, Min, Max);
		if (Max <= Min) return 0;
		const float Ratio = static_cast<float>(Roll.Value - Min) / static_cast<float>(Max - Min);
		return Ratio > 2.f / 3.f ? 2 : (Ratio > 1.f / 3.f ? 1 : 0);
	}

	FString Describe(const FBraceletEffectRoll& Roll, const FString& PrimaryStatName)
	{
		const FEffectDef& Def = GetDef(Roll.Effect);
		if (Roll.Effect == B::PrimaryStat) return FString::Printf(TEXT("%s +%s"), *PrimaryStatName, *LoAItems::FormatWithCommas(Roll.Value));
		if (IsBaseEffect(Roll.Effect) || IsCombatStat(Roll.Effect)) return FString::Printf(TEXT("%s +%s"), Def.Name, *LoAItems::FormatWithCommas(Roll.Value));

		const float V = Def.Values[FMath::Clamp(Roll.Grade, 0, 2)];
		switch (Roll.Effect)
		{
		case B::OnHitWeaponPower:
			return FString::Printf(TEXT("적중 시 10초간 무기 공격력 +%s, 공격·이동 속도 +1%% (최대 %d중첩)"),
				*LoAItems::FormatWithCommas(FMath::RoundToInt64(V)), Bracelet_OnHitStacks);
		case B::StaggeredDamage:
		{
			const float Staggered = Roll.Grade >= 2 ? 4.5f : (Roll.Grade == 1 ? 4.f : 3.5f);
			return FString::Printf(TEXT("적에게 주는 피해 +%.2f%%, 무력화 상태의 적에게 +%.2f%%"), V, Staggered);
		}
		case B::CCImmunity:
			return FString::Printf(TEXT("적중 시 %.0f초간 마비·밀려남 면역 (재사용 %.0f초)"), V, V);
		default:
			break;
		}
		if (Def.bPercent) return FString::Printf(TEXT("%s +%.2f%%"), Def.Name, V);
		return FString::Printf(TEXT("%s +%s"), Def.Name, *LoAItems::FormatWithCommas(FMath::RoundToInt64(V)));
	}

	void AppendModifiers(const FEquipmentInstance& Instance, TArray<FStatModifier>& OutMods)
	{
		auto Add = [&OutMods](EStatModifierTarget Target, double Value)
		{
			FStatModifier& Mod = OutMods.AddDefaulted_GetRef();
			Mod.Target = Target;
			Mod.Value = Value;
		};
		using T = EStatModifierTarget;
		for (const FBraceletEffectRoll& Roll : Instance.BraceletEffects)
		{
			const FEffectDef& Def = GetDef(Roll.Effect);
			if (!Def.bHasEffect) continue;
			const double V = Def.Values[FMath::Clamp(Roll.Grade, 0, 2)];
			switch (Roll.Effect)
			{
			case B::PrimaryStat:      Add(T::PrimaryStat, Roll.Value); break;
			case B::Vitality:         Add(T::Vitality, Roll.Value); break;
			case B::Crit:             Add(T::Crit, Roll.Value); break;
			case B::Specialization:   Add(T::Specialization, Roll.Value); break;
			case B::Domination:       Add(T::Domination, Roll.Value); break;
			case B::Swiftness:        Add(T::Swiftness, Roll.Value); break;
			case B::Endurance:        Add(T::Endurance, Roll.Value); break;
			case B::DamageToEnemies:  Add(T::MultiplicativeDamage, V / 100.0); break;
			case B::AdditionalDamage: Add(T::AdditiveDamage, V / 100.0); break;
			case B::CritRate:         Add(T::CritRate, V / 100.0); break;
			case B::CritDamage:       Add(T::CritDamage, V / 100.0); break;
			case B::WeaponPower:      Add(T::WeaponPower, V); break;
			case B::OnHitWeaponPower: Add(T::WeaponPower, V * Bracelet_OnHitStacks); Add(T::SpeedBonus, 0.01 * Bracelet_OnHitStacks); break;
			case B::StaggeredDamage:
				Add(T::MultiplicativeDamage, V / 100.0);
				Add(T::GroggyTargetDamage, (Roll.Grade >= 2 ? 4.5 : (Roll.Grade == 1 ? 4.0 : 3.5)) / 100.0);
				break;
			case B::NonDirectional:   Add(T::MultiplicativeDamage, V / 100.0); break;  // 소서리스 스킬은 전부 비방향성(각성기 없음)
			case B::PhysDefense:      Add(T::PhysDefense, V); break;
			case B::MagicDefense:     Add(T::MagicDefense, V); break;
			case B::MaxHP:            Add(T::MaxHPFlat, V); break;
			default: break;
			}
		}
	}

	ECategory GetCategory(EBraceletEffect Effect)
	{
		if (IsBaseEffect(Effect)) return ECategory::Base;
		if (IsCombatStat(Effect)) return ECategory::CombatStat;
		return ECategory::Special;
	}

	int32 GetCategoryLimit(ECategory Category)
	{
		return Category == ECategory::Special ? 4 : 2;  // 공식 가이드 — 기본 2 · 전투 특성 2 · 특수 4
	}

	TArray<EBraceletEffect> GetEffectsIn(ECategory Category)
	{
		if (Category == ECategory::Base) return { B::PrimaryStat, B::Vitality };
		if (Category == ECategory::CombatStat) return { B::Crit, B::Specialization, B::Domination, B::Swiftness, B::Endurance, B::Expertise };
		TArray<EBraceletEffect> Out;
		for (const FSpecialWeight& W : Bracelet_SpecialWeights) Out.Add(W.Effect);
		return Out;
	}

	FString DescribeRange(EBraceletEffect Effect, const FString& PrimaryStatName)
	{
		const FEffectDef& Def = GetDef(Effect);
		const FString Name = Effect == B::PrimaryStat ? PrimaryStatName : FString(Def.Name);
		if (IsBaseEffect(Effect) || IsCombatStat(Effect))
		{
			int32 Min = 0, Max = 0;
			GetValueRange(Effect, Min, Max);
			return FString::Printf(TEXT("%s [+%s~+%s]"), *Name, *LoAItems::FormatWithCommas(Min), *LoAItems::FormatWithCommas(Max));
		}
		if (Effect == B::CCImmunity) return FString::Printf(TEXT("%s [%.0f~%.0f초]"), *Name, Def.Values[0], Def.Values[2]);
		if (Effect == B::StaggeredDamage) return FString::Printf(TEXT("%s [+%.2f%%~+%.2f%%] + 무력화 대상 [+3.50%%~+4.50%%]"), *Name, Def.Values[0], Def.Values[2]);
		if (Def.bPercent) return FString::Printf(TEXT("%s [+%.2f%%~+%.2f%%]"), *Name, Def.Values[0], Def.Values[2]);
		return FString::Printf(TEXT("%s [+%s~+%s]"), *Name,
			*LoAItems::FormatWithCommas(FMath::RoundToInt64(Def.Values[0])), *LoAItems::FormatWithCommas(FMath::RoundToInt64(Def.Values[2])));
	}

	TArray<FBraceletEffectRoll> GetEnchants(const FEquipmentInstance& Instance)
	{
		return Instance.BraceletEffects.FilterByPredicate([](const FBraceletEffectRoll& R) { return !R.bFixed; });
	}

	int32 GetFixedCount(const FEquipmentInstance& Instance)
	{
		return Instance.BraceletEffects.FilterByPredicate([](const FBraceletEffectRoll& R) { return R.bFixed; }).Num();
	}

	namespace
	{
		/** 한 줄 굴림 — 분류 비중 35 : 35 : 30 중 상한·중복 때문에 못 들어가는 분류는 빼고, 그 안에서 종류(특수는 비중대로) + 수치/등급 */
		bool Bracelet_RollLine(const TArray<FBraceletEffectRoll>& Existing, FBraceletEffectRoll& Out)
		{
			auto Has = [&Existing](EBraceletEffect E)
			{
				return Existing.ContainsByPredicate([E](const FBraceletEffectRoll& R) { return R.Effect == E; });
			};
			struct FCat { ECategory Category; int32 Weight; };
			const FCat Cats[] = { { ECategory::Base, 35 }, { ECategory::CombatStat, 35 }, { ECategory::Special, 30 } };
			TArray<FCat> Allowed;
			int32 Total = 0;
			for (const FCat& C : Cats)
			{
				int32 Count = 0;
				for (const FBraceletEffectRoll& R : Existing) Count += GetCategory(R.Effect) == C.Category ? 1 : 0;
				if (Count >= GetCategoryLimit(C.Category)) continue;
				if (!GetEffectsIn(C.Category).ContainsByPredicate([&Has](EBraceletEffect E) { return !Has(E); })) continue;
				Allowed.Add(C);
				Total += C.Weight;
			}
			if (Total <= 0) return false;

			int32 Pick = FMath::RandRange(0, Total - 1);
			ECategory Category = Allowed.Last().Category;
			for (const FCat& C : Allowed)
			{
				if (Pick < C.Weight) { Category = C.Category; break; }
				Pick -= C.Weight;
			}

			if (Category == ECategory::Special)
			{
				TArray<FSpecialWeight> Pool;
				int32 PoolTotal = 0;
				for (const FSpecialWeight& W : Bracelet_SpecialWeights)
				{
					if (Has(W.Effect)) continue;
					Pool.Add(W);
					PoolTotal += W.Weight;
				}
				int32 P = FMath::RandRange(0, PoolTotal - 1);
				EBraceletEffect Effect = Pool.Last().Effect;
				for (const FSpecialWeight& W : Pool)
				{
					if (P < W.Weight) { Effect = W.Effect; break; }
					P -= W.Weight;
				}
				Out = Bracelet_Make(Effect, 0, Bracelet_RollGrade());
				return true;
			}

			TArray<EBraceletEffect> Pool = GetEffectsIn(Category).FilterByPredicate([&Has](EBraceletEffect E) { return !Has(E); });
			const EBraceletEffect Effect = Pool[FMath::RandRange(0, Pool.Num() - 1)];
			int32 Min = 0, Max = 0;
			GetValueRange(Effect, Min, Max);
			Out = Bracelet_Make(Effect, FMath::RandRange(Min, Max), 0);
			return true;
		}
	}

	void Roll(FEquipmentInstance& Instance)
	{
		Instance.BraceletEffects.Reset();
		for (int32 i = 0; i < RelicFixedCount; ++i)
		{
			FBraceletEffectRoll Line;
			if (!Bracelet_RollLine(Instance.BraceletEffects, Line)) break;
			Line.bFixed = true;
			Instance.BraceletEffects.Add(Line);
		}
		Instance.BraceletEnchantSlots = RelicEnchantSlots;
		Instance.BraceletEnchantsLeft = RelicEnchantChances;
	}

	namespace
	{
		/** 잠그지 않은 부여 칸을 새로 굴린 부여 효과 전체(칸 순서) — 상한·중복 판정 대상 = 고정 효과 + 잠근 부여 효과 */
		TArray<FBraceletEffectRoll> Bracelet_RollProposal(const FEquipmentInstance& Instance, const TArray<bool>& Locked)
		{
			const TArray<FBraceletEffectRoll> Current = GetEnchants(Instance);
			auto IsLocked = [&Locked, &Current](int32 i) { return Locked.IsValidIndex(i) && Locked[i] && Current.IsValidIndex(i); };
			TArray<FBraceletEffectRoll> Context = Instance.BraceletEffects.FilterByPredicate([](const FBraceletEffectRoll& R) { return R.bFixed; });
			for (int32 i = 0; i < Instance.BraceletEnchantSlots; ++i)
			{
				if (IsLocked(i)) Context.Add(Current[i]);
			}
			TArray<FBraceletEffectRoll> Out;
			for (int32 i = 0; i < Instance.BraceletEnchantSlots; ++i)
			{
				if (IsLocked(i))
				{
					Out.Add(Current[i]);
					continue;
				}
				FBraceletEffectRoll Line;
				if (Bracelet_RollLine(Context, Line))
				{
					Out.Add(Line);
					Context.Add(Line);
				}
			}
			return Out;
		}
	}

	EEnchantResult Enchant(FEquipmentInstance& Instance, const TArray<bool>& Locked, UInventoryComponent& Inventory, TArray<FBraceletEffectRoll>& OutProposal)
	{
		OutProposal.Reset();
		if (Instance.BraceletEnchantSlots <= 0) return EEnchantResult::Invalid;
		if (Instance.BraceletEnchantsLeft <= 0) return EEnchantResult::NoChances;

		const TArray<FBraceletEffectRoll> Current = GetEnchants(Instance);
		bool bAnyReroll = false;
		bool bFirst = true;  // 바꿀 칸이 전부 비어 있음 = 첫 부여 → 고르지 않고 바로 적용
		for (int32 i = 0; i < Instance.BraceletEnchantSlots; ++i)
		{
			if (Locked.IsValidIndex(i) && Locked[i] && Current.IsValidIndex(i)) continue;
			bAnyReroll = true;
			if (Current.IsValidIndex(i)) bFirst = false;
		}
		if (!bAnyReroll) return EEnchantResult::AllLocked;
		if (!Inventory.SpendSilling(EnchantSillingCost)) return EEnchantResult::NotEnoughSilling;
		--Instance.BraceletEnchantsLeft;

		OutProposal = Bracelet_RollProposal(Instance, Locked);
		if (bFirst)
		{
			ApplyChoice(Instance, OutProposal, true);
			return EEnchantResult::Applied;
		}
		return EEnchantResult::NeedChoice;
	}

	EEnchantResult Reroll(FEquipmentInstance& Instance, const TArray<bool>& Locked, UInventoryComponent& Inventory, TArray<FBraceletEffectRoll>& OutProposal)
	{
		if (Instance.BraceletEnchantSlots <= 0) return EEnchantResult::Invalid;
		if (Instance.BraceletEnchantsLeft <= 0) return EEnchantResult::NoChances;
		if (!Inventory.SpendSilling(RerollSillingCost)) return EEnchantResult::NotEnoughSilling;
		--Instance.BraceletEnchantsLeft;
		OutProposal = Bracelet_RollProposal(Instance, Locked);
		return EEnchantResult::NeedChoice;
	}

	void ApplyChoice(FEquipmentInstance& Instance, const TArray<FBraceletEffectRoll>& Proposal, bool bTakeNew)
	{
		if (!bTakeNew) return;
		TArray<FBraceletEffectRoll> Effects = Instance.BraceletEffects.FilterByPredicate([](const FBraceletEffectRoll& R) { return R.bFixed; });
		for (FBraceletEffectRoll Line : Proposal)
		{
			Line.bFixed = false;
			Effects.Add(Line);
		}
		Instance.BraceletEffects = MoveTemp(Effects);
	}

	FText GetEnchantResultText(EEnchantResult Result)
	{
		switch (Result)
		{
		case EEnchantResult::NoChances:     return NSLOCTEXT("Bracelet", "NoChances", "남은 부여 횟수가 없습니다.");
		case EEnchantResult::NotEnoughSilling: return NSLOCTEXT("Bracelet", "NotEnoughSilling", "실링이 부족합니다.");
		case EEnchantResult::AllLocked:     return NSLOCTEXT("Bracelet", "AllLocked", "모든 부여 효과가 잠겨 있습니다.");
		case EEnchantResult::Invalid:       return NSLOCTEXT("Bracelet", "Invalid", "효과를 부여할 수 없는 팔찌입니다.");
		default:                            return FText::GetEmpty();
		}
	}

	void ApplyStarterPreset(FEquipmentInstance& Instance)
	{
		FBraceletEffectRoll Crit = Bracelet_Make(B::Crit, 80, 0);           // 중 (61~80)
		FBraceletEffectRoll Spec = Bracelet_Make(B::Specialization, 80, 0);
		Crit.bFixed = true;
		Spec.bFixed = true;
		Instance.BraceletEffects = {
			Crit, Spec,
			Bracelet_Make(B::PrimaryStat, 9600, 0),   // 중
			Bracelet_Make(B::CritRate, 0, 1),         // 3.40%
			Bracelet_Make(B::CritDamage, 0, 1),       // 6.80%
		};
		Instance.BraceletEnchantSlots = RelicEnchantSlots;
		Instance.BraceletEnchantsLeft = RelicEnchantChances - 1;  // 한 번 부여한 상태
	}
}

// ═══ 어빌리티 스톤 ═══════════════════════════════════════════════════════════

namespace LoAStone
{
	const FName AtkReduction(TEXT("AtkReduction"));
	const FName DefReduction(TEXT("DefReduction"));
	const FName AtkSpeedReduction(TEXT("AtkSpeedReduction"));
	const FName MoveSpeedReduction(TEXT("MoveSpeedReduction"));

	namespace
	{
		struct FNegativeDef { FName Id; const TCHAR* Name; float Values[3]; };
		const FNegativeDef& Stone_Negative(FName Id)
		{
			static const FNegativeDef Defs[] = {
				{ AtkReduction,       TEXT("공격력 감소"),     { 2.f, 4.f, 6.f } },
				{ DefReduction,       TEXT("방어력 감소"),     { 5.f, 10.f, 15.f } },
				{ AtkSpeedReduction,  TEXT("공격 속도 감소"),  { 2.f, 4.f, 6.f } },
				{ MoveSpeedReduction, TEXT("이동 속도 감소"),  { 2.f, 4.f, 6.f } },
			};
			for (const FNegativeDef& D : Defs)
			{
				if (D.Id == Id) return D;
			}
			return Defs[0];
		}

		const FName Stone_NegativeIds[] = { AtkReduction, DefReduction, AtkSpeedReduction, MoveSpeedReduction };

		FStoneLine Stone_MakeDone(FName Id, int32 Successes)
		{
			// 성공을 10칸에 고르게 흩어 둔다 — 표시용 (i번째 칸까지의 성공 수 = floor((i+1) × S / 10))
			FStoneLine Line;
			Line.Id = Id;
			Line.Successes = Successes;
			Line.Attempts = FacetsPerLine;
			for (int32 i = 0; i < FacetsPerLine; ++i)
			{
				Line.Results.Add((i + 1) * Successes / FacetsPerLine > i * Successes / FacetsPerLine);
			}
			return Line;
		}
	}

	bool IsNegativeLine(int32 LineIndex) { return LineIndex == 2; }

	int32 GetLevel(int32 Successes, bool bNegative)
	{
		if (bNegative) return Successes >= 10 ? 3 : (Successes >= 7 ? 2 : (Successes >= 5 ? 1 : 0));
		return Successes >= 10 ? 4 : (Successes >= 9 ? 3 : (Successes >= 7 ? 2 : (Successes >= 6 ? 1 : 0)));
	}

	TArray<int32> GetLevelThresholds(bool bNegative)
	{
		return bNegative ? TArray<int32>{ 5, 7, 10 } : TArray<int32>{ 6, 7, 9, 10 };
	}

	FString GetLineName(const FStoneLine& Line, bool bNegative)
	{
		if (bNegative) return Stone_Negative(Line.Id).Name;
		const FEngravingDefinition* Def = LoAEngravings::Find(Line.Id);
		return Def ? Def->Name.ToString() : Line.Id.ToString();
	}

	FString DescribeLine(const FStoneLine& Line, int32 LineIndex)
	{
		const bool bNegative = IsNegativeLine(LineIndex);
		const int32 Level = GetLevel(Line.Successes, bNegative);
		return FString::Printf(TEXT("%s Lv.%d (%d)"), *GetLineName(Line, bNegative), Level, Line.Successes);
	}

	FString DescribeNegativeEffect(FName Id, int32 Level)
	{
		if (Level <= 0) return TEXT("비활성");
		const FNegativeDef& D = Stone_Negative(Id);
		const FString Short = FString(D.Name).LeftChop(3);  // "공격력 감소" → "공격력"
		return FString::Printf(TEXT("%s -%.0f%%"), *Short, D.Values[FMath::Clamp(Level, 1, 3) - 1]);
	}

	int32 GetTotalSuccesses(const FEquipmentInstance& Stone)
	{
		int32 Sum = 0;
		for (int32 i = 0; i < Stone.StoneLines.Num() && i < 2; ++i) Sum += Stone.StoneLines[i].Successes;
		return Sum;
	}

	int32 GetTotalLevel(const FEquipmentInstance& Stone)
	{
		int32 Sum = 0;
		for (int32 i = 0; i < Stone.StoneLines.Num() && i < 2; ++i) Sum += GetLevel(Stone.StoneLines[i].Successes, false);
		return Sum;
	}

	int32 GetVitalityBonus(int32 TotalSuccesses)
	{
		return TotalSuccesses >= VitalityFirstSuccesses ? (TotalSuccesses - VitalityFirstSuccesses + 1) * VitalityPerSuccess : 0;
	}

	int32 GetEngravingLevel(const FEquipmentInstance& Stone, FName EngravingId)
	{
		for (int32 i = 0; i < Stone.StoneLines.Num() && i < 2; ++i)
		{
			if (Stone.StoneLines[i].Id == EngravingId) return GetLevel(Stone.StoneLines[i].Successes, false);
		}
		return 0;
	}

	void AppendModifiers(const FEquipmentInstance& Stone, TArray<FStatModifier>& OutMods)
	{
		auto Add = [&OutMods](EStatModifierTarget Target, double Value)
		{
			FStatModifier& Mod = OutMods.AddDefaulted_GetRef();
			Mod.Target = Target;
			Mod.Value = Value;
		};
		// 세공 보너스 — 체력(성공 합), 기본 공격력(레벨 합 5 이상)
		if (const int32 Vitality = GetVitalityBonus(GetTotalSuccesses(Stone))) Add(EStatModifierTarget::Vitality, Vitality);
		if (GetTotalLevel(Stone) >= AttackBonusLevel) Add(EStatModifierTarget::AttackPowerPercent, AttackBonusPercent);

		if (!Stone.StoneLines.IsValidIndex(2)) return;
		const FStoneLine& Line = Stone.StoneLines[2];
		const int32 Level = GetLevel(Line.Successes, true);
		if (Level <= 0) return;
		const double V = Stone_Negative(Line.Id).Values[Level - 1] / 100.0;
		if (Line.Id == AtkReduction)           Add(EStatModifierTarget::AttackPowerPercent, -V);
		else if (Line.Id == DefReduction)      Add(EStatModifierTarget::DefensePercent, -V);
		else if (Line.Id == AtkSpeedReduction) Add(EStatModifierTarget::AttackSpeedBonus, -V);
		else                                   Add(EStatModifierTarget::SpeedBonus, -V);  // 이동 속도만 줄이는 대상이 없어 공격·이동 속도로
	}

	bool IsComplete(const FEquipmentInstance& Stone)
	{
		return Stone.StoneLines.Num() == LineCount && GetRemainingFacets(Stone) == 0;
	}

	bool IsStarted(const FEquipmentInstance& Stone)
	{
		return Stone.StoneLines.ContainsByPredicate([](const FStoneLine& Line) { return Line.Attempts > 0; });
	}

	bool CanFacet(const FEquipmentInstance& Stone, int32 LineIndex)
	{
		return Stone.StoneLines.IsValidIndex(LineIndex) && Stone.StoneLines[LineIndex].Attempts < FacetsPerLine;
	}

	int32 GetRemainingFacets(const FEquipmentInstance& Stone)
	{
		int32 Left = 0;
		for (const FStoneLine& Line : Stone.StoneLines) Left += FMath::Max(0, FacetsPerLine - Line.Attempts);
		return Left;
	}

	namespace
	{
		bool Stone_Roll(FEquipmentInstance& Stone, int32 LineIndex)
		{
			FStoneLine& Line = Stone.StoneLines[LineIndex];
			const bool bSuccess = FMath::RandRange(1, 100) <= Stone.StoneChance;
			++Line.Attempts;
			Line.Results.Add(bSuccess);
			if (bSuccess) ++Line.Successes;
			Stone.StoneChance = FMath::Clamp(Stone.StoneChance + (bSuccess ? -10 : 10), 25, 75);
			return bSuccess;
		}

		/**
		 * 자동 세공 정책 — 상태(증가 두 줄의 시도·성공, 감소 줄 시도, 확률)마다 결과 레벨 합의 기대 가중치(합 5 ≫ 4 ≫ 3 …)를 동적 계획법으로 구해 두고
		 * 매 칸 그 값이 가장 높은 줄을 고른다. 시뮬레이션(2만 개): 합 5 약 0.1% · 4 이상 약 5% · 3 이상 약 26% — T4 스톤 확률 구조상 원래 드묾. 확률이 같으면 성공률이 낮을 때(≤45%) 감소 줄, 높을 때 증가 줄 — 감소 줄이 덜 터지게.
		 * 상태 11^5 × 6 ≈ 97만 개(약 8MB), 처음 쓸 때 한 번만 채운다
		 */
		struct FStoneValue { float Prob = 0.f; bool bDone = false; };

		int32 Stone_ChanceIndex(int32 Chance) { return FMath::Clamp((Chance - 25) / 10, 0, 5); }

		TArray<FStoneValue>& Stone_Memo()
		{
			static TArray<FStoneValue> Memo;
			if (Memo.Num() == 0) Memo.SetNum(11 * 11 * 11 * 11 * 11 * 6);
			return Memo;
		}

		float Stone_Solve(int32 Aa, int32 As, int32 Ba, int32 Bs, int32 Ca, int32 P);

		// 상태에서 Line 줄을 세공했을 때의 목표 달성 확률 (그 줄을 못 깎으면 -1)
		float Stone_LineValue(int32 Aa, int32 As, int32 Ba, int32 Bs, int32 Ca, int32 P, int32 Line)
		{
			const float Chance = (25 + P * 10) / 100.f;
			const int32 PUp = FMath::Max(P - 1, 0);   // 성공하면 확률 -10%
			const int32 PDown = FMath::Min(P + 1, 5); // 실패하면 +10%
			if (Line == 0 && Aa < FacetsPerLine) return Chance * Stone_Solve(Aa + 1, As + 1, Ba, Bs, Ca, PUp) + (1.f - Chance) * Stone_Solve(Aa + 1, As, Ba, Bs, Ca, PDown);
			if (Line == 1 && Ba < FacetsPerLine) return Chance * Stone_Solve(Aa, As, Ba + 1, Bs + 1, Ca, PUp) + (1.f - Chance) * Stone_Solve(Aa, As, Ba + 1, Bs, Ca, PDown);
			if (Line == 2 && Ca < FacetsPerLine) return Chance * Stone_Solve(Aa, As, Ba, Bs, Ca + 1, PUp) + (1.f - Chance) * Stone_Solve(Aa, As, Ba, Bs, Ca + 1, PDown);
			return -1.f;
		}

		// 가장 좋은 줄 — 확률이 같으면 성공률 45% 이하면 감소 줄(2) 우선, 아니면 증가 줄 우선
		int32 Stone_PickLine(int32 Aa, int32 As, int32 Ba, int32 Bs, int32 Ca, int32 P, float& OutValue)
		{
			const int32 Order[3] = { P <= 2 ? 2 : 0, P <= 2 ? 0 : 1, P <= 2 ? 1 : 2 };
			int32 Best = INDEX_NONE;
			OutValue = -1.f;
			for (const int32 Line : Order)
			{
				const float V = Stone_LineValue(Aa, As, Ba, Bs, Ca, P, Line);
				if (V > OutValue + 1e-6f * FMath::Max(1.f, FMath::Abs(OutValue)))
				{
					OutValue = V;
					Best = Line;
				}
			}
			return Best;
		}

		float Stone_Solve(int32 Aa, int32 As, int32 Ba, int32 Bs, int32 Ca, int32 P)
		{
			FStoneValue& Slot = Stone_Memo()[((((Aa * 11 + As) * 11 + Ba) * 11 + Bs) * 11 + Ca) * 6 + P];
			if (Slot.bDone) return Slot.Prob;
			float Value = 0.f;
			if (Aa == FacetsPerLine && Ba == FacetsPerLine && Ca == FacetsPerLine)
			{
				// 목표(합 5)가 가장 중요하고, 못 미치면 4 > 3 > … 순으로 — 단계마다 10배 이상 가중치라 사실상 차례대로 극대화
				static constexpr float Weights[AutoTargetLevel + 1] = { 0.f, 1.f, 10.f, 100.f, 1000.f, 100000.f };
				Value = Weights[FMath::Min(GetLevel(As, false) + GetLevel(Bs, false), AutoTargetLevel)];
			}
			else
			{
				Stone_PickLine(Aa, As, Ba, Bs, Ca, P, Value);
			}
			Slot.Prob = Value;
			Slot.bDone = true;
			return Value;
		}

		int32 Stone_BestLine(const FEquipmentInstance& Stone)
		{
			const FStoneLine& A = Stone.StoneLines[0];
			const FStoneLine& Bl = Stone.StoneLines[1];
			float Value = 0.f;
			return Stone_PickLine(A.Attempts, A.Successes, Bl.Attempts, Bl.Successes, Stone.StoneLines[2].Attempts, Stone_ChanceIndex(Stone.StoneChance), Value);
		}
	}

	EFacetResult Facet(FEquipmentInstance& Stone, int32 LineIndex, UInventoryComponent& Inventory)
	{
		if (!Stone.StoneLines.IsValidIndex(LineIndex)) return EFacetResult::Invalid;
		if (!CanFacet(Stone, LineIndex)) return EFacetResult::Full;
		if (!Inventory.SpendSilling(FacetSillingCost)) return EFacetResult::NotEnoughSilling;
		return Stone_Roll(Stone, LineIndex) ? EFacetResult::Success : EFacetResult::Fail;
	}

	EFacetResult AutoFacet(FEquipmentInstance& Stone, UInventoryComponent& Inventory)
	{
		if (Stone.StoneLines.Num() != LineCount) return EFacetResult::Invalid;
		if (IsStarted(Stone)) return EFacetResult::AlreadyStarted;
		const int32 Facets = GetRemainingFacets(Stone);
		if (!Inventory.SpendSilling(FacetSillingCost * Facets)) return EFacetResult::NotEnoughSilling;
		for (int32 i = 0; i < Facets; ++i)
		{
			const int32 Line = Stone_BestLine(Stone);
			if (Line == INDEX_NONE) break;
			Stone_Roll(Stone, Line);
		}
		return GetTotalLevel(Stone) >= AutoTargetLevel ? EFacetResult::Success : EFacetResult::Fail;
	}

	FText GetFacetResultText(EFacetResult Result)
	{
		switch (Result)
		{
		case EFacetResult::NotEnoughSilling: return NSLOCTEXT("Stone", "NotEnoughSilling", "실링이 부족합니다.");
		case EFacetResult::AlreadyStarted: return NSLOCTEXT("Stone", "AlreadyStarted", "이미 세공이 진행 중인 스톤은 자동 세공을 할 수 없습니다.");
		case EFacetResult::Full:           return NSLOCTEXT("Stone", "Full", "더 이상 세공할 수 없습니다.");
		case EFacetResult::Invalid:        return NSLOCTEXT("Stone", "Invalid", "세공할 수 없는 스톤입니다.");
		default:                           return FText::GetEmpty();
		}
	}

	void Roll(FEquipmentInstance& Stone)
	{
		Stone.StoneLines.Reset();
		Stone.StoneChance = 75;
		TArray<FName> Pool;
		for (const FEngravingDefinition& Def : LoAEngravings::GetAll()) Pool.Add(Def.Id);
		for (int32 i = 0; i < 2 && Pool.Num() > 0; ++i)
		{
			const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
			FStoneLine Line;
			Line.Id = Pool[Pick];
			Pool.RemoveAt(Pick);
			Stone.StoneLines.Add(Line);
		}
		FStoneLine Negative;
		Negative.Id = Stone_NegativeIds[FMath::RandRange(0, UE_ARRAY_COUNT(Stone_NegativeIds) - 1)];
		Stone.StoneLines.Add(Negative);
	}

	void ApplyStarterPreset(FEquipmentInstance& Stone)
	{
		Stone.StoneLines = {
			Stone_MakeDone(TEXT("CursedDoll"), 7),
			Stone_MakeDone(LoAEngravings::Adrenaline, 7),
			Stone_MakeDone(AtkReduction, 4),
		};
		Stone.StoneChance = 25;
	}
}

// ═══ 팔찌·스톤 NPC 창 공용 ═══════════════════════════════════════════════════

namespace LoAGearList
{
	TArray<FGuid> CollectWorkable(const UInventoryComponent* Inventory, EEquipSlot Slot)
	{
		TArray<FGuid> Out;
		if (!Inventory) return Out;
		for (int32 i = 0; i < UInventoryComponent::Capacity; ++i)
		{
			if (!Inventory->IsValidIndex(i)) break;
			const FInventorySlot& InvSlot = Inventory->GetSlot(i);
			const FEquipmentInstance* Instance = InvSlot.IsEquipment() ? Inventory->FindEquipment(InvSlot.EquipmentId) : nullptr;
			const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
			if (!Def || Def->EquipSlot != Slot) continue;
			const bool bWorkable = Slot == EEquipSlot::AbilityStone ? !LoAStone::IsComplete(*Instance)
				: Slot == EEquipSlot::Bracelet ? Instance->BraceletEnchantsLeft > 0 && Instance->BraceletEnchantSlots > 0
				: true;
			if (bWorkable) Out.AddUnique(InvSlot.EquipmentId);
		}
		return Out;
	}

	void CommitChange(UInventoryComponent* Inventory, UEquipmentComponent* Equipment, const FGuid& Id)
	{
		if (Inventory) Inventory->NotifyEquipmentChanged(Id);
		if (Equipment)
		{
			bool bWorn = false;
			for (int32 i = 0; i < LoAEquipRules::SlotCount; ++i)
			{
				const FEquipmentInstance* Worn = Equipment->GetEquipped(static_cast<EEquipSlot>(i));
				if (Worn && Worn->InstanceId == Id) { bWorn = true; break; }
			}
			if (bWorn) Equipment->RefreshEquipped();
			Equipment->SaveNow();
		}
	}
}
