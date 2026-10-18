#include "Item/InventoryComponent.h"
#include "LoAGameInstance.h"
#include "Engine/World.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 첫 실행 — 정령의 회복약 30개
	FInventorySlot Potion;
	Potion.ItemId = LoAItems::SpiritRecoveryPotion;
	Potion.Count = 30;
	StartingItems.Add(Potion);
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(Capacity);

	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (GI && GI->bHasInventory)
	{
		for (int32 i = 0; i < Capacity; ++i)
		{
			Slots[i] = GI->InventorySlots.IsValidIndex(i) ? GI->InventorySlots[i] : FInventorySlot();
			// 정의가 사라진 아이템(코드에서 지움)은 버린다
			if (!LoAItems::Find(Slots[i].ItemId)) Slots[i] = FInventorySlot();
			Normalize(Slots[i]);
		}
	}
	else
	{
		for (const FInventorySlot& Item : StartingItems)
		{
			AddItem(Item.ItemId, Item.Count);
		}
	}
	// 실링은 따로 플래그 — 실링이 생기기 전 저장 파일(소지품만 있음)에도 처음 한 번은 지급
	Silling = (GI && GI->bHasSilling) ? FMath::Max<int64>(0, GI->Silling) : FMath::Max<int64>(0, StartingSilling);

	OnSlotChanged.Broadcast(INDEX_NONE);
	OnSillingChanged.Broadcast(Silling);
}

void UInventoryComponent::AddSilling(int64 Amount)
{
	if (Amount <= 0) return;
	Silling = FMath::Min<int64>(Silling + Amount, MAX_int64 / 2);
	OnSillingChanged.Broadcast(Silling);
}

bool UInventoryComponent::SpendSilling(int64 Amount)
{
	if (Amount < 0 || Silling < Amount) return false;
	Silling -= Amount;
	OnSillingChanged.Broadcast(Silling);
	return true;
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 판매 목록에 올려 둔 채로 끝나도(상점을 안 닫고 종료·레벨 이동) 아이템이 사라지지 않게 저장 전에 되돌린다
	UnreserveAllSell();

	if (ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr)
	{
		GI->InventorySlots = Slots;
		GI->bHasInventory = true;
		GI->Silling = Silling;
		GI->bHasSilling = true;
		GI->SaveToDisk();
	}
	Super::EndPlay(EndPlayReason);
}

const FInventorySlot& UInventoryComponent::GetSlot(int32 Index) const
{
	static const FInventorySlot Empty;
	return Slots.IsValidIndex(Index) ? Slots[Index] : Empty;
}

void UInventoryComponent::Normalize(FInventorySlot& Slot)
{
	if (Slot.IsEmpty())
	{
		Slot = FInventorySlot();
	}
}

int32 UInventoryComponent::GetMaxStack(FName ItemId)
{
	const FItemDefinition* Def = LoAItems::Find(ItemId);
	return Def ? FMath::Max(1, Def->MaxStack) : 1;
}

int32 UInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (Count <= 0 || !LoAItems::Find(ItemId)) return FMath::Max(0, Count);
	const int32 MaxStack = GetMaxStack(ItemId);

	// 1) 이미 있는 같은 아이템 칸부터 채움
	for (int32 i = 0; i < Slots.Num() && Count > 0; ++i)
	{
		FInventorySlot& Slot = Slots[i];
		if (Slot.ItemId != ItemId || Slot.Count >= MaxStack) continue;
		const int32 Add = FMath::Min(Count, MaxStack - Slot.Count);
		Slot.Count += Add;
		Count -= Add;
		OnSlotChanged.Broadcast(i);
	}

	// 2) 남으면 빈 칸에 새로
	for (int32 i = 0; i < Slots.Num() && Count > 0; ++i)
	{
		FInventorySlot& Slot = Slots[i];
		if (!Slot.IsEmpty()) continue;
		const int32 Add = FMath::Min(Count, MaxStack);
		Slot = FInventorySlot();
		Slot.ItemId = ItemId;
		Slot.Count = Add;
		Count -= Add;
		OnSlotChanged.Broadcast(i);
	}
	return Count;
}

int32 UInventoryComponent::RemoveFromSlot(int32 Index, int32 Count)
{
	if (!Slots.IsValidIndex(Index) || Count <= 0 || Slots[Index].IsEmpty()) return 0;
	FInventorySlot& Slot = Slots[Index];
	const int32 Taken = FMath::Min(Count, Slot.Count);
	Slot.Count -= Taken;
	Normalize(Slot);
	OnSlotChanged.Broadcast(Index);
	return Taken;
}

int32 UInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	int32 Taken = 0;
	// 뒤쪽 칸부터 — 앞쪽(정렬된 자리)의 묶음이 오래 남게
	for (int32 i = Slots.Num() - 1; i >= 0 && Taken < Count; --i)
	{
		if (Slots[i].ItemId == ItemId)
		{
			Taken += RemoveFromSlot(i, Count - Taken);
		}
	}
	return Taken;
}

