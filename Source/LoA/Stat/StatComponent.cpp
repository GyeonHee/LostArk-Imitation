#include "Stat/StatComponent.h"
#include "Data/CharacterDataAsset.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	ELoACombatStat Stat_ToCombatStat(EStatModifierTarget Target)
	{
		switch (Target)
		{
		case EStatModifierTarget::Crit:           return ELoACombatStat::Crit;
		case EStatModifierTarget::Specialization: return ELoACombatStat::Specialization;
		case EStatModifierTarget::Swiftness:      return ELoACombatStat::Swiftness;
		case EStatModifierTarget::Domination:     return ELoACombatStat::Domination;
		case EStatModifierTarget::Endurance:      return ELoACombatStat::Endurance;
		case EStatModifierTarget::Expertise:      return ELoACombatStat::Expertise;
		default:                                  return ELoACombatStat::MAX;
		}
	}
}

UStatComponent::UStatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const UStatComponent* UStatComponent::Find(const AActor* Actor)
{
	if (const AController* Controller = Cast<AController>(Actor))
	{
		Actor = Controller->GetPawn();
	}
	return Actor ? Actor->FindComponentByClass<UStatComponent>() : nullptr;
}

float UStatComponent::GetAttackSpeedMultiplierOf(const AActor* Actor)
{
	const UStatComponent* Comp = Find(Actor);
	return Comp ? FMath::Max(0.01f, Comp->GetStats().AttackSpeedMultiplier) : 1.f;
}

float UStatComponent::GetCooldownReductionOf(const AActor* Actor)
{
	const UStatComponent* Comp = Find(Actor);
	return Comp ? Comp->GetStats().CooldownReduction : 0.f;
}

float UStatComponent::GetCastSpeedMultiplierOf(const AActor* Actor)
{
	const UStatComponent* Comp = Find(Actor);
	return Comp ? FMath::Max(0.01f, Comp->GetStats().AttackSpeedMultiplier * Comp->GetStats().CastSpeedMultiplier) : 1.f;
}

float UStatComponent::GetChargeSpeedMultiplierOf(const AActor* Actor)
{
	const UStatComponent* Comp = Find(Actor);
	return Comp ? FMath::Max(0.01f, Comp->GetStats().AttackSpeedMultiplier * Comp->GetStats().ChargeSpeedMultiplier) : 1.f;
}

void UStatComponent::SetSkillGemBonuses(const TMap<FName, float>& DamageBonus, const TMap<FName, float>& CooldownReduction)
{
	SkillGemDamage = DamageBonus;
	SkillGemCooldown = CooldownReduction;
	OnStatsChanged.Broadcast(Stats);  // 수치는 그대로지만 창(P·보석)이 다시 그리게
}

float UStatComponent::GetSkillDamageMultiplier(FName SkillRowName) const
{
	const float* Found = SkillGemDamage.Find(SkillRowName);
	return 1.f + (Found ? *Found : 0.f);
}

float UStatComponent::GetSkillCooldownReduction(FName SkillRowName) const
{
	const float* Found = SkillGemCooldown.Find(SkillRowName);
	return Found ? *Found : 0.f;
}

float UStatComponent::GetTotalCooldownReductionOf(const AActor* Actor, FName SkillRowName)
{
	const UStatComponent* Comp = Find(Actor);
	if (!Comp) return 0.f;
	const float Total = 1.f - (1.f - Comp->GetStats().CooldownReduction) * (1.f - Comp->GetSkillCooldownReduction(SkillRowName));
	return FMath::Clamp(Total, 0.f, LoAStatRules::MaxCooldownReduction);
}

void UStatComponent::InitializeFromData(const UCharacterDataAsset* Data)
{
	// DA가 없으면 계산하지 않는다 — 캐릭터는 생성자 기본값(HP·공격력)을 그대로 쓴다
	if (!Data)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stat] %s: CharacterData 없음 — 스탯 계산 건너뜀"), *GetNameSafe(GetOwner()));
		return;
	}

	PrimaryStatType  = Data->PrimaryStatType;
	BasePrimaryStat  = Data->BasePrimaryStat;
	BaseWeaponPower  = Data->BaseWeaponPower;
	BaseVitality     = Data->BaseVitality;
	HPPerVitality    = Data->HPPerVitality;
	DefenseCoefficient = Data->DefenseCoefficient;
	AttackPowerScale = Data->AttackPowerScale;
	BaseCritRate     = Data->BaseCritRate;
	BaseCombatStats  = Data->BaseCombatStats;

	bInitialized = true;
	RecalculateStats();
}

