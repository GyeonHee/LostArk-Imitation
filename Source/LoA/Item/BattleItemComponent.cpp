#include "Item/BattleItemComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemTypes.h"
#include "LoACharacter.h"
#include "LoAGameInstance.h"
#include "Engine/World.h"

UBattleItemComponent::UBattleItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBattleItemComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(SlotCount);

	// 레벨 이동·재시작 뒤에도 칸 배치와 개수 유지
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (GI && GI->bHasBattleItemLoadout)
	{
		for (int32 i = 0; i < SlotCount; ++i)
		{
			const FName Id = GI->BattleItemSlotIds.IsValidIndex(i) ? GI->BattleItemSlotIds[i] : NAME_None;
			if (!LoAItems::Find(Id)) continue;
			Slots[i].ItemId = Id;
			Slots[i].Count = GI->BattleItemSlotCounts.IsValidIndex(i) ? FMath::Max(0, GI->BattleItemSlotCounts[i]) : 0;
		}
	}
}

void UBattleItemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr)
	{
		GI->BattleItemSlotIds.SetNum(SlotCount);
		GI->BattleItemSlotCounts.SetNum(SlotCount);
		for (int32 i = 0; i < SlotCount; ++i)
		{
			GI->BattleItemSlotIds[i] = Slots.IsValidIndex(i) ? Slots[i].ItemId : NAME_None;
			GI->BattleItemSlotCounts[i] = Slots.IsValidIndex(i) ? Slots[i].Count : 0;
		}
		GI->bHasBattleItemLoadout = true;
		GI->SaveToDisk();
	}

	Super::EndPlay(EndPlayReason);
}

UInventoryComponent* UBattleItemComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

bool UBattleItemComponent::UseItem(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return false;
	FBattleItemSlot& Slot = Slots[SlotIndex];
	const FItemDefinition* Def = LoAItems::Find(Slot.ItemId);
	if (!Def || Slot.Count <= 0) return false;
	if (GetCooldownRemaining(SlotIndex) > 0.f) return false;

	ALoACharacter* Character = Cast<ALoACharacter>(GetOwner());
	if (!Character || Character->IsActionLocked() || Character->IsCharmed()) return false;

	if (Def->HealMaxHPRatio > 0.f)
	{
		Character->Heal(Character->GetMaxHP() * Def->HealMaxHPRatio);
	}

	--Slot.Count;
	if (Def->Cooldown > 0.f)
	{
		CooldownEndTimes.Add(Slot.ItemId, GetWorld()->GetTimeSeconds() + Def->Cooldown);
		CooldownDurations.Add(Slot.ItemId, Def->Cooldown);
	}

	UE_LOG(LogTemp, Log, TEXT("[BattleItem] %s 사용 — 칸 남은 개수 %d"), *Def->DisplayName.ToString(), Slot.Count);
	return true;
}

bool UBattleItemComponent::EquipFromInventory(int32 SlotIndex, int32 InventoryIndex, FText& OutFailReason)
{
	UInventoryComponent* Inventory = GetInventory();
	if (!Slots.IsValidIndex(SlotIndex) || !Inventory || !Inventory->IsValidIndex(InventoryIndex)) return false;

	const FInventorySlot& Source = Inventory->GetSlot(InventoryIndex);
	const FItemDefinition* Def = LoAItems::Find(Source.ItemId);
	if (!Def || Source.IsEmpty()) return false;
	if (!Def->bIsBattleItem)
	{
		OutFailReason = NSLOCTEXT("BattleItem", "NotBattleItem", "배틀 아이템만 등록할 수 있습니다.");
		return false;
	}
	const FName ItemId = Source.ItemId;

	// 같은 아이템이 다른 칸에 있으면 그 칸은 비운다 (한 아이템은 한 칸에만)
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (i != SlotIndex && Slots[i].ItemId == ItemId)
		{
			FText Ignored;
			Unequip(i, Ignored);
		}
	}

	FBattleItemSlot& Slot = Slots[SlotIndex];

	// 다른 아이템이 들어 있으면 먼저 인벤토리로
	if (Slot.HasItem() && Slot.ItemId != ItemId)
	{
		if (!Unequip(SlotIndex, OutFailReason)) return false;
	}

	Slot.ItemId = ItemId;
	const int32 Need = FMath::Max(0, Def->BattleEquipMax - Slot.Count);
	Slot.Count += Inventory->RemoveFromSlot(InventoryIndex, Need);

	UE_LOG(LogTemp, Log, TEXT("[BattleItem] %d번 칸에 %s 등록 — %d개"), SlotIndex + 1, *Def->DisplayName.ToString(), Slot.Count);
	return true;
}

bool UBattleItemComponent::Unequip(int32 SlotIndex, FText& OutFailReason)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex].HasItem()) return false;
	FBattleItemSlot& Slot = Slots[SlotIndex];

	if (Slot.Count > 0)
	{
		UInventoryComponent* Inventory = GetInventory();
		const int32 Left = Inventory ? Inventory->AddItem(Slot.ItemId, Slot.Count) : Slot.Count;
		if (Left > 0)
		{
			Slot.Count = Left;
			OutFailReason = NSLOCTEXT("BattleItem", "InventoryFull", "소지품 공간이 부족합니다.");
			return false;
		}
	}

	Slot = FBattleItemSlot();
	return true;
}

void UBattleItemComponent::TopUpFromInventory()
{
	UInventoryComponent* Inventory = GetInventory();
	for (FBattleItemSlot& Slot : Slots)
	{
		const FItemDefinition* Def = LoAItems::Find(Slot.ItemId);
		if (!Def || !Inventory) continue;
		const int32 Need = Def->BattleEquipMax - Slot.Count;
		if (Need > 0)
		{
			Slot.Count += Inventory->RemoveItem(Slot.ItemId, Need);
		}
	}
	CooldownEndTimes.Reset();
	CooldownDurations.Reset();
}

void UBattleItemComponent::SwapSlots(int32 A, int32 B)
{
	if (A == B || !Slots.IsValidIndex(A) || !Slots.IsValidIndex(B)) return;
	Slots.Swap(A, B);
}

float UBattleItemComponent::GetCooldownRemaining(int32 SlotIndex) const
{
	const UWorld* World = GetWorld();
	if (!World || !Slots.IsValidIndex(SlotIndex)) return 0.f;
	const double* End = CooldownEndTimes.Find(Slots[SlotIndex].ItemId);
	return End ? FMath::Max(0.f, static_cast<float>(*End - World->GetTimeSeconds())) : 0.f;
}

float UBattleItemComponent::GetCooldownRatio(int32 SlotIndex) const
{
	if (!Slots.IsValidIndex(SlotIndex)) return 0.f;
	const float* Duration = CooldownDurations.Find(Slots[SlotIndex].ItemId);
	if (!Duration || *Duration <= 0.f) return 0.f;
	return FMath::Clamp(GetCooldownRemaining(SlotIndex) / *Duration, 0.f, 1.f);
}
