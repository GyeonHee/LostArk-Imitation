#include "Stat/ArkPassiveComponent.h"
#include "Stat/StatComponent.h"
#include "Item/EquipmentComponent.h"
#include "Item/GemComponent.h"
#include "Skill/SkillManagerComponent.h"
#include "LoACharacter.h"
#include "LoAGameInstance.h"
#include "TimerManager.h"

namespace
{
	const FName ArkPassive_StaticSource(TEXT("ArkPassive"));
	const FName ArkPassive_BuffSource(TEXT("ArkPassiveBuff"));
}

UArkPassiveComponent::UArkPassiveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UStatComponent* UArkPassiveComponent::GetStats() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UStatComponent>() : nullptr;
}

bool UArkPassiveComponent::IsTreeActive(EArkPassiveTree Tree) const
{
	return LoAArkPassive::GetSpent(Levels, Tree) <= Budget.Get(Tree);
}

int32 UArkPassiveComponent::GetActiveLevel(FName Id) const
{
	return LoAArkPassive::GetLevel(EffectiveLevels, Id);
}

void UArkPassiveComponent::RefreshBudget()
{
	const UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
	Budget = LoAArkPassive::ComputeBudget(Equipment);

	// 실제 적용 레벨 — 포인트 안이면 그대로, 모자라면 (티어, 열) 순서로 한 레벨씩 들어가는 만큼만
	EffectiveLevels.Reset();
	TArray<const FArkPassiveNode*> Ordered;
	for (const FArkPassiveNode& Node : LoAArkPassive::GetAll())
	{
		if (LoAArkPassive::GetLevel(Levels, Node.Id) > 0) Ordered.Add(&Node);
	}
	Ordered.Sort([](const FArkPassiveNode& A, const FArkPassiveNode& B)
	{
		if (A.Tree != B.Tree) return A.Tree < B.Tree;
		return A.Tier != B.Tier ? A.Tier < B.Tier : A.Column < B.Column;
	});
	for (const FArkPassiveNode* Node : Ordered)
	{
		const int32 Wanted = LoAArkPassive::GetLevel(Levels, Node->Id);
		if (IsTreeActive(Node->Tree))
		{
			EffectiveLevels.Add(Node->Id, Wanted);
			continue;
		}
		for (int32 L = 1; L <= Wanted; ++L)
		{
			TMap<FName, int32> Trial = EffectiveLevels;
			Trial.Add(Node->Id, L);
			FText Reason;
			if (!LoAArkPassive::Validate(Trial, &Budget, Reason)) break;
			EffectiveLevels = MoveTemp(Trial);
		}
	}
}

void UArkPassiveComponent::RefreshAll()
{
	RefreshBudget();
	ApplyStatic();
	ApplyBuffs();
	UE_LOG(LogTemp, Log, TEXT("[ArkPassive] 포인트 진화 %d/%d · 깨달음 %d/%d · 도약 %d/%d"),
		LoAArkPassive::GetSpent(EffectiveLevels, EArkPassiveTree::Evolution), Budget.Get(EArkPassiveTree::Evolution),
		LoAArkPassive::GetSpent(EffectiveLevels, EArkPassiveTree::Enlightenment), Budget.Get(EArkPassiveTree::Enlightenment),
		LoAArkPassive::GetSpent(EffectiveLevels, EArkPassiveTree::Leap), Budget.Get(EArkPassiveTree::Leap));
}

