#include "Item/InventoryComponent.h"
#include "Item/BraceletStone.h"
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
	EnsureLoaded();
	OnSlotChanged.Broadcast(INDEX_NONE);
	OnSillingChanged.Broadcast(Silling);
	OnGoldChanged.Broadcast(Gold);
}

void UInventoryComponent::EnsureLoaded()
{
	if (bLoaded) return;
	bLoaded = true;

	Slots.SetNum(Capacity);

	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (GI)
	{
		Equipments = GI->EquipmentInstances;
		// 2026-10-24 — 장신구 전투 특성(치명·특화 등) 폐지. 예전 저장 파일의 특성은 여기서 지운다 (아크 패시브 진화 1티어가 대신)
		for (TPair<FGuid, FEquipmentInstance>& Pair : Equipments)
		{
			Pair.Value.StatRolls.Reset();
		}
	}
	if (GI && GI->bHasInventory)
	{
		for (int32 i = 0; i < Capacity; ++i)
		{
			Slots[i] = GI->InventorySlots.IsValidIndex(i) ? GI->InventorySlots[i] : FInventorySlot();
			// 정의가 사라진 아이템(코드에서 지움)·인스턴스를 잃은 장비는 버린다
			if (!LoAItems::Find(Slots[i].ItemId)) Slots[i] = FInventorySlot();
			if (Slots[i].IsEquipment() && !Equipments.Contains(Slots[i].EquipmentId)) Slots[i] = FInventorySlot();
			Normalize(Slots[i]);
		}

		// 최대 개수가 줄어든 아이템(보석 999 → 1, 2026-10-23) — 넘친 만큼은 빈 칸으로, 칸이 없으면 대기 보상으로
		TMap<FName, int32> Overflow;
		for (FInventorySlot& Slot : Slots)
		{
			if (Slot.IsEmpty() || Slot.IsEquipment()) continue;
			const int32 Max = GetMaxStack(Slot.ItemId);
			if (Slot.Count > Max)
			{
				Overflow.FindOrAdd(Slot.ItemId) += Slot.Count - Max;
				Slot.Count = Max;
			}
		}
		for (const TPair<FName, int32>& Pair : Overflow)
		{
			const int32 Left = AddItem(Pair.Key, Pair.Value);
			if (Left > 0) GI->PendingRewards.Add({ Pair.Key, Left });
			UE_LOG(LogTemp, Log, TEXT("[Inventory] %s 겹친 %d개를 나눔 (칸 부족 %d개는 대기 보상으로)"), *Pair.Key.ToString(), Pair.Value, Left);
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
	// 골드도 따로 플래그 — 골드가 생기기 전 저장 파일에도 처음 한 번은 지급
	Gold = (GI && GI->bHasGold) ? FMath::Max<int64>(0, GI->Gold) : FMath::Max<int64>(0, StartingGold);

	// 운명의 파편 — 장비 성장이 생기기 전 저장 파일에도 한 번
	if (GI && !GI->bHasStarterFragments)
	{
		GI->bHasStarterFragments = true;
		AddItem(LoAItems::FateFragment, StartingFateFragments);
	}
	// 영롱한 보석 파편 — 장신구 연마가 생기기 전 저장 파일에도 한 번 (장신구 하나를 3단계까지 = 60개)
	if (GI && !GI->bHasStarterPolishShards)
	{
		GI->bHasStarterPolishShards = true;
		AddItem(LoAItems::PolishShard, StartingPolishShards);
	}
	// 테스트용 추가 지급 (2026-10-22) — 저장 파일마다 한 번
	if (GI && !GI->bHasFragmentGift)
	{
		GI->bHasFragmentGift = true;
		AddItem(LoAItems::FateFragment, FragmentGiftAmount);
	}
}

FGuid UInventoryComponent::CreateEquipment(FName ItemId, int32 RefineLevel, int32 Quality)
{
	const FItemDefinition* Def = LoAItems::Find(ItemId);
	if (!Def || !Def->IsEquipment()) return FGuid();

	FEquipmentInstance Instance;
	Instance.InstanceId = FGuid::NewGuid();
	Instance.ItemId = ItemId;
	Instance.RefineLevel = FMath::Clamp(RefineLevel, 0, LoAEquipRules::MaxRefineLevel);
	Instance.Quality = FMath::Clamp(Quality, 0, 100);
	// 장신구는 만들 때 특성을 굴린다 (시작 장신구는 장비 컴포넌트가 고정값으로 덮어씀)
	if (Def->IsAccessory())
	{
		Instance.RefineLevel = 0;
		Instance.StatRolls = LoAItems::RollAccessoryStats(*Def);
		// 기본 효과(주 스탯·체력)는 연마 단계로 정해진다 — 새 장신구는 연마 0단계(로아와 같음)
	}
	else if (Def->EquipSlot == EEquipSlot::Bracelet)
	{
		Instance.RefineLevel = 0;
		LoABracelet::Roll(Instance);   // 시작 팔찌는 장비 컴포넌트가 고정 세팅으로 덮어씀
	}
	else if (Def->EquipSlot == EEquipSlot::AbilityStone)
	{
		Instance.RefineLevel = 0;
		LoAStone::Roll(Instance);      // 세공 전 — 소지품에서 우클릭해 세공
	}
	Equipments.Add(Instance.InstanceId, Instance);
	return Instance.InstanceId;
}

int32 UInventoryComponent::PlaceEquipmentInEmptySlot(const FGuid& Id)
{
	if (!Equipments.Contains(Id)) return INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].IsEmpty())
		{
			SetSlotEquipment(i, Id);
			return i;
		}
	}
	return INDEX_NONE;
}

