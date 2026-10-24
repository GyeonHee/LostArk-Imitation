#include "LoAGameInstance.h"
#include "LoASaveGame.h"
#include "Item/InventoryComponent.h"
#include "Algo/Reverse.h"
#include "Kismet/GameplayStatics.h"

const FString ULoAGameInstance::SaveSlotName = TEXT("LoA");

void ULoAGameInstance::Init()
{
	Super::Init();
	LoadFromDisk();
}

void ULoAGameInstance::LoadFromDisk()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Log, TEXT("[Save] 저장 파일 없음 — BP 기본 스킬 배치로 시작"));
		return;
	}

	const ULoASaveGame* Save = Cast<ULoASaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 저장 파일을 읽지 못함 — 무시하고 기본값으로 시작"));
		return;
	}

	bHasSkillLoadout = Save->bHasSkillLoadout;
	SkillSlotRowNames = Save->SkillSlotRowNames;
	SkillLevels = Save->SkillLevels;
	AvailableSkillPoints = Save->AvailableSkillPoints;
	BestClearTime = Save->BestClearTime;
	ClearCount = Save->ClearCount;
	bHasInventory = Save->bHasInventory;
	InventorySlots = Save->InventorySlots;
	bHasSilling = Save->bHasSilling;
	Silling = Save->Silling;
	bHasGold = Save->bHasGold;
	Gold = Save->Gold;
	bHasBattleItemLoadout = Save->bHasBattleItemLoadout;
	BattleItemSlotIds = Save->BattleItemSlotIds;
	BattleItemSlotCounts = Save->BattleItemSlotCounts;
	EquipmentInstances = Save->EquipmentInstances;
	EquippedIds = Save->EquippedIds;
	bHasStarterGear = Save->bHasStarterGear;
	bHasStarterAccessories = Save->bHasStarterAccessories;
	bHasStarterFragments = Save->bHasStarterFragments;
	bHasFragmentGift = Save->bHasFragmentGift;
	bHasStarterGems = Save->bHasStarterGems;
	bHasStarterEngravings = Save->bHasStarterEngravings;
	GemSlots = Save->GemSlots;
	EngravingBooks = Save->EngravingBooks;
	EquippedEngravings = Save->EquippedEngravings;
	GemFuseInputs = Save->GemFuseInputs;
	bGemsUpgradedToLv7 = Save->bGemsUpgradedToLv7;
	bEngravingBooksV2 = Save->bEngravingBooksV2;
	bHasStarterArkPassive = Save->bHasStarterArkPassive;
	ArkPassiveLevels = Save->ArkPassiveLevels;
	bArkPassiveBudgetV2 = Save->bArkPassiveBudgetV2;
	bHasStarterPolishShards = Save->bHasStarterPolishShards;
	bAccessoryPolishPresetV1 = Save->bAccessoryPolishPresetV1;
	bHasStarterBraceletStone = Save->bHasStarterBraceletStone;
	PendingRewards = Save->PendingRewards;
	bFirstClearRewarded = Save->bFirstClearRewarded;
	if (Save->SaveVersion < CurrentSaveVersion)
	{
		// 0 → 1: 장비 필드가 생김. 옮길 데이터는 없고 bHasStarterGear=false라 시작 장비가 한 번 지급된다
		// 1 → 2: 레이드 보상이 생기며 실링이 재련 비용의 제한 역할을 하도록 상한까지 낮춤 (시작 실링도 1억 → 200만)
		if (Save->SaveVersion < 2 && bHasSilling && Silling > SillingCapOnMigrate)
		{
			UE_LOG(LogTemp, Log, TEXT("[Save] 실링 %lld → %lld (저장 버전 2 조정)"), Silling, SillingCapOnMigrate);
			Silling = SillingCapOnMigrate;
		}
		UE_LOG(LogTemp, Log, TEXT("[Save] 저장 버전 %d → %d"), Save->SaveVersion, CurrentSaveVersion);
	}
	UE_LOG(LogTemp, Log, TEXT("[Save] 불러옴 — 스킬 배치 %s, 최고 기록 %.1f초, 클리어 %d회"),
		bHasSkillLoadout ? TEXT("있음") : TEXT("없음"), BestClearTime, ClearCount);
}