void UArkPassiveComponent::BeginPlay()
{
	Super::BeginPlay();

	// 장비 컴포넌트가 먼저 BeginPlay에서 장착을 끝낸다(생성 순서) — 그 장비로 포인트 계산
	RefreshBudget();
	if (UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr)
	{
		Equipment->OnEquipmentChanged.AddWeakLambda(this, [this]() { RefreshAll(); });
	}
	// 같은 프레임의 다른 컴포넌트 BeginPlay(장비 장착)가 끝난 뒤 한 번 더
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { RefreshAll(); }));
	}

	if (ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr)
	{
		// 포인트 규칙(장비 기준)·점화 추천 세팅 이전 배치(진화 140 기준 환류)는 한 번 점화 추천 세팅으로 바꾼다
		if (!GI->bArkPassiveBudgetV2)
		{
			GI->bHasStarterArkPassive = false;
			GI->bArkPassiveBudgetV2 = true;
		}
		if (!GI->bHasStarterArkPassive)
		{
			Levels = LoAArkPassive::GetRecommended(Budget);
			GI->bHasStarterArkPassive = true;
			SaveToGameInstance(true);
			UE_LOG(LogTemp, Log, TEXT("[ArkPassive] 시작 배치(추천 세팅) 적용"));
		}
		else
		{
			Levels = GI->ArkPassiveLevels;
			// 표에서 사라진 노드·규칙 위반(저장 파일을 손댔거나 규칙이 바뀐 경우)은 통째로 추천 세팅으로.
			// 포인트 합계는 안 본다 — 장비를 벗어 모자란 건 효과만 끄고 배치는 남긴다
			for (auto It = Levels.CreateIterator(); It; ++It)
			{
				if (!LoAArkPassive::Find(It.Key()) || It.Value() <= 0) It.RemoveCurrent();
			}
			FText Reason;
			if (!LoAArkPassive::Validate(Levels, nullptr, Reason))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ArkPassive] 저장된 배치가 규칙에 맞지 않음(%s) — 추천 세팅으로"), *Reason.ToString());
				Levels = LoAArkPassive::GetRecommended(Budget);
			}
		}
	}

	if (USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr)
	{
		SkillManager->OnSkillUsed.AddUObject(this, &UArkPassiveComponent::HandleSkillUsed);
	}
	RefreshAll();
}

void UArkPassiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveToGameInstance(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UArkPassiveComponent::SaveToGameInstance(bool bWriteDisk) const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (!GI) return;
	GI->ArkPassiveLevels = Levels;
	if (!bWriteDisk) return;
	// 소지품·장착 GUID를 같이 써야 어긋나지 않는다 — 장비 컴포넌트의 즉시 저장을 탄다 (없으면 그냥 디스크로)
	if (const UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr)
	{
		Equipment->SaveNow();
	}
	else
	{
		GI->SaveToDisk();
	}
}

bool UArkPassiveComponent::SetAllLevels(const TMap<FName, int32>& NewLevels, FText& OutFailReason)
{
	if (!UGemComponent::IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("ArkPassive", "NotHere", "레이드 중에는 아크 패시브를 바꿀 수 없습니다.");
		return false;
	}
	TMap<FName, int32> Clean;
	for (const TPair<FName, int32>& Pair : NewLevels)
	{
		if (Pair.Value > 0 && LoAArkPassive::Find(Pair.Key)) Clean.Add(Pair.Key, Pair.Value);
	}
	if (!LoAArkPassive::Validate(Clean, &Budget, OutFailReason)) return false;

	Levels = MoveTemp(Clean);
	// 배운 게 바뀌면 발동 중이던 효과는 끈다 (빠진 노드의 버프가 남지 않게)
	ClearBuffs();
	RefreshAll();
	SaveToGameInstance(true);
	return true;
}

void UArkPassiveComponent::ApplyStatic()
{
	UStatComponent* Stats = GetStats();
	if (!Stats) return;
	TArray<FStatModifier> Modifiers;
	TMap<FName, float> SkillDamage;
	LoAArkPassive::BuildStaticModifiers(EffectiveLevels, Modifiers, SkillDamage);
	Stats->SetModifiers(ArkPassive_StaticSource, Modifiers);
	Stats->SetSkillArkPassiveBonuses(SkillDamage);
	OnArkPassiveChanged.Broadcast();
}

