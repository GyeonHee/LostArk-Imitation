#include "Character/SorceressIdentityComponent.h"
#include "Character/Sorceress.h"
#include "Skill/SkillManagerComponent.h"
#include "Stat/ArkPassiveComponent.h"
#include "Stat/ArkPassiveTypes.h"
#include "Stat/StatComponent.h"
#include "TimerManager.h"

namespace
{
	const FName Identity_StatSource(TEXT("Identity"));
}

USorceressIdentityComponent::USorceressIdentityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USorceressIdentityComponent::BeginPlay()
{
	Super::BeginPlay();
	// 아크 패시브를 바꾸면(정비소) 켜져 있는 상태의 수치도 다시
	if (UArkPassiveComponent* Ark = GetArkPassive())
	{
		Ark->OnArkPassiveChanged.AddWeakLambda(this, [this]()
		{
			if (State != ESorceressMagickState::None || bMagickCycleSpeed) ApplyModifiers();
		});
	}
}

void USorceressIdentityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

float USorceressIdentityComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

UArkPassiveComponent* USorceressIdentityComponent::GetArkPassive() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UArkPassiveComponent>() : nullptr;
}

int32 USorceressIdentityComponent::ArkLevel(const TCHAR* NodeId) const
{
	// 실제 적용 레벨 — 포인트가 모자라 빠진 노드는 0
	const UArkPassiveComponent* Ark = GetArkPassive();
	return Ark ? LoAArkPassive::GetLevel(Ark->GetEffectiveLevels(), NodeId) : 0;
}

float USorceressIdentityComponent::ArkValue(const TCHAR* NodeId, int32 Index) const
{
	const int32 Level = ArkLevel(NodeId);
	const FArkPassiveNode* Node = Level > 0 ? LoAArkPassive::Find(NodeId) : nullptr;
	return Node ? LoAArkPassive::GetValue(*Node, Index, Level) : 0.f;
}

float USorceressIdentityComponent::GetStateRemaining() const
{
	return State == ESorceressMagickState::None ? 0.f : FMath::Max(0.f, StateEndTime - Now());
}

float USorceressIdentityComponent::GetReleaseCooldownRemaining() const
{
	return FMath::Max(0.f, ReleaseReadyTime - Now());
}

float USorceressIdentityComponent::GetBlinkCooldownRemaining() const
{
	return FMath::Max(0.f, BlinkReadyTime - Now());
}

bool USorceressIdentityComponent::IsReleaseBlocked() const
{
	return ArkLevel(TEXT("Enl_Reflux")) > 0 || ArkLevel(TEXT("Enl_ConcentratedMagick")) > 0;
}

float USorceressIdentityComponent::GetBlinkCost() const
{
	if (ArkLevel(TEXT("Enl_BlinkExplosion")) > 0) return 0.f;
	return LoASorceressIdentity::BlinkCost * (1.f - ArkValue(TEXT("Enl_RefluxEnergy"), 0) / 100.f);
}

void USorceressIdentityComponent::AddGaugeFromHit(float HitCoefficient)
{
	if (State != ESorceressMagickState::None || HitCoefficient <= 0.f || Gauge >= LoASorceressIdentity::MaxGauge) return;
	// 정열의 춤사위 +, 안정된 관리자 -
	const float GainScale = FMath::Max(0.f, 1.f + ArkValue(TEXT("Evo_PassionateDance"), 0) / 100.f - ArkValue(TEXT("Evo_StableManager"), 1) / 100.f);
	Gauge = FMath::Min(LoASorceressIdentity::MaxGauge, Gauge + HitCoefficient * LoASorceressIdentity::GaugePerCoefficient * GainScale);
}

bool USorceressIdentityComponent::TryRelease(FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	if (IsReleaseBlocked())
	{
		OutFailReason = NSLOCTEXT("Identity", "ReleaseBlocked", "환류(또는 응집되는 마력)를 배워 마력 방출을 사용할 수 없습니다.");
		return false;
	}
	if (State != ESorceressMagickState::None || Now() < ReleaseReadyTime) return false;
	if (Gauge < LoASorceressIdentity::MinRelease)
	{
		OutFailReason = NSLOCTEXT("Identity", "NotEnough", "신비한 마력이 부족합니다.");
		return false;
	}
	UWorld* World = GetWorld();
	if (!World) return false;

	const bool bFull = Gauge >= LoASorceressIdentity::MaxGauge - KINDA_SMALL_NUMBER;
	State = bFull ? ESorceressMagickState::Release : ESorceressMagickState::Amplify;
	StateDuration = Gauge * LoASorceressIdentity::SecondsPerGauge;
	StateEndTime = Now() + StateDuration;
	ReleaseReadyTime = Now() + LoASorceressIdentity::ReleaseCooldown;
	Gauge = 0.f;
	World->GetTimerManager().SetTimer(StateTimer, this, &USorceressIdentityComponent::EndState, StateDuration, false);

	// 점화 — 해방 발동 시 스킬(Q~F)의 남은 쿨타임 감소
	if (State == ESorceressMagickState::Release)
	{
		const float Igniter = ArkValue(TEXT("Enl_Igniter"), 0) / 100.f;
		USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr;
		if (Igniter > 0.f && SkillManager)
		{
			SkillManager->ReduceRemainingSkillCooldowns(Igniter);
		}
	}
	ApplyModifiers();
	NotifySkillUsed();
	UE_LOG(LogTemp, Log, TEXT("[Identity] 마력 %s %.1f초"), bFull ? TEXT("해방") : TEXT("강화"), StateDuration);
	return true;
}

