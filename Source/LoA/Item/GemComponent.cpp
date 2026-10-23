#include "Item/GemComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemTypes.h"
#include "Stat/StatComponent.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Raid/EchidnaBoss.h"
#include "LoAGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"

namespace
{
	constexpr int32 Gem_StarterLevel = 7;  // 2026-10-23 Lv5 → Lv7 (유저 요청) — 예전 저장 파일은 BeginPlay에서 한 번 올려 줌
	constexpr int32 Gem_StarterDamageCount = 6;
	constexpr int32 Gem_StarterCooldownCount = 5;
}

UGemComponent::UGemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UGemComponent::IsBuildChangeAllowed(const UWorld* World)
{
	// 보스 맵 = 에키드나가 있는 레벨 (장비 교체와 같은 규칙)
	return World && !UGameplayStatics::GetActorOfClass(World, AEchidnaBoss::StaticClass());
}

UInventoryComponent* UGemComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UStatComponent* UGemComponent::GetStats() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UStatComponent>() : nullptr;
}

void UGemComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(LoAGems::SlotCount);
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (GI)
	{
		for (int32 i = 0; i < LoAGems::SlotCount && i < GI->GemSlots.Num(); ++i)
		{
			EGemType Type;
			int32 Level;
			if (LoAGems::ParseGemId(GI->GemSlots[i].GemItemId, Type, Level)) Slots[i] = GI->GemSlots[i];
		}
		// 지난번에 합성 창에 올려 둔 채 끝난 보석 — 소지품으로 돌려준다 (소지품이 먼저 불러와져 있어야 함)
		if (GI->GemFuseInputs.Num() > 0)
		{
			if (UInventoryComponent* Inventory = GetInventory())
			{
				Inventory->EnsureLoaded();
				for (const FName& Id : GI->GemFuseInputs)
				{
					if (Inventory->AddItem(Id, 1) > 0) FuseInputs.Add(Id);  // 칸이 없으면 재료 칸에 그대로
				}
			}
			GI->GemFuseInputs = FuseInputs;
		}
		// 7레벨 미만으로 장착된 보석은 한 번 7레벨로 (시작 보석을 Lv5 → Lv7로 바꿀 때 기존 저장 파일용)
		if (GI->bHasStarterGems && !GI->bGemsUpgradedToLv7)
		{
			for (FGemSlot& Slot : Slots)
			{
				EGemType Type;
				int32 Level;
				if (LoAGems::ParseGemId(Slot.GemItemId, Type, Level) && Level < Gem_StarterLevel) Slot.GemItemId = LoAGems::MakeGemId(Type, Gem_StarterLevel);
			}
			UE_LOG(LogTemp, Log, TEXT("[Gem] 장착 보석을 Lv%d로 올림 (한 번)"), Gem_StarterLevel);
		}
		GI->bGemsUpgradedToLv7 = true;
		if (!GI->bHasStarterGems)
		{
			GrantStarterGems();
			GI->bHasStarterGems = true;
			SaveToGameInstance(true);
		}
	}
	ApplyStats();
}

void UGemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReturnAllFuseInputs();
	SaveToGameInstance(true);
	Super::EndPlay(EndPlayReason);
}

void UGemComponent::SaveToGameInstance(bool bWriteDisk) const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (!GI) return;
	GI->GemSlots = Slots;
	GI->GemFuseInputs = FuseInputs;
	if (bWriteDisk)
	{
		// 소지품도 같이 — 보석이 소지품↔칸을 오가므로 둘이 어긋난 채 저장되면 보석이 사라지거나 늘어난다
		if (const UInventoryComponent* Inventory = GetInventory())
		{
			if (!Inventory->WriteToGameInstance()) return;  // 판매 목록에 올린 게 있으면 저장 보류 (장비 SaveNow와 같은 규칙)
		}
		GI->SaveToDisk();
	}
}

void UGemComponent::GrantStarterGems()
{
	// 딜 비중(계수 ÷ 쿨타임)이 높은 스킬부터 — 겁화 6개, 작열 5개
	const USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr;
	if (!SkillManager || !SkillManager->SkillDataTable) return;

	TArray<TPair<float, FName>> Ranked;
	for (const FName& Row : SkillManager->GetAllSkillRowNames())
	{
		if (!SkillManager->IsSkillTreeRow(Row)) continue;
		const FSkillData* Data = SkillManager->SkillDataTable->FindRow<FSkillData>(Row, TEXT("StarterGems"), false);
		if (!Data || Data->Cooldown <= 0.f) continue;
		Ranked.Emplace(Data->DamageCoefficient / Data->Cooldown, Row);
	}
	Ranked.Sort([](const TPair<float, FName>& A, const TPair<float, FName>& B) { return A.Key > B.Key; });

	int32 Next = 0;
	for (int32 i = 0; i < Ranked.Num() && i < Gem_StarterDamageCount && Next < LoAGems::SlotCount; ++i)
	{
		Slots[Next++] = { LoAGems::MakeGemId(EGemType::Damage, Gem_StarterLevel), Ranked[i].Value };
	}
	for (int32 i = 0; i < Ranked.Num() && i < Gem_StarterCooldownCount && Next < LoAGems::SlotCount; ++i)
	{
		Slots[Next++] = { LoAGems::MakeGemId(EGemType::Cooldown, Gem_StarterLevel), Ranked[i].Value };
	}
	UE_LOG(LogTemp, Log, TEXT("[Gem] 시작 보석 Lv%d %d개 장착"), Gem_StarterLevel, Next);
}

