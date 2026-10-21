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
	BaseMaxHP        = Data->MaxHP;
	HPPerVitality    = Data->HPPerVitality;
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
			default:
				New.CombatPoints.Add(Stat_ToCombatStat(Mod.Target), FMath::RoundToInt32(Mod.Value));
				break;
			}
		}
	}

	// 공격력 — 로아 공식. 정수로 반올림해 K의 소수점 오차가 데미지 숫자에 새지 않게 한다
	New.DisplayAttackPower = FMath::Sqrt(FMath::Max(0.0, New.PrimaryStat * New.WeaponPower / 6.0));
	New.AttackPower = FMath::RoundToDouble(New.DisplayAttackPower * AttackPowerScale);

	New.MaxHP = FMath::Max(1.f, BaseMaxHP + static_cast<float>(New.Vitality) * HPPerVitality);

	const FCombatStatPoints& P = New.CombatPoints;
	New.CritRate                 = FMath::Clamp(BaseCritRate + P.Crit * LoAStatRules::CritRatePerPoint + ExtraCritRate, 0.f, 1.f);
	New.CritDamage               = LoAStatRules::BaseCritDamage + ExtraCritDamage;
	New.SpecializationMultiplier = 1.f + P.Specialization * LoAStatRules::SpecializationPerPoint;
	New.CooldownReduction        = FMath::Clamp(P.Swiftness * LoAStatRules::CooldownPerSwiftness, 0.f, LoAStatRules::MaxCooldownReduction);
	New.MoveSpeedMultiplier      = 1.f + P.Swiftness * LoAStatRules::SpeedPerSwiftness;
	New.AttackSpeedMultiplier    = New.MoveSpeedMultiplier;
	New.DominationMultiplier     = 1.f + P.Domination * LoAStatRules::DominationPerPoint;
	// 인내·숙련은 효과 없음 — 포인트만 들고 있다

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