void ULoAGameInstance::SaveToDisk()
{
	ULoASaveGame* Save = Cast<ULoASaveGame>(UGameplayStatics::CreateSaveGameObject(ULoASaveGame::StaticClass()));
	if (!Save) return;

	Save->bHasSkillLoadout = bHasSkillLoadout;
	Save->SkillSlotRowNames = SkillSlotRowNames;
	Save->SkillLevels = SkillLevels;
	Save->AvailableSkillPoints = AvailableSkillPoints;
	Save->BestClearTime = BestClearTime;
	Save->ClearCount = ClearCount;
	Save->bHasInventory = bHasInventory;
	Save->InventorySlots = InventorySlots;
	Save->bHasSilling = bHasSilling;
	Save->Silling = Silling;
	Save->bHasGold = bHasGold;
	Save->Gold = Gold;
	Save->bHasBattleItemLoadout = bHasBattleItemLoadout;
	Save->BattleItemSlotIds = BattleItemSlotIds;
	Save->BattleItemSlotCounts = BattleItemSlotCounts;
	Save->SaveVersion = CurrentSaveVersion;
	Save->EquipmentInstances = EquipmentInstances;
	Save->EquippedIds = EquippedIds;
	Save->bHasStarterGear = bHasStarterGear;
	Save->bHasStarterAccessories = bHasStarterAccessories;
	Save->bHasStarterFragments = bHasStarterFragments;
	Save->bHasFragmentGift = bHasFragmentGift;
	Save->bHasStarterGems = bHasStarterGems;
	Save->bHasStarterEngravings = bHasStarterEngravings;
	Save->GemSlots = GemSlots;
	Save->EngravingBooks = EngravingBooks;
	Save->EquippedEngravings = EquippedEngravings;
	Save->GemFuseInputs = GemFuseInputs;
	Save->bGemsUpgradedToLv7 = bGemsUpgradedToLv7;
	Save->bEngravingBooksV2 = bEngravingBooksV2;
	Save->bHasStarterArkPassive = bHasStarterArkPassive;
	Save->ArkPassiveLevels = ArkPassiveLevels;
	Save->bArkPassiveBudgetV2 = bArkPassiveBudgetV2;
	Save->bHasStarterPolishShards = bHasStarterPolishShards;
	Save->bAccessoryPolishPresetV1 = bAccessoryPolishPresetV1;
	Save->bHasStarterBraceletStone = bHasStarterBraceletStone;
	Save->PendingRewards = PendingRewards;
	Save->bFirstClearRewarded = bFirstClearRewarded;

	if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 디스크 저장 실패"));
	}
}

int32 ULoAGameInstance::DeliverPendingRewards(UInventoryComponent* Inventory, TArray<FLoARewardItem>& OutDelivered)
{
	if (!Inventory) return PendingRewards.Num();
	for (int32 i = PendingRewards.Num() - 1; i >= 0; --i)
	{
		FLoARewardItem& Item = PendingRewards[i];
		int64 Given = 0;
		if (Item.ItemId == LoARaidReward::Silling)
		{
			Inventory->AddSilling(Item.Count);
			Given = Item.Count;
		}
		else if (Item.ItemId == LoARaidReward::Gold)
		{
			Inventory->AddGold(Item.Count);
			Given = Item.Count;
		}
		else
		{
			const int32 Try = static_cast<int32>(FMath::Min<int64>(Item.Count, MAX_int32));
			Given = Try - Inventory->AddItem(Item.ItemId, Try);
		}
		if (Given > 0) LoARaidReward::Merge(OutDelivered, { FLoARewardItem{ Item.ItemId, Given } });
		Item.Count -= Given;
		if (Item.Count <= 0) PendingRewards.RemoveAt(i);
	}
	// 위에서 뒤부터 돌았으니 표시용으로는 원래 순서로
	Algo::Reverse(OutDelivered);
	return PendingRewards.Num();
}

bool ULoAGameInstance::RecordClear(float ClearSeconds)
{
	++ClearCount;
	const bool bNewRecord = ClearSeconds > 0.f && (BestClearTime <= 0.f || ClearSeconds < BestClearTime);
	if (bNewRecord)
	{
		BestClearTime = ClearSeconds;
	}
	SaveToDisk();
	return bNewRecord;
}
