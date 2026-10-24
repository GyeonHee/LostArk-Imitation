#include "Stat/EngravingComponent.h"
#include "Stat/StatComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/GemComponent.h"
#include "Item/EquipmentComponent.h"
#include "Item/BraceletStone.h"
#include "Skill/SkillManagerComponent.h"
#include "LoAGameInstance.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* Eng_StarterIds[] = { TEXT("Grudge"), TEXT("Adrenaline"), TEXT("KeenBlunt"), TEXT("CursedDoll"), TEXT("HitMaster") };
}

UEngravingComponent::UEngravingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UInventoryComponent* UEngravingComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UStatComponent* UEngravingComponent::GetStats() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UStatComponent>() : nullptr;
}

void UEngravingComponent::BeginPlay()
{
	Super::BeginPlay();

	Equipped.Init(NAME_None, LoAEngravings::SlotCount);
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (GI)
	{
		for (const TPair<FName, int32>& Pair : GI->EngravingBooks)
		{
			// 예전 규칙(1권 = 배움) 저장 파일은 1권씩 빼서 단계를 그대로 유지
			const int32 Books = GI->bEngravingBooksV2 ? Pair.Value : Pair.Value - 1;
			if (LoAEngravings::Find(Pair.Key)) BooksRead.Add(Pair.Key, FMath::Clamp(Books, 0, LoAEngravings::MaxBooks));
		}
		GI->bEngravingBooksV2 = true;
		for (int32 i = 0; i < LoAEngravings::SlotCount && i < GI->EquippedEngravings.Num(); ++i)
		{
			const FName Id = GI->EquippedEngravings[i];
			if (GetLevel(Id) >= 0 && !IsEquipped(Id)) Equipped[i] = Id;
		}
		if (!GI->bHasStarterEngravings)
		{
			for (int32 i = 0; i < UE_ARRAY_COUNT(Eng_StarterIds); ++i)
			{
				const FName Id(Eng_StarterIds[i]);
				BooksRead.FindOrAdd(Id);
				if (!IsEquipped(Id) && Equipped.IsValidIndex(i) && Equipped[i].IsNone()) Equipped[i] = Id;
			}
			GI->bHasStarterEngravings = true;
			SaveToGameInstance(true);
			UE_LOG(LogTemp, Log, TEXT("[Engraving] 시작 각인 5개 (유물 0단계) 장착"));
		}
	}

	if (USkillManagerComponent* SkillManager = GetOwner() ? GetOwner()->FindComponentByClass<USkillManagerComponent>() : nullptr)
	{
		SkillManager->OnSkillUsed.AddUObject(this, &UEngravingComponent::HandleSkillUsed);
	}
	// 어빌리티 스톤을 바꾸면 스톤 각인 레벨이 바뀐다
	if (UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr)
	{
		Equipment->OnEquipmentChanged.AddUObject(this, &UEngravingComponent::HandleEquipmentChanged);
	}
	ApplyStatic();
	ApplyBuffs();
}

int32 UEngravingComponent::GetStoneLevel(FName EngravingId) const
{
	if (EngravingId.IsNone() || !IsEquipped(EngravingId)) return 0;
	const UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
	const FEquipmentInstance* Stone = Equipment ? Equipment->GetEquipped(EEquipSlot::AbilityStone) : nullptr;
	return Stone ? LoAStone::GetEngravingLevel(*Stone, EngravingId) : 0;
}

void UEngravingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveToGameInstance(true);
	Super::EndPlay(EndPlayReason);
}

void UEngravingComponent::SaveToGameInstance(bool bWriteDisk) const
{
	ULoAGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<ULoAGameInstance>() : nullptr;
	if (!GI) return;
	GI->EngravingBooks = BooksRead;
	GI->EquippedEngravings = Equipped;
	if (bWriteDisk)
	{
		// 각인서를 소지품에서 꺼냈으므로 소지품도 같이 (보석과 같은 규칙)
		if (const UInventoryComponent* Inventory = GetInventory())
		{
			if (!Inventory->WriteToGameInstance()) return;
		}
		GI->SaveToDisk();
	}
}

int32 UEngravingComponent::GetBooksRead(FName EngravingId) const
{
	const int32* Found = BooksRead.Find(EngravingId);
	return Found ? *Found : 0;
}

int32 UEngravingComponent::GetLevel(FName EngravingId) const
{
	return LoAEngravings::LevelFromBooks(GetBooksRead(EngravingId));
}