void UStatComponent::SetModifiers(FName Source, const TArray<FStatModifier>& Modifiers)
{
	if (Modifiers.Num() == 0)
	{
		ModifiersBySource.Remove(Source);
	}
	else
	{
		ModifiersBySource.Add(Source, Modifiers);
	}
	RecalculateStats();
}

void UStatComponent::ClearModifiers(FName Source)
{
	if (ModifiersBySource.Remove(Source) > 0)
	{
		RecalculateStats();
	}
}

TArray<FStatModifier> UStatComponent::GetModifiers(FName Source) const
{
	const TArray<FStatModifier>* Found = ModifiersBySource.Find(Source);
	return Found ? *Found : TArray<FStatModifier>();
}

void UStatComponent::RecalculateStats()
{
	if (!bInitialized) return;

	FCombatStats New;
	New.PrimaryStat  = BasePrimaryStat;
	New.WeaponPower  = BaseWeaponPower;
	New.Vitality     = BaseVitality;
	New.CombatPoints = BaseCombatStats;

	float ExtraCritRate = 0.f;
	float ExtraCritDamage = 0.f;
	double AttackPowerPercent = 0.0;
	float SpeedBonus = 0.f;
	float ExtraCooldownKeep = 1.f;   // (1 - 각인 쿨감)들의 곱
	float MoveSpeedToDamage = 0.f;
	double DefensePercent = 0.0;
	float AttackSpeedBonus = 0.f;
	double GearPhysDefense = 0.0;
	double GearMagicDefense = 0.0;

	for (const TPair<FName, TArray<FStatModifier>>& Pair : ModifiersBySource)
	{
		for (const FStatModifier& Mod : Pair.Value)
		{
			switch (Mod.Target)
			{
			case EStatModifierTarget::PrimaryStat:          New.PrimaryStat += Mod.Value; break;
			case EStatModifierTarget::WeaponPower:          New.WeaponPower += Mod.Value; break;
			case EStatModifierTarget::Vitality:             New.Vitality += Mod.Value; break;
			case EStatModifierTarget::CritRate:             ExtraCritRate += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::CritDamage:           ExtraCritDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::AdditiveDamage:       New.AdditiveDamageBonus += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::MultiplicativeDamage: New.MultiplicativeDamage *= 1.f + static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::PhysDefense:          GearPhysDefense += Mod.Value; break;
			case EStatModifierTarget::MagicDefense:         GearMagicDefense += Mod.Value; break;
			case EStatModifierTarget::AttackPowerPercent:   AttackPowerPercent += Mod.Value; break;
			case EStatModifierTarget::SpeedBonus:           SpeedBonus += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::CooldownReduction:    ExtraCooldownKeep *= 1.f - static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::IncomingDamage:       New.IncomingDamageMultiplier += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::HealingReceived:      New.HealingMultiplier += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::GetUpCooldown:        New.GetUpCooldownReduction += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::MPRegen:              New.MPRegenMultiplier += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::CastSkillDamage:      New.CastSkillDamageMultiplier *= 1.f + static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::CastSpeed:            New.CastSpeedMultiplier += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::MoveSpeedToDamage:    MoveSpeedToDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::DamagePenaltyChance:  New.DamagePenaltyChance += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::LowTargetHPDamage:    New.LowTargetHPDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::LowTargetHPThreshold: New.LowTargetHPThreshold = FMath::Max(New.LowTargetHPThreshold, static_cast<float>(Mod.Value)); break;
			case EStatModifierTarget::HighSelfHPDamage:     New.HighSelfHPDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::LowSelfHPDamage:      New.LowSelfHPDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::ChargeSkillDamage:    New.ChargeSkillDamageMultiplier *= 1.f + static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::ChargeSpeed:          New.ChargeSpeedMultiplier += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::PushedDamageReduction: New.PushedDamageReduction += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::CrisisEvasionCooldown: New.CrisisEvasionCooldown = static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::MaxMPBonus:           New.MaxMPMultiplier += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::ManaSkillDamage:      New.ManaSkillDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::FortitudeMax:         New.FortitudeMax += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::DivineChance:         New.DivineChance += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::DefensePercent:       DefensePercent += Mod.Value; break;
			case EStatModifierTarget::GroggyTargetDamage:   New.GroggyTargetDamage += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::AttackSpeedBonus:     AttackSpeedBonus += static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::EmergencyRescueCooldown: New.EmergencyRescueCooldown = static_cast<float>(Mod.Value); break;
			case EStatModifierTarget::SkillDamage:          New.SkillDamageMultiplier *= 1.f + static_cast<float>(Mod.Value); break;
			default:
				New.CombatPoints.Add(Stat_ToCombatStat(Mod.Target), FMath::RoundToInt32(Mod.Value));
				break;
			}
		}
	}

	// 공격력 — 로아 공식. 정수로 반올림해 K의 소수점 오차가 데미지 숫자에 새지 않게 한다
	New.DisplayAttackPower = FMath::Sqrt(FMath::Max(0.0, New.PrimaryStat * New.WeaponPower / 6.0));
	// 로아 "공격력 %"(보석 기본 공격력·아드레날린) — 서로 더해서 공격력에 곱한다
	New.AttackPowerPercent = static_cast<float>(AttackPowerPercent);
	New.AttackPower = FMath::RoundToDouble(New.DisplayAttackPower * AttackPowerScale * (1.0 + AttackPowerPercent));

	// 최대 생명력 — 로아: 체력 × 직업 생명 활성력 계수. 맨몸 체력(BaseVitality)분이 캐릭터 기본 HP
	New.BaseHP = static_cast<float>(BaseVitality * HPPerVitality);
	New.EquipmentHP = static_cast<float>((New.Vitality - BaseVitality) * HPPerVitality);
	New.MaxHP = FMath::Max(1.f, static_cast<float>(New.Vitality * HPPerVitality));

	const FCombatStatPoints& P = New.CombatPoints;

	// 방어력 — 로아: 장비 방어력 × 직업 방어 계수 × (1 + 인내). 받는 피해 감소 = 방어력 / (방어력 + 6500)
	// 중갑 착용·에테르 포식자의 방어력 %는 인내와 합연산
	const double DefenseScale = DefenseCoefficient * (1.0 + P.Endurance * LoAStatRules::DefensePerEndurance + DefensePercent);
	New.PhysDefense = FMath::RoundToDouble(GearPhysDefense * DefenseScale);
	New.MagicDefense = FMath::RoundToDouble(GearMagicDefense * DefenseScale);
	New.PhysDamageReduction = static_cast<float>(New.PhysDefense / (New.PhysDefense + LoAStatRules::DefenseConstant));
	New.MagicDamageReduction = static_cast<float>(New.MagicDefense / (New.MagicDefense + LoAStatRules::DefenseConstant));
	New.CritRate                 = FMath::Clamp(BaseCritRate + P.Crit * LoAStatRules::CritRatePerPoint + ExtraCritRate, 0.f, 1.f);
	New.CritDamage               = LoAStatRules::BaseCritDamage + ExtraCritDamage;
	New.SpecializationMultiplier = 1.f + P.Specialization * LoAStatRules::SpecializationPerPoint;
	// 쿨감 — 로아처럼 곱연산: (1 - 신속) × (1 - 각인). 스킬별 보석(작열)은 StartCooldown에서 한 번 더 곱하고 합계 80%로 자름
	const float SwiftCooldown = P.Swiftness * LoAStatRules::CooldownPerSwiftness;
	New.CooldownReduction        = FMath::Clamp(1.f - (1.f - SwiftCooldown) * ExtraCooldownKeep, 0.f, LoAStatRules::MaxCooldownReduction);
	New.MoveSpeedMultiplier      = 1.f + FMath::Clamp(P.Swiftness * LoAStatRules::SpeedPerSwiftness + SpeedBonus, 0.f, LoAStatRules::MaxSpeedBonus);
	// 공격 속도 = 이동속도 몫 + 공격 속도만 바꾸는 효과(질량 증가 -10%) — 상한은 같은 40%
	New.AttackSpeedMultiplier    = FMath::Max(0.1f, 1.f + FMath::Clamp(P.Swiftness * LoAStatRules::SpeedPerSwiftness + SpeedBonus, 0.f, LoAStatRules::MaxSpeedBonus) + AttackSpeedBonus);
	New.DominationMultiplier     = 1.f + P.Domination * LoAStatRules::DominationPerPoint;
	// 돌격대장 — 이동속도 증가량(%)의 N%만큼 피해, 별도 곱연산
	if (MoveSpeedToDamage > 0.f)
	{
		New.MultiplicativeDamage *= 1.f + MoveSpeedToDamage * (New.MoveSpeedMultiplier - 1.f);
	}
	New.IncomingDamageMultiplier = FMath::Max(0.f, New.IncomingDamageMultiplier);
	New.HealingMultiplier        = FMath::Max(0.f, New.HealingMultiplier);
	New.GetUpCooldownReduction   = FMath::Clamp(New.GetUpCooldownReduction, 0.f, 0.9f);
	New.DamagePenaltyChance      = FMath::Clamp(New.DamagePenaltyChance, 0.f, 1.f);
	// 숙련은 효과 없음 — 포인트만 들고 있다

	Stats = New;
	OnStatsChanged.Broadcast(Stats);
}