void UArkPassiveComponent::ApplyBuffs()
{
	UStatComponent* Stats = GetStats();
	if (!Stats) return;
	TArray<FStatModifier> Modifiers;
	auto Add = [&Modifiers](EStatModifierTarget Target, double Value)
	{
		if (Value == 0.0) return;
		FStatModifier& M = Modifiers.AddDefaulted_GetRef();
		M.Target = Target;
		M.Value = Value;
	};
	if (MasterStacks > 0 && GetActiveLevel(TEXT("Evo_Master")) > 0)
	{
		Add(EStatModifierTarget::CritRate, LoAArkPassive::MasterCritPerStack * MasterStacks);
		Add(EStatModifierTarget::AdditiveDamage, LoAArkPassive::MasterDamagePerStack * MasterStacks);
	}
	if (const FArkPassiveNode* Charge = LoAArkPassive::Find(TEXT("Enl_MagickCharge")); Charge && bMagickCharge)
	{
		Add(EStatModifierTarget::AttackPowerPercent, LoAArkPassive::GetValue(*Charge, 0, GetActiveLevel(Charge->Id)) / 100.0);
	}
	if (const FArkPassiveNode* Accel = LoAArkPassive::Find(TEXT("Enl_ConcentratedMagick")); Accel && AccelStacks > 0)
	{
		const int32 L = GetActiveLevel(Accel->Id);
		Add(EStatModifierTarget::MultiplicativeDamage, LoAArkPassive::GetValue(*Accel, 0, L) / 100.0 * AccelStacks);
		if (AccelStacks >= LoAArkPassive::AccelMaxStacks)
		{
			Add(EStatModifierTarget::AttackSpeedBonus, LoAArkPassive::GetValue(*Accel, 1, L) / 100.0);
		}
	}
	Stats->SetModifiers(ArkPassive_BuffSource, Modifiers);
	OnArkPassiveChanged.Broadcast();
}

void UArkPassiveComponent::ClearBuffs()
{
	MasterStacks = 0;
	bMagickCharge = false;
	AccelStacks = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MasterTimer);
		World->GetTimerManager().ClearTimer(MagickChargeTimer);
		World->GetTimerManager().ClearTimer(AccelTimer);
	}
}

void UArkPassiveComponent::HandleSkillUsed(int32 SlotIndex)
{
	// 달인 — 이동기·기상기를 제외한 스킬 사용마다 +1 (최대 5), 10초 동안 안 쓰면 전부 사라진다
	if (GetActiveLevel(TEXT("Evo_Master")) <= 0 || !GetWorld()) return;
	MasterStacks = FMath::Min(MasterStacks + 1, LoAArkPassive::MasterMaxStacks);
	GetWorld()->GetTimerManager().SetTimer(MasterTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		MasterStacks = 0;
		ApplyBuffs();
	}), LoAArkPassive::MasterDuration, false);
	ApplyBuffs();
}

void UArkPassiveComponent::NotifyMovementSkill()
{
	UWorld* World = GetWorld();
	if (!World) return;
	bool bChanged = false;

	// 환류의 기운 — 점멸 시 최대 마나 7% 회복
	if (GetActiveLevel(TEXT("Enl_RefluxEnergy")) > 0)
	{
		if (ALoACharacter* Char = Cast<ALoACharacter>(GetOwner()))
		{
			Char->RestoreMP(Char->GetMaxMP() * LoAArkPassive::RefluxMPRestore);
		}
	}
	// 마력 충전 — 45초간 공격력 증가 (다시 쓰면 시간만 갱신)
	if (GetActiveLevel(TEXT("Enl_MagickCharge")) > 0)
	{
		bMagickCharge = true;
		World->GetTimerManager().SetTimer(MagickChargeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bMagickCharge = false;
			ApplyBuffs();
		}), LoAArkPassive::MagickChargeDuration, false);
		bChanged = true;
	}
	// 응집되는 마력 — 45초 마력 가속, 최대 2중첩
	if (GetActiveLevel(TEXT("Enl_ConcentratedMagick")) > 0)
	{
		AccelStacks = FMath::Min(AccelStacks + 1, LoAArkPassive::AccelMaxStacks);
		World->GetTimerManager().SetTimer(AccelTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			AccelStacks = 0;
			ApplyBuffs();
		}), LoAArkPassive::AccelDuration, false);
		bChanged = true;
	}
	if (bChanged) ApplyBuffs();
}