bool UEngravingComponent::EquipEngraving(int32 SlotIndex, FName EngravingId, FText& OutFailReason)
{
	if (!Equipped.IsValidIndex(SlotIndex) || !LoAEngravings::Find(EngravingId)) return false;
	if (!UGemComponent::IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Engraving", "NotHere", "레이드 중에는 각인을 바꿀 수 없습니다.");
		return false;
	}
	const int32 Existing = Equipped.IndexOfByKey(EngravingId);
	if (Existing != INDEX_NONE) Equipped[Existing] = Equipped[SlotIndex];  // 맞바꿈
	Equipped[SlotIndex] = EngravingId;
	ApplyStatic();
	ApplyBuffs();
	SaveToGameInstance(true);
	return true;
}

bool UEngravingComponent::UnequipEngraving(int32 SlotIndex, FText& OutFailReason)
{
	if (!Equipped.IsValidIndex(SlotIndex) || Equipped[SlotIndex].IsNone()) return false;
	if (!UGemComponent::IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Engraving", "NotHere", "레이드 중에는 각인을 바꿀 수 없습니다.");
		return false;
	}
	Equipped[SlotIndex] = NAME_None;
	ApplyStatic();
	ApplyBuffs();
	SaveToGameInstance(true);
	return true;
}

bool UEngravingComponent::SetAllEquipped(const TArray<FName>& NewEquipped, FText& OutFailReason)
{
	if (!UGemComponent::IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Engraving", "NotHere", "레이드 중에는 각인을 바꿀 수 없습니다.");
		return false;
	}
	TArray<FName> Result;
	Result.Init(NAME_None, LoAEngravings::SlotCount);
	for (int32 i = 0; i < LoAEngravings::SlotCount && i < NewEquipped.Num(); ++i)
	{
		const FName Id = NewEquipped[i];
		if (GetLevel(Id) >= 0 && !Result.Contains(Id)) Result[i] = Id;
	}
	Equipped = Result;
	ApplyStatic();
	ApplyBuffs();
	SaveToGameInstance(true);
	return true;
}

bool UEngravingComponent::ReadBook(FName EngravingId, FText& OutFailReason)
{
	const FEngravingDefinition* Def = LoAEngravings::Find(EngravingId);
	UInventoryComponent* Inventory = GetInventory();
	if (!Def || !Inventory) return false;
	if (!UGemComponent::IsBuildChangeAllowed(GetWorld()))
	{
		OutFailReason = NSLOCTEXT("Engraving", "NotHereRead", "레이드 중에는 각인서를 읽을 수 없습니다.");
		return false;
	}
	if (GetBooksRead(EngravingId) >= LoAEngravings::MaxBooks)
	{
		OutFailReason = NSLOCTEXT("Engraving", "Max", "이미 유물 4단계입니다.");
		return false;
	}
	if (Inventory->RemoveItem(Def->BookItemId, 1) <= 0)
	{
		OutFailReason = FText::Format(NSLOCTEXT("Engraving", "NoBook", "유물 {0} 각인서가 없습니다."), Def->Name);
		return false;
	}
	BooksRead.FindOrAdd(EngravingId) += 1;
	ApplyStatic();
	ApplyBuffs();
	SaveToGameInstance(true);
	return true;
}

void UEngravingComponent::HandleSkillUsed(int32 SlotIndex)
{
	UWorld* World = GetWorld();
	if (!World) return;
	bool bChanged = false;
	if (IsEquipped(LoAEngravings::Adrenaline))
	{
		bChanged |= AdrenalineStacks < LoAEngravings::AdrenalineMaxStacks;
		AdrenalineStacks = FMath::Min(AdrenalineStacks + 1, LoAEngravings::AdrenalineMaxStacks);
		// 쓸 때마다 지속시간이 처음부터 — 중첩은 한꺼번에 사라진다
		World->GetTimerManager().SetTimer(AdrenalineTimer, this, &UEngravingComponent::ExpireAdrenaline, LoAEngravings::AdrenalineDuration, false);
	}
	if (IsEquipped(LoAEngravings::MagickStream))
	{
		bChanged |= MagickStreamStacks < LoAEngravings::MagickStreamMaxStacks;
		MagickStreamStacks = FMath::Min(MagickStreamStacks + 1, LoAEngravings::MagickStreamMaxStacks);
		World->GetTimerManager().SetTimer(MagickStreamTimer, this, &UEngravingComponent::ExpireMagickStream, LoAEngravings::MagickStreamDuration, false);
	}
	// 에테르 포식자 — 로아는 공격하면 에테르가 떨어지고 주우면 중첩. 줍기가 없어 스킬 사용(4초에 한 번)으로 대신
	if (IsEquipped(LoAEngravings::EtherPredator) && World->GetTimeSeconds() - LastEtherGainTime >= LoAEngravings::EtherGainCooldown)
	{
		LastEtherGainTime = World->GetTimeSeconds();
		bChanged |= EtherStacks < LoAEngravings::EtherMaxStacks;
		EtherStacks = FMath::Min(EtherStacks + 1, LoAEngravings::EtherMaxStacks);
		World->GetTimerManager().SetTimer(EtherTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { EtherStacks = 0; ApplyBuffs(); }),
			LoAEngravings::EtherDuration, false);
	}
	if (bChanged) ApplyBuffs();
}

