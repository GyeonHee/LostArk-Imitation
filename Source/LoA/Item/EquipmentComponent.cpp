#include "Item/EquipmentComponent.h"
#include "Item/InventoryComponent.h"
#include "Stat/StatComponent.h"
#include "Raid/EchidnaBoss.h"
#include "LoAGameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName Equip_ModifierSource(TEXT("Equipment"));
}

UEquipmentComponent::UEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Equipped.SetNum(LoAEquipRules::SlotCount);
}

UInventoryComponent* UEquipmentComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equipment] 소지품 컴포넌트 없음 — 장비 비활성"));
		return;
	}
	// 소지품보다 먼저 BeginPlay될 수 있어 인스턴스 맵을 먼저 불러오게 한다
	Inventory->EnsureLoaded();

	Equipped.Init(FGuid(), LoAEquipRules::SlotCount);
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (GI)
	{
		for (int32 i = 0; i < LoAEquipRules::SlotCount; ++i)
		{
			const FGuid Id = GI->EquippedIds.IsValidIndex(i) ? GI->EquippedIds[i] : FGuid();
			const FEquipmentInstance* Instance = Inventory->FindEquipment(Id);
			const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
			// 부위가 맞는 장비만 — 정의가 바뀌었거나 인스턴스를 잃었으면 비운다
			Equipped[i] = (Def && static_cast<int32>(Def->EquipSlot) == i) ? Id : FGuid();
		}

		// 시작 장비 — 한 번만. 장비 이전 저장 파일에도 지급(실링과 같은 방식)
		if (!GI->bHasStarterGear)
		{
			for (int32 i = 0; i < LoAEquipRules::SlotCount; ++i)
			{
				const FGuid Id = Inventory->CreateEquipment(LoAItems::GetStarterEquipment(static_cast<EEquipSlot>(i)), LoAEquipRules::StarterRefineLevel);
				if (!Equipped[i].IsValid())
				{
					Equipped[i] = Id;
				}
				else if (Inventory->PlaceEquipmentInEmptySlot(Id) == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Equipment] 시작 장비 — 소지품이 가득 차 %d번 부위 지급 못 함"), i);
				}
			}
			GI->bHasStarterGear = true;
			UE_LOG(LogTemp, Log, TEXT("[Equipment] 시작 장비 1640 세트 지급"));
		}
	}

	ApplyStats();
}

void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr)
	{
		GI->EquippedIds = Equipped;
		// 인스턴스 맵은 소지품이 저장하지만, 소지품이 먼저 EndPlay됐을 수도 있어 장착 GUID까지 확실히 디스크에 쓴다
		if (const UInventoryComponent* Inventory = GetInventory())
		{
			for (const FGuid& Id : Equipped)
			{
				if (const FEquipmentInstance* Instance = Inventory->FindEquipment(Id))
				{
					GI->EquipmentInstances.Add(Id, *Instance);
				}
			}
		}
		GI->SaveToDisk();
	}
	Super::EndPlay(EndPlayReason);
}

bool UEquipmentComponent::IsEquipChangeAllowed() const
{
	// 보스 맵 = 에키드나가 있는 레벨
	return !UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass());
}

bool UEquipmentComponent::EquipFromInventory(int32 InventoryIndex, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Inventory->IsValidIndex(InventoryIndex)) return false;

	const FInventorySlot& Slot = Inventory->GetSlot(InventoryIndex);
	const FEquipmentInstance* Instance = Slot.IsEquipment() ? Inventory->FindEquipment(Slot.EquipmentId) : nullptr;
	const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
	if (!Def || !Def->IsEquipment()) return false;

	if (!IsEquipChangeAllowed())
	{
		OutFailReason = NSLOCTEXT("Equipment", "NotInRaid", "레이드 중에는 장비를 바꿀 수 없습니다.");
		return false;
	}

	const int32 Part = static_cast<int32>(Def->EquipSlot);
	const FGuid Incoming = Slot.EquipmentId;
	const FGuid Outgoing = Equipped[Part];
	Equipped[Part] = Incoming;
	Inventory->SetSlotEquipment(InventoryIndex, Outgoing); // 있던 장비는 그 칸으로, 없었으면 칸이 빈다

	ApplyStats();
	return true;
}

bool UEquipmentComponent::Unequip(EEquipSlot Slot, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	const int32 Part = static_cast<int32>(Slot);
	if (!Inventory || !Equipped.IsValidIndex(Part) || !Equipped[Part].IsValid()) return false;

	if (!IsEquipChangeAllowed())
	{
		OutFailReason = NSLOCTEXT("Equipment", "NotInRaid", "레이드 중에는 장비를 바꿀 수 없습니다.");
		return false;
	}
	if (Inventory->PlaceEquipmentInEmptySlot(Equipped[Part]) == INDEX_NONE)
	{
		OutFailReason = NSLOCTEXT("Equipment", "BagFull", "소지품이 가득 찼습니다.");
		return false;
	}

	Equipped[Part] = FGuid();
	ApplyStats();
	return true;
}

const FEquipmentInstance* UEquipmentComponent::GetEquipped(EEquipSlot Slot) const
{
	const int32 Part = static_cast<int32>(Slot);
	const UInventoryComponent* Inventory = GetInventory();
	return (Inventory && Equipped.IsValidIndex(Part)) ? Inventory->FindEquipment(Equipped[Part]) : nullptr;
}

