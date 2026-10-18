#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Item/ItemTypes.h"
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

	// 배틀아이템 칸 1~4
	UPROPERTY()
	bool bHasBattleItemLoadout = false;

	UPROPERTY()
	TArray<FName> BattleItemSlotIds;

	UPROPERTY()
	TArray<int32> BattleItemSlotCounts;
};