void UGemComponent::ApplyStats()
{
	UStatComponent* Stats = GetStats();
	if (!Stats) return;

	TMap<FName, float> Damage, Cooldown;
	double AttackPercent = 0.0;
	for (const FGemSlot& Slot : Slots)
	{
		EGemType Type;
		int32 Level;
		if (Slot.SkillRowName.IsNone() || !LoAGems::ParseGemId(Slot.GemItemId, Type, Level)) continue;
		(Type == EGemType::Damage ? Damage : Cooldown).FindOrAdd(Slot.SkillRowName) += LoAGems::GetEffect(Type, Level);
		AttackPercent += LoAGems::GetAttackPowerPercent(Level);
	}
	Stats->SetSkillGemBonuses(Damage, Cooldown);

	TArray<FStatModifier> Mods;
	if (AttackPercent > 0.0)
	{
		FStatModifier& Mod = Mods.AddDefaulted_GetRef();
		Mod.Target = EStatModifierTarget::AttackPowerPercent;
		Mod.Value = AttackPercent;
	}
	Stats->SetModifiers(TEXT("Gem"), Mods);
	OnGemsChanged.Broadcast();
}

int32 UGemComponent::GetGemLevelOnSkill(FName SkillRowName, EGemType Type) const
{
	for (const FGemSlot& Slot : Slots)
	{
		EGemType SlotType;
		int32 Level;
		if (Slot.SkillRowName == SkillRowName && LoAGems::ParseGemId(Slot.GemItemId, SlotType, Level) && SlotType == Type) return Level;
	}
	return 0;
}

bool UGemComponent::EquipGem(FName GemItemId, FName SkillRowName, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	EGemType Type;
	int32 Level;
	if (!Inventory || !LoAGems::ParseGemId(GemItemId, Type, Level) || SkillRowName.IsNone()) return false;
	if (!IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Gem", "NotHere", "레이드 중에는 보석을 바꿀 수 없습니다.");
		return false;
	}
	if (Inventory->CountItem(GemItemId) <= 0)
	{
		OutFailReason = NSLOCTEXT("Gem", "NoGem", "소지품에 그 보석이 없습니다.");
		return false;
	}

	// 같은 스킬·같은 종류가 이미 있으면 그 칸을 바꿔 끼우고, 없으면 빈 칸
	int32 Target = INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		EGemType SlotType;
		int32 SlotLevel;
		if (Slots[i].SkillRowName == SkillRowName && LoAGems::ParseGemId(Slots[i].GemItemId, SlotType, SlotLevel) && SlotType == Type)
		{
			Target = i;
			break;
		}
	}
	if (Target == INDEX_NONE)
	{
		Target = Slots.IndexOfByPredicate([](const FGemSlot& S) { return S.IsEmpty(); });
	}
	if (Target == INDEX_NONE)
	{
		OutFailReason = NSLOCTEXT("Gem", "Full", "보석 칸 11개가 모두 찼습니다. 먼저 하나를 빼 주세요.");
		return false;
	}

	const FName OldGem = Slots[Target].GemItemId;
	Inventory->RemoveItem(GemItemId, 1);
	if (!OldGem.IsNone() && Inventory->AddItem(OldGem, 1) > 0)
	{
		// 원래 보석이 돌아갈 자리가 없으면 되돌린다
		Inventory->AddItem(GemItemId, 1);
		OutFailReason = NSLOCTEXT("Gem", "InvFull", "소지품이 가득 차 원래 보석을 돌려받을 수 없습니다.");
		return false;
	}
	Slots[Target] = { GemItemId, SkillRowName };
	ApplyStats();
	SaveToGameInstance(true);
	return true;
}

bool UGemComponent::UnequipGem(int32 SlotIndex, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty()) return false;
	if (!IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Gem", "NotHere", "레이드 중에는 보석을 바꿀 수 없습니다.");
		return false;
	}
	if (Inventory->AddItem(Slots[SlotIndex].GemItemId, 1) > 0)
	{
		OutFailReason = NSLOCTEXT("Gem", "InvFullOut", "소지품이 가득 찼습니다.");
		return false;
	}
	Slots[SlotIndex] = FGemSlot();
	ApplyStats();
	SaveToGameInstance(true);
	return true;
}