TArray<FString> UStatComponent::DescribeStats() const
{
	const UEnum* PrimaryEnum = StaticEnum<EPrimaryStat>();
	const FCombatStatPoints& P = Stats.CombatPoints;

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("%s %.0f / 무기공격력 %.0f / 체력 %.0f → 공격력 %.0f (표시 %.0f, K %.4f) / 최대 HP %.0f"),
		*PrimaryEnum->GetDisplayNameTextByValue(static_cast<int64>(PrimaryStatType)).ToString(),
		Stats.PrimaryStat, Stats.WeaponPower, Stats.Vitality,
		Stats.AttackPower, Stats.DisplayAttackPower, AttackPowerScale, Stats.MaxHP));
	Lines.Add(FString::Printf(TEXT("최대 생명력 %.0f (기본 %.0f + 장비 %.0f) / 물리 방어력 %.0f (피해 -%.2f%%) / 마법 방어력 %.0f (피해 -%.2f%%)"),
		Stats.MaxHP, Stats.BaseHP, Stats.EquipmentHP, Stats.PhysDefense, Stats.PhysDamageReduction * 100.f, Stats.MagicDefense, Stats.MagicDamageReduction * 100.f));
	Lines.Add(FString::Printf(TEXT("치명 %d / 특화 %d / 신속 %d / 제압 %d / 인내 %d / 숙련 %d"),
		P.Crit, P.Specialization, P.Swiftness, P.Domination, P.Endurance, P.Expertise));
	Lines.Add(FString::Printf(TEXT("치명률 %.2f%% / 치명피해 %.0f%% / 특화 x%.4f / 쿨감 %.2f%% / 속도 x%.4f / 제압 x%.4f / 피해 합 +%.2f%% 곱 x%.4f"),
		Stats.CritRate * 100.f, Stats.CritDamage * 100.f, Stats.SpecializationMultiplier,
		Stats.CooldownReduction * 100.f, Stats.MoveSpeedMultiplier, Stats.DominationMultiplier,
		Stats.AdditiveDamageBonus * 100.f, Stats.MultiplicativeDamage));
	for (const TPair<FName, TArray<FStatModifier>>& Pair : ModifiersBySource)
	{
		Lines.Add(FString::Printf(TEXT("  수정치 출처 '%s' — %d개"), *Pair.Key.ToString(), Pair.Value.Num()));
	}
	return Lines;
}
