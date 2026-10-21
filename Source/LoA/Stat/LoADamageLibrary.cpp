#include "Stat/LoADamageLibrary.h"
#include "Stat/StatComponent.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Stat/DamageMeterComponent.h"
#include "LoAPlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	// 스탯 컴포넌트가 없을 때(테스트용 액터 등) — 예전 스킬 코드의 "캐릭터가 없으면 100"과 같은 값
	constexpr double Damage_FallbackAttackPower = 100.0;

	const AActor* Damage_ResolvePawn(const AActor* Attacker)
	{
		if (const AController* Controller = Cast<AController>(Attacker))
		{
			return Controller->GetPawn();
		}
		return Attacker;
	}

	const UStatComponent* Damage_FindStats(const AActor* Attacker)
	{
		return UStatComponent::Find(Attacker);
	}

	// 특화를 받는 스킬인지 — DT_Skills 행의 입력 타입이 캐스팅·차지(종말의 날·천벌·익스플로전)
	bool Damage_IsSpecializationSkill(const AActor* Attacker, FName SkillRowName)
	{
		Attacker = Damage_ResolvePawn(Attacker);
		const USkillManagerComponent* SkillManager = Attacker ? Attacker->FindComponentByClass<USkillManagerComponent>() : nullptr;
		if (!SkillManager || !SkillManager->SkillDataTable || SkillRowName.IsNone()) return false;

		const FSkillData* Row = SkillManager->SkillDataTable->FindRow<FSkillData>(SkillRowName, TEXT("Specialization"), false);
		return Row && (Row->InputType == ESkillInputType::Cast || Row->InputType == ESkillInputType::Charge);
	}
}

float ULoADamageLibrary::CalcOutgoingDamage(const AActor* Attacker, float Coefficient, FName SkillRowName)
{
	const UStatComponent* StatComp = Damage_FindStats(Attacker);
	if (!StatComp)
	{
		return static_cast<float>(Damage_FallbackAttackPower * Coefficient);
	}

	const FCombatStats& Stats = StatComp->GetStats();
	const bool bSpecialization = Damage_IsSpecializationSkill(Attacker, SkillRowName);
	const double Damage = Stats.AttackPower * Coefficient
		* (bSpecialization ? Stats.SpecializationMultiplier : 1.0)
		* (1.0 + Stats.AdditiveDamageBonus)
		* Stats.MultiplicativeDamage;

	const FString SpecText = bSpecialization ? FString::Printf(TEXT(" × 특화 %.4f"), Stats.SpecializationMultiplier) : FString();
	UE_LOG(LogTemp, Verbose, TEXT("[Damage] 시전 %s — 공격력 %.0f × 계수 %.3f%s → %.0f"),
		*SkillRowName.ToString(), Stats.AttackPower, Coefficient, *SpecText, Damage);
	return static_cast<float>(Damage);
}

float ULoADamageLibrary::ResolveIncomingHit(float BaseDamage, const AActor* Instigator, bool bTargetGroggy, bool& bOutCritical)
{
	bOutCritical = false;

	const UStatComponent* StatComp = Damage_FindStats(Instigator);
	if (!StatComp || BaseDamage <= 0.f)
	{
		return BaseDamage;
	}

	const FCombatStats& Stats = StatComp->GetStats();
	double Damage = BaseDamage;

	bOutCritical = FMath::FRand() < Stats.CritRate;
	if (bOutCritical)
	{
		Damage *= Stats.CritDamage;
	}
	if (bTargetGroggy)
	{
		Damage *= Stats.DominationMultiplier;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[Damage] 피격 %.0f → %.0f%s%s"),
		BaseDamage, Damage, bOutCritical ? TEXT(" (치명)") : TEXT(""), bTargetGroggy ? TEXT(" (그로기)") : TEXT(""));
	return static_cast<float>(Damage);
}

namespace
{
	const TCHAR* Damage_SkillTagPrefix = TEXT("LoASkill.");
}

void ULoADamageLibrary::TagDamageSource(AActor* DamageSource, FName SkillRowName)
{
	if (!DamageSource || SkillRowName.IsNone()) return;
	DamageSource->Tags.AddUnique(FName(*(FString(Damage_SkillTagPrefix) + SkillRowName.ToString())));
}

FName ULoADamageLibrary::GetDamageSourceSkill(const AActor* DamageSource)
{
	if (!DamageSource) return NAME_None;
	for (const FName& Tag : DamageSource->Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(Damage_SkillTagPrefix))
		{
			return FName(*TagString.RightChop(FCString::Strlen(Damage_SkillTagPrefix)));
		}
	}
	return NAME_None;
}

void ULoADamageLibrary::ReportDamageDealt(const AActor* Instigator, const AActor* Target, const AActor* DamageCauser,
	float FinalDamage, bool bCritical, bool bTrainingTarget)
{
	const AActor* Pawn = Damage_ResolvePawn(Instigator);
	const APawn* AsPawn = Cast<APawn>(Pawn);
	const ALoAPlayerController* PC = AsPawn ? Cast<ALoAPlayerController>(AsPawn->GetController()) : nullptr;
	UDamageMeterComponent* Meter = PC ? PC->FindComponentByClass<UDamageMeterComponent>() : nullptr;
	if (!Meter) return;

	FLoADamageEvent Event;
	Event.SkillRowName = GetDamageSourceSkill(DamageCauser);
	Event.FinalDamage = FinalDamage;
	Event.bCritical = bCritical;
	Event.bTrainingTarget = bTrainingTarget;
	Event.Target = Target;
	Meter->RecordHit(Event);
}
