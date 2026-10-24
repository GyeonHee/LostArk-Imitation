#include "Item/EquipmentComponent.h"
#include "Stat/ArkPassiveTypes.h"
#include "Item/AccessoryPolish.h"
#include "Item/BraceletStone.h"
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
					Instance->StatRolls.Reset();  // 전투 특성 없음 — 아크 패시브 진화 1티어가 대신
					Instance->AccessoryTier = 0;  // 최하 등급 — K·기본 체력이 이 값 기준으로 맞춰져 있다
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

		// 착용 장신구를 연마 3단계 + 중·중 효과로 (2026-10-25 유저 요청) — 저장 파일마다 한 번
		// 목걸이 적주피·추피 / 귀걸이 공퍼·무공 / 반지 치적·치피 (+ 셋째 줄 하급 공통 효과)
		if (!GI->bAccessoryPolishPresetV1)
		{
			int32 Polished = 0;
			for (int32 i = LoAEquipRules::GearSlotCount; i < LoAEquipRules::SlotCount; ++i)
			{
				if (!LoAEquipRules::IsAccessorySlot(static_cast<EEquipSlot>(i))) continue;
				if (FEquipmentInstance* Instance = Equipped[i].IsValid() ? Inventory->FindEquipmentMutable(Equipped[i]) : nullptr)
				{
					LoAPolish::ApplyPresetFullPolish(*Instance);
					++Polished;
				}
			}
			GI->bAccessoryPolishPresetV1 = true;
			UE_LOG(LogTemp, Log, TEXT("[Equipment] 착용 장신구 %d개 연마 3단계 세팅"), Polished);
		}

		// 팔찌 · 어빌리티 스톤 (2026-10-25 유저 요청) — 저장 파일마다 한 번. 팔찌 = 치명·특화 중옵 + 지능·치적·치피, 스톤 = 저받 7 / 아드 7 (77돌)
		if (!GI->bHasStarterBraceletStone)
		{
			struct FStarterExtra { EEquipSlot Slot; const TCHAR* ItemId; };
			const FStarterExtra Extras[] = { { EEquipSlot::Bracelet, TEXT("Bracelet_Starter") }, { EEquipSlot::AbilityStone, TEXT("Stone_Starter") } };
			for (const FStarterExtra& Extra : Extras)
			{
				const FGuid Id = Inventory->CreateEquipment(FName(Extra.ItemId), 0);
				FEquipmentInstance* Instance = Id.IsValid() ? Inventory->FindEquipmentMutable(Id) : nullptr;
				if (!Instance) continue;
				if (Extra.Slot == EEquipSlot::Bracelet) LoABracelet::ApplyStarterPreset(*Instance);
				else LoAStone::ApplyStarterPreset(*Instance);
				const int32 Part = static_cast<int32>(Extra.Slot);
				if (!Equipped[Part].IsValid())
				{
					Equipped[Part] = Id;
				}
				else if (Inventory->PlaceEquipmentInEmptySlot(Id) == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Equipment] 시작 %s — 소지품이 가득 차 지급 못 함"), Extra.ItemId);
				}
			}
			GI->bHasStarterBraceletStone = true;
			UE_LOG(LogTemp, Log, TEXT("[Equipment] 시작 팔찌 · 어빌리티 스톤 지급"));
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
	if (!Def) return FGearStats();
	if (Def->IsAccessory()) return LoAItems::GetAccessoryStats(Def->EquipSlot, Instance.PolishLevel);  // 장신구 = 주 스탯·체력 — 연마 단계로 오른다
	if (!LoAEquipRules::IsGearSlot(Def->EquipSlot)) return FGearStats();
	return LoAItems::GetGearStats(Def->EquipSlot, Instance.RefineLevel);
}