bool USorceressIdentityComponent::TryBlink(const FVector& TargetLocation, FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	if (Now() < BlinkReadyTime) return false;
	const float Cost = GetBlinkCost();
	if (Gauge < Cost - KINDA_SMALL_NUMBER)
	{
		OutFailReason = NSLOCTEXT("Identity", "NotEnough", "신비한 마력이 부족합니다.");
		return false;
	}
	ASorceress* Sorceress = Cast<ASorceress>(GetOwner());
	if (!Sorceress || !Sorceress->BlinkTo(TargetLocation)) return false;

	Gauge = FMath::Max(0.f, Gauge - Cost);
	// 점멸 폭발 — 게이지 대신 쿨타임 +20초. 해방 중엔 점멸 쿨타임 절반
	BlinkCooldownTotal = LoASorceressIdentity::BlinkCooldown + (ArkLevel(TEXT("Enl_BlinkExplosion")) > 0 ? 20.f : 0.f);
	if (State == ESorceressMagickState::Release)
	{
		BlinkCooldownTotal *= LoASorceressIdentity::BlinkReleaseCooldownScale;
	}
	BlinkReadyTime = Now() + BlinkCooldownTotal;

	// 환류의 기운·마력 충전·응집되는 마력
	if (UArkPassiveComponent* Ark = GetArkPassive())
	{
		Ark->NotifyMovementSkill();
	}
	NotifySkillUsed();
	return true;
}

void USorceressIdentityComponent::SetGaugeForTest(float NewGauge)
{
	Gauge = FMath::Clamp(NewGauge, 0.f, LoASorceressIdentity::MaxGauge);
}

void USorceressIdentityComponent::ResetIdentity()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StateTimer);
		World->GetTimerManager().ClearTimer(MagickCycleTimer);
	}
	Gauge = 0.f;
	State = ESorceressMagickState::None;
	StateDuration = 0.f;
	bMagickCycleSpeed = false;
	ApplyModifiers();
}

void USorceressIdentityComponent::EndState()
{
	const bool bWasRelease = State == ESorceressMagickState::Release;
	State = ESorceressMagickState::None;
	StateDuration = 0.f;

	// 마나 순환 — 해방이 끝나면 30초간 공격 속도
	UWorld* World = GetWorld();
	if (bWasRelease && World && ArkLevel(TEXT("Enl_MagickCycle")) > 0)
	{
		bMagickCycleSpeed = true;
		World->GetTimerManager().SetTimer(MagickCycleTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bMagickCycleSpeed = false;
			ApplyModifiers();
		}), LoASorceressIdentity::MagickCycleDuration, false);
	}
	ApplyModifiers();
}

void USorceressIdentityComponent::ApplyModifiers()
{
	UStatComponent* Stats = GetOwner() ? GetOwner()->FindComponentByClass<UStatComponent>() : nullptr;
	if (!Stats) return;

	TArray<FStatModifier> Modifiers;
	auto Add = [&Modifiers](EStatModifierTarget Target, double Value)
	{
		if (Value == 0.0) return;
		FStatModifier& M = Modifiers.AddDefaulted_GetRef();
		M.Target = Target;
		M.Value = Value;
	};
	using T = EStatModifierTarget;
	if (State == ESorceressMagickState::Amplify)
	{
		Add(T::SkillDamage, LoASorceressIdentity::AmplifySkillDamage);
		Add(T::CastSpeed, LoASorceressIdentity::AmplifyCastSpeed);
	}
	else if (State == ESorceressMagickState::Release)
	{
		Add(T::SkillDamage, LoASorceressIdentity::ReleaseSkillDamage);
		Add(T::CastSpeed, LoASorceressIdentity::ReleaseCastSpeed);
		Add(T::CooldownReduction, LoASorceressIdentity::ReleaseCooldownReduction);
		Add(T::CritRate, ArkValue(TEXT("Enl_IgniterEmber"), 0) / 100.0);        // 점화의 불씨
		Add(T::CritDamage, ArkValue(TEXT("Enl_Ignite"), 0) / 100.0);            // 발화 (무력화 피해는 없음)
		Add(T::MultiplicativeDamage, ArkValue(TEXT("Enl_MagickCycle"), 1) / 100.0);  // 마나 순환 — 해방 중 적에게 주는 피해
	}
	if (State != ESorceressMagickState::None)
	{
		Add(T::MultiplicativeDamage, ArkValue(TEXT("Enl_BlinkExplosion"), 0) / 100.0);  // 점멸 폭발 — 강화·해방 중 피해 +
		Add(T::MultiplicativeDamage, -ArkValue(TEXT("Enl_ReleaseSeal"), 1) / 100.0);    // 해방 봉인 — 강화·해방 중 피해 -
	}
	if (bMagickCycleSpeed)
	{
		Add(T::AttackSpeedBonus, LoASorceressIdentity::MagickCycleAttackSpeed);
	}
	Stats->SetModifiers(Identity_StatSource, Modifiers);
}

void USorceressIdentityComponent::NotifySkillUsed()
{
	// 로아: 마력 방출·점멸도 스킬 사용 판정 — 아드레날린·달인 중첩
	if (USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr)
	{
		SkillManager->OnSkillUsed.Broadcast(INDEX_NONE);
	}
}