int32 UInventoryComponent::CountItem(FName ItemId) const
{
	int32 Total = 0;
	for (const FInventorySlot& Slot : Slots)
	{
		if (Slot.ItemId == ItemId) Total += Slot.Count;
	}
	return Total;
}

bool UInventoryComponent::CanAddItems(const TMap<FName, int32>& Items) const
{
	TArray<FInventorySlot> Sim = Slots;
	for (const TPair<FName, int32>& Pair : Items)
	{
		int32 Count = Pair.Value;
		if (Count <= 0) continue;
		if (!LoAItems::Find(Pair.Key)) return false;
		const int32 MaxStack = GetMaxStack(Pair.Key);
		for (FInventorySlot& S : Sim)
		{
			if (Count <= 0) break;
			if (S.ItemId != Pair.Key || S.Count >= MaxStack) continue;
			const int32 Add = FMath::Min(Count, MaxStack - S.Count);
			S.Count += Add;
			Count -= Add;
		}
		for (FInventorySlot& S : Sim)
		{
			if (Count <= 0) break;
			if (!S.IsEmpty()) continue;
			const int32 Add = FMath::Min(Count, MaxStack);
			S.ItemId = Pair.Key;
			S.Count = Add;
			Count -= Add;
		}
		if (Count > 0) return false;
	}
	return true;
}

void UInventoryComponent::MoveOrSwap(int32 From, int32 To)
{
	if (From == To || !Slots.IsValidIndex(From) || !Slots.IsValidIndex(To)) return;
	FInventorySlot& A = Slots[From];
	FInventorySlot& B = Slots[To];
	if (A.IsEmpty()) return;

	// 같은 아이템이고 받는 쪽에 자리가 있으면 합친다 (잠금은 받는 쪽 상태 유지)
	if (A.ItemId == B.ItemId)
	{
		const int32 Room = GetMaxStack(B.ItemId) - B.Count;
		if (Room > 0)
		{
			const int32 Move = FMath::Min(Room, A.Count);
			B.Count += Move;
			A.Count -= Move;
			Normalize(A);
			OnSlotChanged.Broadcast(From);
			OnSlotChanged.Broadcast(To);
			return;
		}
	}

	Swap(A, B);
	OnSlotChanged.Broadcast(From);
	OnSlotChanged.Broadcast(To);
}

void UInventoryComponent::ToggleLock(int32 Index)
{
	if (!Slots.IsValidIndex(Index) || Slots[Index].IsEmpty()) return;
	Slots[Index].bLocked = !Slots[Index].bLocked;
	OnSlotChanged.Broadcast(Index);
}

void UInventoryComponent::SortItems()
{
	// 1) 잠기지 않은 칸 위치(앞→뒤) 수집 + 그 안의 아이템을 종류별 총개수로 합침 — 해시맵이라 합치기 O(n)
	TArray<int32> FreeIndices;
	FreeIndices.Reserve(Capacity);
	TMap<FName, int32> Totals;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FInventorySlot& Slot = Slots[i];
		if (Slot.bLocked && !Slot.IsEmpty()) continue;
		FreeIndices.Add(i);
		if (!Slot.IsEmpty())
		{
			Totals.FindOrAdd(Slot.ItemId) += Slot.Count;
		}
	}

	// 2) 종류 정렬 — 카테고리 → SortOrder → 이름. 종류 수 k개라 O(k log k)
	TArray<FName> Ids;
	Totals.GenerateKeyArray(Ids);
	Ids.Sort([](const FName& L, const FName& R)
	{
		const FItemDefinition* A = LoAItems::Find(L);
		const FItemDefinition* B = LoAItems::Find(R);
		if (A->Category != B->Category) return static_cast<uint8>(A->Category) < static_cast<uint8>(B->Category);
		if (A->SortOrder != B->SortOrder) return A->SortOrder < B->SortOrder;
		return L.LexicalLess(R);
	});

	// 3) 잠기지 않은 칸에 앞에서부터 다시 채움 (MaxStack 단위로 나눠서) — 합쳤으니 칸이 모자랄 일은 없다
	int32 Cursor = 0;
	for (const FName& Id : Ids)
	{
		int32 Remaining = Totals[Id];
		const int32 MaxStack = GetMaxStack(Id);
		while (Remaining > 0 && Cursor < FreeIndices.Num())
		{
			FInventorySlot& Slot = Slots[FreeIndices[Cursor++]];
			Slot = FInventorySlot();
			Slot.ItemId = Id;
			Slot.Count = FMath::Min(Remaining, MaxStack);
			Remaining -= Slot.Count;
		}
	}
	for (; Cursor < FreeIndices.Num(); ++Cursor)
	{
		Slots[FreeIndices[Cursor]] = FInventorySlot();
	}

	OnSlotChanged.Broadcast(INDEX_NONE);
}