void UEngravingComponent::NotifyCounterSuccess()
{
	UWorld* World = GetWorld();
	if (!World || !IsEquipped(LoAEngravings::CrushingFist)) return;
	bCrushingFistActive = true;
	World->GetTimerManager().SetTimer(CrushingFistTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { bCrushingFistActive = false; ApplyBuffs(); }),
		LoAEngravings::CrushingFistDuration, false);
	ApplyBuffs();
}

void UEngravingComponent::NotifyMovementSkill()
{
	UWorld* World = GetWorld();
	if (!World || !IsEquipped(LoAEngravings::Propulsion)) return;
	bPropulsionActive = true;
	World->GetTimerManager().SetTimer(PropulsionTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { bPropulsionActive = false; ApplyBuffs(); }),
		LoAEngravings::PropulsionDuration, false);
	ApplyBuffs();
}

void UEngravingComponent::ExpireAdrenaline()
{
	AdrenalineStacks = 0;
	ApplyBuffs();
}

void UEngravingComponent::ExpireMagickStream()
{
	MagickStreamStacks = 0;
	ApplyBuffs();
}

void UEngravingComponent::ApplyStatic()
{
	TArray<FStatModifier> Mods;
	for (const FName& Id : Equipped)
	{
		if (!Id.IsNone()) LoAEngravings::AppendStaticModifiers(Id, FMath::Max(0, GetLevel(Id)), Mods, GetStoneLevel(Id));
	}
	if (UStatComponent* Stats = GetStats()) Stats->SetModifiers(TEXT("Engraving"), Mods);

	// 장착을 뺀 중첩 각인은 중첩도 지운다
	if (!IsEquipped(LoAEngravings::Adrenaline)) AdrenalineStacks = 0;
	if (!IsEquipped(LoAEngravings::MagickStream)) MagickStreamStacks = 0;
	if (!IsEquipped(LoAEngravings::EtherPredator)) EtherStacks = 0;
	if (!IsEquipped(LoAEngravings::CrushingFist)) bCrushingFistActive = false;
	if (!IsEquipped(LoAEngravings::Propulsion)) bPropulsionActive = false;
	OnEngravingsChanged.Broadcast();
}

void UEngravingComponent::ApplyBuffs()
{
	TArray<FStatModifier> Mods;
	if (IsEquipped(LoAEngravings::Adrenaline))
	{
		LoAEngravings::AppendStackModifiers(LoAEngravings::Adrenaline, FMath::Max(0, GetLevel(LoAEngravings::Adrenaline)), AdrenalineStacks, Mods, GetStoneLevel(LoAEngravings::Adrenaline));
	}
	if (IsEquipped(LoAEngravings::MagickStream))
	{
		LoAEngravings::AppendStackModifiers(LoAEngravings::MagickStream, FMath::Max(0, GetLevel(LoAEngravings::MagickStream)), MagickStreamStacks, Mods, GetStoneLevel(LoAEngravings::MagickStream));
	}
	auto Stack = [this, &Mods](FName Id, int32 Stacks)
	{
		if (IsEquipped(Id)) LoAEngravings::AppendStackModifiers(Id, FMath::Max(0, GetLevel(Id)), Stacks, Mods, GetStoneLevel(Id));
	};
	Stack(LoAEngravings::EtherPredator, EtherStacks);
	Stack(LoAEngravings::CrushingFist, bCrushingFistActive ? 1 : 0);
	Stack(LoAEngravings::Propulsion, bPropulsionActive ? 1 : 0);
	if (UStatComponent* Stats = GetStats()) Stats->SetModifiers(TEXT("EngravingBuff"), Mods);
}
