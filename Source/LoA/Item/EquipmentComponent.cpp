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
			// 부위가 맞는 장비만 — 정의가 바뀌었거나 인스턴스를 잃었으면 비운다 (귀걸이·반지는 두 칸 다 허용)
			Equipped[i] = (Def && LoAEquipRules::CanEquipInSlot(Def->EquipSlot, static_cast<EEquipSlot>(i))) ? Id : FGuid();
		}

		// 시작 장비 — 한 번만. 장비 이전 저장 파일에도 지급(실링과 같은 방식)
		if (!GI->bHasStarterGear)
		{
			for (int32 i = 0; i < LoAEquipRules::GearSlotCount; ++i)
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

		// 시작 장신구 — 한 번만. 치명 750 + 특화 750 고정(예전 DA 치명 280 대신, K를 이 기준으로 다시 맞춤)
		if (!GI->bHasStarterAccessories)
		{
			for (int32 i = LoAEquipRules::GearSlotCount; i < LoAEquipRules::SlotCount; ++i)
			{
				const EEquipSlot Part = static_cast<EEquipSlot>(i);
				const FGuid Id = Inventory->CreateEquipment(LoAItems::GetStarterAccessory(Part), 0);
				if (!Id.IsValid()) continue;
				if (FEquipmentInstance* Instance = Inventory->FindEquipmentMutable(Id))
				{
					Instance->StatRolls = LoAItems::GetStarterAccessoryStats(Part);
				}
				if (!Equipped[i].IsValid())
				{
					Equipped[i] = Id;
				}
				else if (Inventory->PlaceEquipmentInEmptySlot(Id) == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Equipment] 시작 장신구 — 소지품이 가득 차 %d번 칸 지급 못 함"), i);
				}
			}
			GI->bHasStarterAccessories = true;
			UE_LOG(LogTemp, Log, TEXT("[Equipment] 시작 장신구 5종 지급"));
		}
	}

	ApplyStats();
}

void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 인스턴스 맵은 소지품이 저장하지만, 소지품이 먼저 EndPlay됐을 수도 있어 장착 GUID까지 확실히 디스크에 쓴다
	WriteToGameInstance();
	if (ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr)
	{
		GI->SaveToDisk();
	}
	Super::EndPlay(EndPlayReason);
}

void UEquipmentComponent::WriteToGameInstance() const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (!GI) return;
	GI->EquippedIds = Equipped;
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
}

void UEquipmentComponent::SaveNow() const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	const UInventoryComponent* Inventory = GetInventory();
	if (!GI || !Inventory) return;
	// 소지품 먼저(인스턴스 맵 통째로) → 장착 GUID. 둘이 어긋난 채 저장되면 장비가 사라지거나 둘로 늘어난다
	if (!Inventory->WriteToGameInstance()) return;
	WriteToGameInstance();
	GI->SaveToDisk();
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

	// 귀걸이·반지는 칸이 둘 — 빈 칸이 있으면 거기, 둘 다 차 있으면 1번 칸과 맞바꾼다
	int32 Part = static_cast<int32>(Def->EquipSlot);
	if (Def->EquipSlot == EEquipSlot::Earring1 || Def->EquipSlot == EEquipSlot::Ring1)
	{
		const int32 Second = Part + 1;
		if (Equipped[Part].IsValid() && !Equipped[Second].IsValid()) Part = Second;
	}
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
	// 재련 장비 6칸 평균 — 장신구는 아이템 레벨에 안 들어간다(로아와 같음)
	int32 Sum = 0;
	for (int32 i = 0; i < LoAEquipRules::GearSlotCount; ++i)
	{
		if (const FEquipmentInstance* Instance = GetEquipped(static_cast<EEquipSlot>(i)))
		{
			Sum += LoAItems::GetEquipmentItemLevel(*Instance);
		}
	}
	return static_cast<float>(Sum) / LoAEquipRules::GearSlotCount;
}

FGearStats UEquipmentComponent::GetEquipmentStats(const FEquipmentInstance& Instance)
{
	const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
	if (!Def || !LoAEquipRules::IsGearSlot(Def->EquipSlot)) return FGearStats();  // 장신구는 특성만
	return LoAItems::GetGearStats(Def->EquipSlot, Instance.RefineLevel);
}