float UEquipmentComponent::GetItemLevel() const
{
	int32 Sum = 0;
	for (int32 i = 0; i < LoAEquipRules::SlotCount; ++i)
	{
		if (const FEquipmentInstance* Instance = GetEquipped(static_cast<EEquipSlot>(i)))
		{
			Sum += LoAItems::GetEquipmentItemLevel(*Instance);
		}
	}
	return static_cast<float>(Sum) / LoAEquipRules::SlotCount;
}

void UEquipmentComponent::GetEquipmentStats(const FEquipmentInstance& Instance, double& OutPrimary, double& OutWeapon, double& OutVitality)
{
	OutPrimary = OutWeapon = OutVitality = 0.0;
	const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
	if (!Def) return;
	const double Scale = LoAItems::GetRefineStatMultiplier(Instance.RefineLevel);
	OutPrimary = FMath::RoundToDouble(Def->PrimaryStat * Scale);
	OutWeapon = FMath::RoundToDouble(Def->WeaponPower * Scale);
	OutVitality = FMath::RoundToDouble(Def->Vitality * Scale);
}

FText UEquipmentComponent::DescribeEquipment(const FEquipmentInstance& Instance, const FEquipmentInstance* Compare)
{
	const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
	if (!Def) return FText::GetEmpty();

	// 착용 중 대비 차이 — 같으면 안 붙임
	auto Diff = [](double Mine, double Theirs) -> FString
	{
		const double D = Mine - Theirs;
		if (FMath::IsNearlyZero(D)) return FString();
		return FString::Printf(TEXT("  %s%s"), D > 0 ? TEXT("▲") : TEXT("▼"), *LoAItems::FormatWithCommas(static_cast<int64>(FMath::Abs(D))));
	};

	double P, W, V;
	GetEquipmentStats(Instance, P, W, V);
	double CP = 0.0, CW = 0.0, CV = 0.0;
	if (Compare) GetEquipmentStats(*Compare, CP, CW, CV);
	const int32 Level = LoAItems::GetEquipmentItemLevel(Instance);
	const int32 CompareLevel = Compare ? LoAItems::GetEquipmentItemLevel(*Compare) : Level;

	FString Text = FString::Printf(TEXT("%s +%d\n%s\n아이템 레벨 %d%s"),
		*Def->DisplayName.ToString(), Instance.RefineLevel,
		*StaticEnum<EEquipSlot>()->GetDisplayNameTextByValue(static_cast<int64>(Def->EquipSlot)).ToString(),
		Level, Compare ? *Diff(Level, CompareLevel) : TEXT(""));
	if (P > 0.0 || CP > 0.0) Text += FString::Printf(TEXT("\n지능 +%s%s"), *LoAItems::FormatWithCommas(static_cast<int64>(P)), Compare ? *Diff(P, CP) : TEXT(""));
	if (W > 0.0 || CW > 0.0) Text += FString::Printf(TEXT("\n무기 공격력 +%s%s"), *LoAItems::FormatWithCommas(static_cast<int64>(W)), Compare ? *Diff(W, CW) : TEXT(""));
	if (V > 0.0 || CV > 0.0) Text += FString::Printf(TEXT("\n체력 +%s%s"), *LoAItems::FormatWithCommas(static_cast<int64>(V)), Compare ? *Diff(V, CV) : TEXT(""));
	if (Compare) Text += TEXT("\n(▲▼ = 착용 중인 장비와 비교)");
	return FText::FromString(Text);
}

void UEquipmentComponent::RefreshEquipped()
{
	ApplyStats();
}

bool UEquipmentComponent::DebugSetRefine(EEquipSlot Slot, int32 RefineLevel)
{
	UInventoryComponent* Inventory = GetInventory();
	const int32 Part = static_cast<int32>(Slot);
	FEquipmentInstance* Instance = (Inventory && Equipped.IsValidIndex(Part)) ? Inventory->FindEquipmentMutable(Equipped[Part]) : nullptr;
	if (!Instance) return false;
	Instance->RefineLevel = FMath::Clamp(RefineLevel, 0, LoAEquipRules::MaxRefineLevel);
	ApplyStats();
	return true;
}

void UEquipmentComponent::ApplyStats()
{
	double Primary = 0.0, Weapon = 0.0, Vitality = 0.0;
	for (int32 i = 0; i < LoAEquipRules::SlotCount; ++i)
	{
		if (const FEquipmentInstance* Instance = GetEquipped(static_cast<EEquipSlot>(i)))
		{
			double P, W, V;
			GetEquipmentStats(*Instance, P, W, V);
			Primary += P;
			Weapon += W;
			Vitality += V;
		}
	}

	if (UStatComponent* Stats = GetOwner() ? GetOwner()->FindComponentByClass<UStatComponent>() : nullptr)
	{
		TArray<FStatModifier> Mods;
		auto Add = [&Mods](EStatModifierTarget Target, double Value)
		{
			if (Value == 0.0) return;
			FStatModifier& Mod = Mods.AddDefaulted_GetRef();
			Mod.Target = Target;
			Mod.Value = Value;
		};
		Add(EStatModifierTarget::PrimaryStat, Primary);
		Add(EStatModifierTarget::WeaponPower, Weapon);
		Add(EStatModifierTarget::Vitality, Vitality);
		Stats->SetModifiers(Equip_ModifierSource, Mods);
	}

	UE_LOG(LogTemp, Log, TEXT("[Equipment] 장착 아이템 레벨 %.2f — 장비 지능 +%.0f / 무기공격력 +%.0f"), GetItemLevel(), Primary, Weapon);
	OnEquipmentChanged.Broadcast();
}
