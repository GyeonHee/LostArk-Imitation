#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Item/ItemTypes.h"
#include "Item/RaidReward.h"
#include "Item/BuildTypes.h"
#include "LoASaveGame.generated.h"

/**
 * 게임을 껐다 켜도 남아야 하는 데이터 — Saved/SaveGames/LoA.sav.
 * ULoAGameInstance가 Init에서 읽고, 스킬 배치 저장·클리어 기록 때마다 쓴다.
 */
UCLASS()
class LOA_API ULoASaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bHasSkillLoadout = false;

	UPROPERTY()
	TArray<FName> SkillSlotRowNames;

	UPROPERTY()
	TMap<FName, int32> SkillLevels;

	UPROPERTY()
	int32 AvailableSkillPoints = 0;

	// 최고 클리어 기록 (초, 0 = 기록 없음)
	UPROPERTY()
	float BestClearTime = 0.f;

	UPROPERTY()
	int32 ClearCount = 0;

	// 소지품 100칸
	UPROPERTY()
	bool bHasInventory = false;

	UPROPERTY()
	TArray<FInventorySlot> InventorySlots;

	// 실링(은화)
	UPROPERTY()
	bool bHasSilling = false;

	UPROPERTY()
	int64 Silling = 0;

	// 골드 (버전 2~)
	UPROPERTY()
	bool bHasGold = false;

	UPROPERTY()
	int64 Gold = 0;

	// 배틀아이템 칸 1~4
	UPROPERTY()
	bool bHasBattleItemLoadout = false;

	UPROPERTY()
	TArray<FName> BattleItemSlotIds;

	UPROPERTY()
	TArray<int32> BattleItemSlotCounts;

	// 저장 구조 버전 — 구조를 바꿀 때 올리고 LoadFromDisk에서 옛 버전을 옮겨 담는다 (0 = 장비 이전 파일)
	UPROPERTY()
	int32 SaveVersion = 0;

	// 장비 — 인스턴스 전부(소지품에 있는 것 + 장착한 것)와 부위별 장착 GUID
	UPROPERTY()
	TMap<FGuid, FEquipmentInstance> EquipmentInstances;

	UPROPERTY()
	TArray<FGuid> EquippedIds;

	// 시작 장비(1640 세트)를 받았는지 — 실링처럼 따로 플래그라 장비 이전 저장 파일에도 한 번 지급
	UPROPERTY()
	bool bHasStarterGear = false;

	// 시작 장신구를 받았는지 (6단계~)
	UPROPERTY()
	bool bHasStarterAccessories = false;

	UPROPERTY()
	bool bHasStarterFragments = false;

	UPROPERTY()
	bool bHasFragmentGift = false;

	// 보석·각인 (7단계)
	UPROPERTY()
	bool bHasStarterGems = false;

	UPROPERTY()
	bool bHasStarterEngravings = false;

	UPROPERTY()
	TArray<FGemSlot> GemSlots;

	UPROPERTY()
	TMap<FName, int32> EngravingBooks;

	UPROPERTY()
	TArray<FName> EquippedEngravings;

	// 보석 합성 창에 올려 둔 재료 — 소지품에서 빠져 있으므로 저장해 두고 다음 실행 때 돌려준다
	UPROPERTY()
	TArray<FName> GemFuseInputs;

	// 장착 보석을 한 번 Lv7로 올렸는지 (시작 보석 Lv5 → Lv7, 2026-10-23)
	UPROPERTY()
	bool bGemsUpgradedToLv7 = false;

	// 각인서 권수를 새 규칙(0/20, 5권마다 +1)으로 옮겼는지 — 예전엔 1권 = 배움이라 1권씩 많았다
	UPROPERTY()
	bool bEngravingBooksV2 = false;

	// 아크 패시브 (2026-10-24)
	UPROPERTY()
	bool bHasStarterArkPassive = false;

	UPROPERTY()
	TMap<FName, int32> ArkPassiveLevels;

	UPROPERTY()
	bool bArkPassiveBudgetV2 = false;

	// 장신구 연마 (2026-10-25)
	UPROPERTY()
	bool bHasStarterPolishShards = false;

	UPROPERTY()
	bool bAccessoryPolishPresetV1 = false;

	UPROPERTY()
	bool bHasStarterBraceletStone = false;

	// 레이드 보상 중 소지품이 가득 차 아직 못 받은 것 — 대기 지역에서 칸이 생기면 지급 (버전 2~)
	UPROPERTY()
	TArray<FLoARewardItem> PendingRewards;

	// 첫 클리어 보너스(기본 보상 ×3)를 받았는지 (버전 2~)
	UPROPERTY()
	bool bFirstClearRewarded = false;
};