void UInventoryComponent::SetSlotEquipment(int32 Index, const FGuid& Id)
{
	if (!Slots.IsValidIndex(Index)) return;
	FInventorySlot& Slot = Slots[Index];
	Slot = FInventorySlot();
	if (const FEquipmentInstance* Instance = Equipments.Find(Id))
	{
		Slot.ItemId = Instance->ItemId;
		Slot.Count = 1;
		Slot.EquipmentId = Id;
	}
	OnSlotChanged.Broadcast(Index);
}

void UInventoryComponent::NotifyEquipmentChanged(const FGuid& Id)
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].EquipmentId == Id) OnSlotChanged.Broadcast(i);
	}
}

void UInventoryComponent::AddSilling(int64 Amount)
{
	if (Amount <= 0) return;
	Silling = FMath::Min<int64>(Silling + Amount, MAX_int64 / 2);
	OnSillingChanged.Broadcast(Silling);
}

void UInventoryComponent::AddGold(int64 Amount)
{
	if (Amount <= 0) return;
	Gold = FMath::Min<int64>(Gold + Amount, MAX_int64 / 2);
	OnGoldChanged.Broadcast(Gold);
}

bool UInventoryComponent::SpendGold(int64 Amount)
{
	if (Amount < 0 || Gold < Amount) return false;
	Gold -= Amount;
	OnGoldChanged.Broadcast(Gold);
	return true;
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

	if (WriteToGameInstance())
	{
		GetWorld()->GetGameInstance<ULoAGameInstance>()->SaveToDisk();
	}
	Super::EndPlay(EndPlayReason);
}

