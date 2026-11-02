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

namespace
{
	/** 스킬별 신비한 마력 수급량 — 풀히트 1회(풀 게이지 10000 기준 값 ÷ 100 = 게이지 %), 특화 0·룬 없음.
	 *  출처: 유저가 준 표(혹한 1150 · 아이스 에로우 620 · 인페르노 310/스택 · 블레이즈 틱당 약 30(합 약 220) · 돌풍 50 ·
	 *  종말의 날 250 · 익스플로전 230 · 천벌 200). Key = 데미지 액터에 붙는 SkillRowName, DtRow = 스킬 계수를 읽을 DT 행
	 *  (종말의 날 BP는 행 이름이 "Meteor"라 따로) */
	struct FSorceressSkillGauge
	{
		const TCHAR* Key;
		const TCHAR* DtRow;
		float Gauge;
	};
	const FSorceressSkillGauge SorceressIdentity_SkillGauge[] = {
		{ TEXT("혹한의부름"), TEXT("혹한의부름"), 11.5f },
		{ TEXT("아이스에로우"), TEXT("아이스에로우"), 6.2f },
		{ TEXT("인페르노"),   TEXT("인페르노"),   3.1f },
		{ TEXT("블레이즈"),   TEXT("블레이즈"),   2.2f },
		{ TEXT("돌풍"),       TEXT("돌풍"),       0.5f },
		{ TEXT("종말의날"),   TEXT("종말의날"),   2.5f },
		{ TEXT("Meteor"),     TEXT("종말의날"),   2.5f },
		{ TEXT("익스플로전"), TEXT("익스플로전"), 2.3f },
		{ TEXT("천벌"),       TEXT("천벌"),       2.0f },
	};
}

void USorceressIdentityComponent::AddGaugeFromHit(float HitCoefficient, FName SkillRowName)
{
	if (State != ESorceressMagickState::None || HitCoefficient <= 0.f || Gauge >= LoASorceressIdentity::MaxGauge) return;
	// 정열의 춤사위 +, 안정된 관리자 -
	const float GainScale = FMath::Max(0.f, 1.f + ArkValue(TEXT("Evo_PassionateDance"), 0) / 100.f - ArkValue(TEXT("Evo_StableManager"), 1) / 100.f);

	float Gain = HitCoefficient * LoASorceressIdentity::GaugePerCoefficient;
	const FString Key = SkillRowName.ToString();
	for (const FSorceressSkillGauge& Entry : SorceressIdentity_SkillGauge)
	{
		if (Key != Entry.Key) continue;
		// 다단히트는 타격마다 스킬 계수를 나눠 가지므로, 이 타격의 몫 = 타격 계수 ÷ 스킬 계수(전부 맞으면 합이 1)
		const USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr;
		const FSkillData* Row = SkillManager && SkillManager->SkillDataTable
			? SkillManager->SkillDataTable->FindRow<FSkillData>(Entry.DtRow, TEXT("IdentityGauge"), false) : nullptr;
		const float Share = Row && Row->DamageCoefficient > 0.f ? FMath::Min(1.f, HitCoefficient / Row->DamageCoefficient) : 1.f;
		// 특화 — StatComponent는 배율(1 + 포인트 × 0.0005)만 들고 있어 포인트로 되돌린다
		const UStatComponent* Stats = UStatComponent::Find(GetOwner());
		const float SpecPoints = Stats ? (Stats->GetStats().SpecializationMultiplier - 1.f) / LoAStatRules::SpecializationPerPoint : 0.f;
		Gain = Entry.Gauge * Share * (1.f + FMath::Max(0.f, SpecPoints) * LoASorceressIdentity::GaugeGainPerSpecialization);
		break;
	}
	Gauge = FMath::Min(LoASorceressIdentity::MaxGauge, Gauge + Gain * GainScale);
}

bool USorceressIdentityComponent::CanRelease(FText& OutFailReason) const
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
	return true;
}

bool USorceressIdentityComponent::TryRelease(FText& OutFailReason)
{
	if (!CanRelease(OutFailReason)) return false;
	UWorld* World = GetWorld();
	if (!World) return false;

	const bool bFull = Gauge >= LoASorceressIdentity::MaxGauge - KINDA_SMALL_NUMBER;
	State = bFull ? ESorceressMagickState::Release : ESorceressMagickState::Amplify;
	StateDuration = Gauge * LoASorceressIdentity::SecondsPerGauge;
	StateEndTime = Now() + StateDuration;
	ReleaseReadyTime = Now() + LoASorceressIdentity::ReleaseCooldown;
	Gauge = 0.f;
	World->GetTimerManager().SetTimer(StateTimer, this, &USorceressIdentityComponent::EndState, StateDuration, false);

	// 해방 발동 즉시 스킬(Q~F — 이동기·기상기·각성기 제외)의 남은 쿨타임 25% 감소. 점화는 이 효과를 33/41/50%로 강화(겹쳐 곱하지 않음)
	if (State == ESorceressMagickState::Release)
	{
		const float Reduction = FMath::Max(LoASorceressIdentity::ReleaseCooldownReduction, ArkValue(TEXT("Enl_Igniter"), 0) / 100.f);
		if (USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr)
		{
			SkillManager->ReduceRemainingSkillCooldowns(Reduction);
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
	}
	else if (State == ESorceressMagickState::Release)
	{
		Add(T::SkillDamage, LoASorceressIdentity::ReleaseSkillDamage);
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