FText UEquipmentComponent::DescribeEquipment(const FEquipmentInstance& Instance, const FEquipmentInstance* Compare)
{
	const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
	if (!Def) return FText::GetEmpty();

	// 장신구 — 이름, 부위, 전투 특성. 비교는 특성 종류별 합으로 (착용 중에 없는 종류는 그대로 ▲)
	if (Def->IsAccessory())
	{
		const UEnum* StatEnum = StaticEnum<ELoACombatStat>();
		FString Text = FString::Printf(TEXT("%s\n%s"), *Def->DisplayName.ToString(),
			*StaticEnum<EEquipSlot>()->GetDisplayNameTextByValue(static_cast<int64>(Def->EquipSlot)).ToString());
		auto SumOf = [](const FEquipmentInstance* Inst, ELoACombatStat Stat)
		{
			int32 Sum = 0;
			if (Inst)
			{
				for (const FStatRoll& R : Inst->StatRolls)
				{
					if (R.Stat == Stat) Sum += R.Value;
				}
			}
			return Sum;
		};
		TArray<ELoACombatStat> Shown;
		for (const FStatRoll& Roll : Instance.StatRolls) Shown.AddUnique(Roll.Stat);
		if (Compare)
		{
			for (const FStatRoll& Roll : Compare->StatRolls) Shown.AddUnique(Roll.Stat);
		}
		for (const ELoACombatStat Stat : Shown)
		{
			const int32 Mine = SumOf(&Instance, Stat);
			const int32 Theirs = SumOf(Compare, Stat);
			FString Line = FString::Printf(TEXT("\n%s +%d"), *StatEnum->GetDisplayNameTextByValue(static_cast<int64>(Stat)).ToString(), Mine);
			if (Compare && Mine != Theirs) Line += FString::Printf(TEXT("  %s%d"), Mine > Theirs ? TEXT("▲") : TEXT("▼"), FMath::Abs(Mine - Theirs));
			Text += Line;
		}
		Text += TEXT("\n재련 불가");
		if (Compare) Text += TEXT("\n(▲▼ = 착용 중인 장신구와 비교)");
		return FText::FromString(Text);
	}

	// 착용 중 대비 차이 — 같으면 안 붙임
	auto Diff = [](double Mine, double Theirs) -> FString
	{
		const double D = Mine - Theirs;
		if (FMath::IsNearlyZero(D)) return FString();
		return FString::Printf(TEXT("  %s%s"), D > 0 ? TEXT("▲") : TEXT("▼"), *LoAItems::FormatWithCommas(static_cast<int64>(FMath::Abs(D))));
	};

	const FGearStats Mine = GetEquipmentStats(Instance);
	const FGearStats Theirs = Compare ? GetEquipmentStats(*Compare) : FGearStats();
	const int32 Level = LoAItems::GetEquipmentItemLevel(Instance);
	const int32 CompareLevel = Compare ? LoAItems::GetEquipmentItemLevel(*Compare) : Level;

	FString Text = FString::Printf(TEXT("%s +%d\n%s\n아이템 레벨 %d%s"),
		*Def->DisplayName.ToString(), Instance.RefineLevel,
		*StaticEnum<EEquipSlot>()->GetDisplayNameTextByValue(static_cast<int64>(Def->EquipSlot)).ToString(),
		Level, Compare ? *Diff(Level, CompareLevel) : TEXT(""));
	// 로아 툴팁 순서 — 물리 방어력 · 마법 방어력 · 주 스탯 · 체력 / 무기 공격력
	auto Line = [&Text, Compare, &Diff](const TCHAR* Label, double Value, double CompareValue)
	{
		if (Value <= 0.0 && CompareValue <= 0.0) return;
		Text += FString::Printf(TEXT("\n%s +%s%s"), Label, *LoAItems::FormatWithCommas(static_cast<int64>(Value)), Compare ? *Diff(Value, CompareValue) : TEXT(""));
	};
	Line(TEXT("물리 방어력"), Mine.PhysDefense, Theirs.PhysDefense);
	Line(TEXT("마법 방어력"), Mine.MagicDefense, Theirs.MagicDefense);
	Line(TEXT("지능"), Mine.PrimaryStat, Theirs.PrimaryStat);
	Line(TEXT("체력"), Mine.Vitality, Theirs.Vitality);
	Line(TEXT("무기 공격력"), Mine.WeaponPower, Theirs.WeaponPower);
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
	if (!Instance || !LoAEquipRules::IsGearSlot(Slot)) return false;  // 장신구는 재련 없음
	Instance->RefineLevel = FMath::Clamp(RefineLevel, 0, LoAEquipRules::MaxRefineLevel);
	ApplyStats();
	return true;
}

void UEquipmentComponent::ApplyStats()
{
	FGearStats Sum;
	FCombatStatPoints Points;  // 장신구 전투 특성 합
	for (int32 i = 0; i < LoAEquipRules::SlotCount; ++i)
	{
		if (const FEquipmentInstance* Instance = GetEquipped(static_cast<EEquipSlot>(i)))
		{
			const FGearStats Gear = GetEquipmentStats(*Instance);
			Sum.PrimaryStat += Gear.PrimaryStat;
			Sum.WeaponPower += Gear.WeaponPower;
			Sum.Vitality += Gear.Vitality;
			Sum.PhysDefense += Gear.PhysDefense;
			Sum.MagicDefense += Gear.MagicDefense;
			for (const FStatRoll& Roll : Instance->StatRolls) Points.Add(Roll.Stat, Roll.Value);
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
		Add(EStatModifierTarget::PrimaryStat, Sum.PrimaryStat);
		Add(EStatModifierTarget::WeaponPower, Sum.WeaponPower);
		Add(EStatModifierTarget::Vitality, Sum.Vitality);
		Add(EStatModifierTarget::PhysDefense, Sum.PhysDefense);
		Add(EStatModifierTarget::MagicDefense, Sum.MagicDefense);
		Add(EStatModifierTarget::Crit, Points.Crit);
		Add(EStatModifierTarget::Specialization, Points.Specialization);
		Add(EStatModifierTarget::Swiftness, Points.Swiftness);
		Add(EStatModifierTarget::Domination, Points.Domination);
		Stats->SetModifiers(Equip_ModifierSource, Mods);
	}

	UE_LOG(LogTemp, Log, TEXT("[Equipment] 장착 아이템 레벨 %.2f — 장비 지능 +%.0f / 무기공격력 +%.0f / 체력 +%.0f / 물방 +%.0f 마방 +%.0f / 장신구 치명 %d 특화 %d 신속 %d 제압 %d"),
		GetItemLevel(), Sum.PrimaryStat, Sum.WeaponPower, Sum.Vitality, Sum.PhysDefense, Sum.MagicDefense,
		Points.Crit, Points.Specialization, Points.Swiftness, Points.Domination);
	OnEquipmentChanged.Broadcast();
}