FText UEquipmentComponent::DescribeEquipment(const FEquipmentInstance& Instance, const FEquipmentInstance* Compare)
{
	const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
	if (!Def) return FText::GetEmpty();

	// 팔찌 — 효과 줄 + 도약 포인트
	if (Def->EquipSlot == EEquipSlot::Bracelet)
	{
		FString Text = FString::Printf(TEXT("%s\n팔찌 (유물)"), *Def->DisplayName.ToString());
		// 고정 효과(▣) 먼저, 부여 효과(◎) — 빈 부여 칸은 "(효과 부여 필요)"
		const TArray<FBraceletEffectRoll> Enchants = LoABracelet::GetEnchants(Instance);
		for (const FBraceletEffectRoll& Roll : Instance.BraceletEffects)
		{
			if (!Roll.bFixed) continue;
			const LoABracelet::FEffectDef& EffectDef = LoABracelet::GetDef(Roll.Effect);
			Text += FString::Printf(TEXT("\n  ▣ [%s] %s%s"), LoAPolish::GetGradeName(LoABracelet::GetGrade(Roll)), *LoABracelet::Describe(Roll),
				TEXT(""));
		}
		for (int32 i = 0; i < Instance.BraceletEnchantSlots; ++i)
		{
			if (!Enchants.IsValidIndex(i))
			{
				Text += TEXT("\n  ◎ (효과 부여 필요)");
				continue;
			}
			const LoABracelet::FEffectDef& EffectDef = LoABracelet::GetDef(Enchants[i].Effect);
			Text += FString::Printf(TEXT("\n  ◎ [%s] %s%s"), LoAPolish::GetGradeName(LoABracelet::GetGrade(Enchants[i])), *LoABracelet::Describe(Enchants[i]),
				TEXT(""));
		}
		Text += FString::Printf(TEXT("\n남은 효과 부여 %d회 ([팔찌 효과 부여] NPC)"), Instance.BraceletEnchantsLeft);
		Text += FString::Printf(TEXT("\n도약 포인트 +%d"), LoABracelet::LeapPointsRelic);
		if (Compare) Text += TEXT("\n(착용 중인 팔찌와 바꿔 낍니다)");
		return FText::FromString(Text);
	}

	// 어빌리티 스톤 — 세 줄(각인 2 + 감소 1)과 세공 상태
	if (Def->EquipSlot == EEquipSlot::AbilityStone)
	{
		FString Text = FString::Printf(TEXT("%s\n어빌리티 스톤 (유물)"), *Def->DisplayName.ToString());
		for (int32 i = 0; i < Instance.StoneLines.Num(); ++i)
		{
			const FStoneLine& Line = Instance.StoneLines[i];
			FString Extra;
			if (LoAStone::IsNegativeLine(i))
			{
				Extra = FString::Printf(TEXT(" — %s"), *LoAStone::DescribeNegativeEffect(Line.Id, LoAStone::GetLevel(Line.Successes, true)));
			}
			Text += FString::Printf(TEXT("\n  %s  [세공 %d/%d]%s"), *LoAStone::DescribeLine(Line, i), Line.Attempts, LoAStone::FacetsPerLine, *Extra);
		}
		const int32 TotalSuccesses = LoAStone::GetTotalSuccesses(Instance);
		Text += FString::Printf(TEXT("\n세공 성공 총 %d회 · 총 Lv.%d"), TotalSuccesses, LoAStone::GetTotalLevel(Instance));
		if (const int32 Vitality = LoAStone::GetVitalityBonus(TotalSuccesses)) Text += FString::Printf(TEXT("\n  체력 +%s"), *LoAItems::FormatWithCommas(Vitality));
		if (LoAStone::GetTotalLevel(Instance) >= LoAStone::AttackBonusLevel) Text += TEXT("\n  기본 공격력 +1.50%");
		Text += TEXT("\n각인 레벨은 장착한 각인에만 더해집니다");
		if (!LoAStone::IsComplete(Instance)) Text += TEXT("\n세공은 [어빌리티 스톤 세공사] NPC");
		return FText::FromString(Text);
	}

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
		// 주 스탯·체력 (로아 툴팁처럼 특성보다 위)
		const FGearStats MineGear = GetEquipmentStats(Instance);
		const FGearStats TheirGear = Compare ? GetEquipmentStats(*Compare) : FGearStats();
		auto GearLine = [&Text, Compare](const TCHAR* Label, double Mine, double Theirs)
		{
			Text += FString::Printf(TEXT("\n%s +%s"), Label, *LoAItems::FormatWithCommas(static_cast<int64>(Mine)));
			if (Compare && !FMath::IsNearlyEqual(Mine, Theirs))
			{
				Text += FString::Printf(TEXT("  %s%s"), Mine > Theirs ? TEXT("▲") : TEXT("▼"), *LoAItems::FormatWithCommas(static_cast<int64>(FMath::Abs(Mine - Theirs))));
			}
		};
		GearLine(TEXT("지능"), MineGear.PrimaryStat, TheirGear.PrimaryStat);
		GearLine(TEXT("체력"), MineGear.Vitality, TheirGear.Vitality);
		{
			const int32 MinePoints = LoAArkPassive::GetAccessoryEnlightenment(Instance);
			Text += FString::Printf(TEXT("\n깨달음 포인트 +%d"), MinePoints);
			if (Compare)
			{
				const int32 TheirPoints = LoAArkPassive::GetAccessoryEnlightenment(*Compare);
				if (MinePoints != TheirPoints) Text += FString::Printf(TEXT("  %s%d"), MinePoints > TheirPoints ? TEXT("▲") : TEXT("▼"), FMath::Abs(MinePoints - TheirPoints));
			}
		}

		// 연마 효과 — 단계 순서대로, 등급은 [하]/[중]/[상]
		Text += FString::Printf(TEXT("\n연마 %d / %d단계"), Instance.PolishLevel, LoAPolish::MaxLevel);
		for (const FPolishEffectRoll& Roll : Instance.PolishEffects)
		{
			const LoAPolish::FEffectDef& PolishDef = LoAPolish::GetDef(Roll.Effect);
			Text += FString::Printf(TEXT("\n  [%s] %s%s"), LoAPolish::GetGradeName(Roll.Grade), *LoAPolish::Describe(Roll), TEXT(""));
		}

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
		Text += TEXT("\n재련 불가 · 연마는 [장신구 연마] NPC");
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
	if (Def->EquipSlot != EEquipSlot::Weapon) Text += FString::Printf(TEXT("\n진화 포인트 +%d"), LoAArkPassive::ArmorEvolutionPoints);
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
			// 장신구 전투 특성은 2026-10-24부터 없음 (예전 저장 파일의 특성은 소지품을 불러올 때 지운다)
		}
	}
	// 장신구 연마 효과 + 팔찌 효과 + 어빌리티 스톤 감소 효과 (스톤 각인 레벨은 각인 컴포넌트가 읽음)
	TArray<FStatModifier> PolishMods;
	for (int32 i = LoAEquipRules::GearSlotCount; i < LoAEquipRules::SlotCount; ++i)
	{
		if (const FEquipmentInstance* Instance = GetEquipped(static_cast<EEquipSlot>(i)))
		{
			LoAPolish::AppendModifiers(*Instance, PolishMods);
		}
	}
	if (const FEquipmentInstance* Bracelet = GetEquipped(EEquipSlot::Bracelet)) LoABracelet::AppendModifiers(*Bracelet, PolishMods);
	if (const FEquipmentInstance* Stone = GetEquipped(EEquipSlot::AbilityStone)) LoAStone::AppendModifiers(*Stone, PolishMods);

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
		Mods.Append(PolishMods);
		Stats->SetModifiers(Equip_ModifierSource, Mods);
	}

	UE_LOG(LogTemp, Log, TEXT("[Equipment] 장착 아이템 레벨 %.2f — 장비 지능 +%.0f / 무기공격력 +%.0f / 체력 +%.0f / 물방 +%.0f 마방 +%.0f / 장신구 치명 %d 특화 %d 신속 %d 제압 %d"),
		GetItemLevel(), Sum.PrimaryStat, Sum.WeaponPower, Sum.Vitality, Sum.PhysDefense, Sum.MagicDefense,
		Points.Crit, Points.Specialization, Points.Swiftness, Points.Domination);
	OnEquipmentChanged.Broadcast();
}