void UInventoryComponent::DebugSetSlot(int32 Index, FName ItemId, int32 Count)
{
	if (!Slots.IsValidIndex(Index)) return;
	Slots[Index] = FInventorySlot();
	if (LoAItems::Find(ItemId) && Count > 0)
	{
		Slots[Index].ItemId = ItemId;
		Slots[Index].Count = FMath::Min(Count, GetMaxStack(ItemId));
	}
	OnSlotChanged.Broadcast(Index);
}

// ─── 상점 판매 목록 ─────────────────────────────────────────────────────────

bool UInventoryComponent::ReserveForSell(int32 Index, int32 Count, FText& OutFailReason)
{
	if (!Slots.IsValidIndex(Index) || Slots[Index].IsEmpty() || Count <= 0) return false;
	const FInventorySlot& Source = Slots[Index];

	if (Source.bLocked)
	{
		OutFailReason = NSLOCTEXT("Inventory", "SellLocked", "잠긴 아이템은 판매할 수 없습니다.");
		return false;
	}
	const FItemDefinition* Def = LoAItems::Find(Source.ItemId);
	if (!Def || Def->SellPrice <= 0)
	{
		OutFailReason = NSLOCTEXT("Inventory", "SellNotAllowed", "판매할 수 없는 아이템입니다.");
		return false;
	}

	FSellReserveEntry* Entry = SellReserve.FindByPredicate([&](const FSellReserveEntry& E)
	{
		return E.SourceIndex == Index && E.ItemId == Source.ItemId;
	});
	if (!Entry)
	{
		if (SellReserve.Num() >= MaxSellReserve)
		{
			OutFailReason = NSLOCTEXT("Inventory", "SellFull", "판매 목록이 가득 찼습니다.");
			return false;
		}
		Entry = &SellReserve.AddDefaulted_GetRef();
		Entry->ItemId = Source.ItemId;
		Entry->SourceIndex = Index;
	}

	Entry->Count += RemoveFromSlot(Index, Count);
	OnSellReserveChanged.Broadcast();
	return true;
}

void UInventoryComponent::UnreserveSell(int32 ReserveIndex)
{
	if (!SellReserve.IsValidIndex(ReserveIndex)) return;
	FSellReserveEntry& Entry = SellReserve[ReserveIndex];
	int32 Remaining = Entry.Count;
	const int32 MaxStack = GetMaxStack(Entry.ItemId);

	// 원래 칸으로 — 그 사이 다른 아이템이 들어왔으면 소지품 아무 데나
	if (Slots.IsValidIndex(Entry.SourceIndex))
	{
		FInventorySlot& Home = Slots[Entry.SourceIndex];
		if (Home.IsEmpty())
		{
			Home = FInventorySlot();
			Home.ItemId = Entry.ItemId;
			Home.Count = FMath::Min(Remaining, MaxStack);
			Remaining -= Home.Count;
			OnSlotChanged.Broadcast(Entry.SourceIndex);
		}
		else if (Home.ItemId == Entry.ItemId && Home.Count < MaxStack)
		{
			const int32 Add = FMath::Min(Remaining, MaxStack - Home.Count);
			Home.Count += Add;
			Remaining -= Add;
			OnSlotChanged.Broadcast(Entry.SourceIndex);
		}
	}
	if (Remaining > 0)
	{
		Remaining = AddItem(Entry.ItemId, Remaining);
	}

	if (Remaining > 0)
	{
		// 소지품이 꽉 차 다 못 돌려놓음 — 남은 만큼은 목록에 그대로
		Entry.Count = Remaining;
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] 판매 목록 되돌리기 — 소지품이 가득 차 %d개 남음"), Remaining);
	}
	else
	{
		SellReserve.RemoveAt(ReserveIndex);
	}
	OnSellReserveChanged.Broadcast();
}

void UInventoryComponent::UnreserveAllSell()
{
	for (int32 i = SellReserve.Num() - 1; i >= 0; --i)
	{
		UnreserveSell(i);
	}
}

int64 UInventoryComponent::GetSellReserveValue() const
{
	int64 Total = 0;
	for (const FSellReserveEntry& Entry : SellReserve)
	{
		if (const FItemDefinition* Def = LoAItems::Find(Entry.ItemId))
		{
			Total += Def->SellPrice * Entry.Count;
		}
	}
	return Total;
}

int64 UInventoryComponent::SellReserved()
{
	const int64 Total = GetSellReserveValue();
	if (SellReserve.Num() == 0) return 0;
	SellReserve.Reset();
	AddSilling(Total);
	OnSellReserveChanged.Broadcast();
	return Total;
}