bool UInventoryComponent::WriteToGameInstance() const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (!GI || !bLoaded) return false;
	if (SellReserve.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] 판매 목록에 아이템이 있어 저장 건너뜀"));
		return false;
	}
	GI->InventorySlots = Slots;
	GI->EquipmentInstances = Equipments;
	GI->bHasInventory = true;
	GI->Silling = Silling;
	GI->bHasSilling = true;
	GI->Gold = Gold;
	GI->bHasGold = true;
	return true;
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
	const FItemDefinition* Def = LoAItems::Find(ItemId);
	if (Count <= 0 || !Def) return FMath::Max(0, Count);

	// 장비 — 한 개마다 인스턴스(재련 0단계)를 만들어 빈 칸에
	if (Def->IsEquipment())
	{
		for (; Count > 0; --Count)
		{
			const FGuid Id = CreateEquipment(ItemId, 0);
			if (PlaceEquipmentInEmptySlot(Id) == INDEX_NONE)
			{
				Equipments.Remove(Id);
				break;
			}
		}
		return Count;
	}
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
	// 1) 잠기지 않은 칸 위치(앞→뒤) 수집 + 그 안의 아이템을 모음.
	//    일반 아이템은 종류별 총개수로 합치고(해시맵, O(n)), 장비는 인스턴스가 제각각이라 하나씩 따로 둔다
	TArray<int32> FreeIndices;
	FreeIndices.Reserve(Capacity);
	TMap<FName, int32> Totals;
	TArray<FGuid> EquipmentIds;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FInventorySlot& Slot = Slots[i];
		if (Slot.bLocked && !Slot.IsEmpty()) continue;
		FreeIndices.Add(i);
		if (Slot.IsEmpty()) continue;
		if (Slot.IsEquipment()) EquipmentIds.Add(Slot.EquipmentId);
		else Totals.FindOrAdd(Slot.ItemId) += Slot.Count;
	}

	// 2) 정렬 단위 — 일반 아이템 종류 하나 또는 장비 한 개
	struct FSortEntry
	{
		FName ItemId;
		int32 Count = 0;
		FGuid EquipmentId;
		int32 ItemLevel = 0;
	};
	TArray<FSortEntry> Entries;
	Entries.Reserve(Totals.Num() + EquipmentIds.Num());
	for (const TPair<FName, int32>& Pair : Totals)
	{
		Entries.Add({ Pair.Key, Pair.Value, FGuid(), 0 });
	}
	for (const FGuid& Id : EquipmentIds)
	{
		if (const FEquipmentInstance* Instance = Equipments.Find(Id))
		{
			Entries.Add({ Instance->ItemId, 1, Id, LoAItems::GetEquipmentItemLevel(*Instance) });
		}
	}

	// 카테고리 → (장비는 아이템 레벨 높은 순) → SortOrder → 이름. O(k log k)
	Entries.Sort([](const FSortEntry& L, const FSortEntry& R)
	{
		const FItemDefinition* A = LoAItems::Find(L.ItemId);
		const FItemDefinition* B = LoAItems::Find(R.ItemId);
		if (A->Category != B->Category) return static_cast<uint8>(A->Category) < static_cast<uint8>(B->Category);
		if (L.ItemLevel != R.ItemLevel) return L.ItemLevel > R.ItemLevel;
		if (A->SortOrder != B->SortOrder) return A->SortOrder < B->SortOrder;
		return L.ItemId.LexicalLess(R.ItemId);
	});

	// 3) 잠기지 않은 칸에 앞에서부터 다시 채움 (일반 아이템은 MaxStack 단위로 나눠서) — 합쳤으니 칸이 모자랄 일은 없다
	int32 Cursor = 0;
	for (const FSortEntry& Entry : Entries)
	{
		if (Entry.EquipmentId.IsValid())
		{
			if (Cursor >= FreeIndices.Num()) break;
			FInventorySlot& Slot = Slots[FreeIndices[Cursor++]];
			Slot = FInventorySlot();
			Slot.ItemId = Entry.ItemId;
			Slot.Count = 1;
			Slot.EquipmentId = Entry.EquipmentId;
			continue;
		}
		int32 Remaining = Entry.Count;
		const int32 MaxStack = GetMaxStack(Entry.ItemId);
		while (Remaining > 0 && Cursor < FreeIndices.Num())
		{
			FInventorySlot& Slot = Slots[FreeIndices[Cursor++]];
			Slot = FInventorySlot();
			Slot.ItemId = Entry.ItemId;
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

	// 장비는 인스턴스마다 따로 한 줄 — 같은 칸에 나중에 들어온 같은 종류 장비와 합치면 안 된다
	const FGuid EquipmentId = Source.EquipmentId;
	FSellReserveEntry* Entry = EquipmentId.IsValid() ? nullptr : SellReserve.FindByPredicate([&](const FSellReserveEntry& E)
	{
		return E.SourceIndex == Index && E.ItemId == Source.ItemId && !E.EquipmentId.IsValid();
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
		Entry->EquipmentId = EquipmentId;
	}

	Entry->Count += RemoveFromSlot(Index, Count);
	OnSellReserveChanged.Broadcast();
	return true;
}

void UInventoryComponent::UnreserveSell(int32 ReserveIndex)
{
	if (!SellReserve.IsValidIndex(ReserveIndex)) return;
	FSellReserveEntry& Entry = SellReserve[ReserveIndex];

	// 장비 — 원래 칸이 비었으면 거기, 아니면 첫 빈 칸
	if (Entry.EquipmentId.IsValid())
	{
		if (Slots.IsValidIndex(Entry.SourceIndex) && Slots[Entry.SourceIndex].IsEmpty())
		{
			SetSlotEquipment(Entry.SourceIndex, Entry.EquipmentId);
		}
		else if (PlaceEquipmentInEmptySlot(Entry.EquipmentId) == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Inventory] 판매 목록 되돌리기 — 소지품이 가득 차 장비가 목록에 남음"));
			return;
		}
		SellReserve.RemoveAt(ReserveIndex);
		OnSellReserveChanged.Broadcast();
		return;
	}

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
	for (const FSellReserveEntry& Entry : SellReserve)
	{
		if (Entry.EquipmentId.IsValid()) Equipments.Remove(Entry.EquipmentId);
	}
	SellReserve.Reset();
	AddSilling(Total);
	OnSellReserveChanged.Broadcast();
	return Total;
}