bool UGemComponent::AddFuseInput(FName GemItemId, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	EGemType Type;
	int32 Level;
	if (!Inventory || !LoAGems::ParseGemId(GemItemId, Type, Level)) return false;
	if (!IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Gem", "NotHereFuse", "레이드 중에는 보석을 합성할 수 없습니다.");
		return false;
	}
	if (Level >= LoAGems::MaxLevel)
	{
		OutFailReason = NSLOCTEXT("Gem", "MaxLevel", "10레벨 보석은 더 합성할 수 없습니다.");
		return false;
	}
	if (FuseInputs.Num() >= LoAGems::FuseCount)
	{
		OutFailReason = NSLOCTEXT("Gem", "FuseFull", "재료 칸이 가득 찼습니다 — 재료 칸을 우클릭해 빼 주세요.");
		return false;
	}
	if (FuseInputs.Num() > 0)
	{
		EGemType FirstType;
		int32 FirstLevel;
		LoAGems::ParseGemId(FuseInputs[0], FirstType, FirstLevel);
		if (FirstLevel != Level)
		{
			OutFailReason = NSLOCTEXT("Gem", "SameLevel", "같은 레벨 보석끼리만 합성할 수 있습니다.");
			return false;
		}
	}
	if (Inventory->RemoveItem(GemItemId, 1) <= 0) return false;
	FuseInputs.Add(GemItemId);
	SaveToGameInstance(true);
	OnFuseInputsChanged.Broadcast();
	return true;
}

bool UGemComponent::RemoveFuseInput(int32 Index, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !FuseInputs.IsValidIndex(Index)) return false;
	if (Inventory->AddItem(FuseInputs[Index], 1) > 0)
	{
		OutFailReason = NSLOCTEXT("Gem", "InvFullOut", "소지품이 가득 찼습니다.");
		return false;
	}
	FuseInputs.RemoveAt(Index);
	SaveToGameInstance(true);
	OnFuseInputsChanged.Broadcast();
	return true;
}

void UGemComponent::ReturnAllFuseInputs()
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory || FuseInputs.Num() == 0) return;
	for (int32 i = FuseInputs.Num() - 1; i >= 0; --i)
	{
		// 칸이 없으면 재료 칸에 남긴다 — 저장 파일에 들어가 있어 다음 실행 때 다시 돌려준다
		if (Inventory->AddItem(FuseInputs[i], 1) == 0) FuseInputs.RemoveAt(i);
	}
	SaveToGameInstance(true);
	OnFuseInputsChanged.Broadcast();
}

bool UGemComponent::FillFuseInputsForTarget(int32 TargetLevel, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory) return false;
	ReturnAllFuseInputs();
	if (FuseInputs.Num() > 0) return false;  // 돌려놓을 자리가 없었음

	for (int32 Level = 1; Level < FMath::Min(TargetLevel, LoAGems::MaxLevel); ++Level)
	{
		// 이 레벨의 보석을 하나씩 늘어놓고 3개를 무작위로
		TArray<FName> Pool;
		for (const EGemType Type : { EGemType::Damage, EGemType::Cooldown })
		{
			const FName Id = LoAGems::MakeGemId(Type, Level);
			for (int32 i = 0, Count = Inventory->CountItem(Id); i < Count; ++i) Pool.Add(Id);
		}
		if (Pool.Num() < LoAGems::FuseCount) continue;
		for (int32 i = 0; i < LoAGems::FuseCount; ++i)
		{
			const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
			Inventory->RemoveItem(Pool[Pick], 1);
			FuseInputs.Add(Pool[Pick]);
			Pool.RemoveAtSwap(Pick);
		}
		SaveToGameInstance(true);
		OnFuseInputsChanged.Broadcast();
		return true;
	}
	OutFailReason = FText::FromString(FString::Printf(TEXT("%d레벨보다 낮은 같은 레벨 보석이 3개 이상 없습니다."), TargetLevel));
	return false;
}

bool UGemComponent::FuseInputs3(FName& OutResult, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory) return false;
	if (!IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Gem", "NotHereFuse", "레이드 중에는 보석을 합성할 수 없습니다.");
		return false;
	}
	if (FuseInputs.Num() != LoAGems::FuseCount)
	{
		OutFailReason = NSLOCTEXT("Gem", "NeedThree", "같은 레벨 보석 3개를 올려 주세요.");
		return false;
	}
	EGemType ResultType;
	int32 Level;
	// 결과 종류 = 재료 3개 중 하나를 랜덤으로 (겁화 2 + 작열 1이면 겁화 2/3)
	LoAGems::ParseGemId(FuseInputs[FMath::RandRange(0, FuseInputs.Num() - 1)], ResultType, Level);
	OutResult = LoAGems::MakeGemId(ResultType, Level + 1);
	if (Inventory->AddItem(OutResult, 1) > 0)
	{
		OutFailReason = NSLOCTEXT("Gem", "FuseInvFull", "소지품에 빈 칸이 없습니다.");
		return false;
	}
	FuseInputs.Reset();
	SaveToGameInstance(true);
	OnFuseInputsChanged.Broadcast();
	return true;
}
