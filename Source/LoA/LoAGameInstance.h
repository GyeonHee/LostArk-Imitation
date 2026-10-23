#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Item/ItemTypes.h"
#include "Item/RaidReward.h"
#include "Item/BuildTypes.h"
#include "LoAGameInstance.generated.h"

/**
 * 레벨이 바뀌어도 유지해야 하는 플레이어 데이터 보관소.
 * OpenLevel로 레벨이 바뀌면 캐릭터·컴포넌트는 전부 새로 만들어지지만 GameInstance는 게임 내내 살아 있다.
 * 스킬 배치(Q~F 슬롯)·스킬 레벨·남은 스킬 포인트 — USkillManagerComponent가 EndPlay에서 저장하고 BeginPlay에서 복원한다.
 * 사망·클리어 후 정비소 복귀 표시, 레벨 도착 페이드 인 표시 — ALoAPlayerController가 쓰고 읽는다.
 * 게임을 껐다 켜도 남아야 하는 것(스킬 배치·레벨·포인트, 최고 클리어 기록)은 ULoASaveGame으로 디스크에도 쓴다.
 */
UCLASS()
class LOA_API ULoAGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	// 한 번이라도 저장됐는지 — 저장 전(첫 실행)에는 BP 기본 배치를 그대로 쓴다
	UPROPERTY(Transient)
	bool bHasSkillLoadout = false;

	// 슬롯 0~7(Q~F)에 배정된 스킬 행 이름, 빈 슬롯은 NAME_None
	UPROPERTY(Transient)
	TArray<FName> SkillSlotRowNames;

	UPROPERTY(Transient)
	TMap<FName, int32> SkillLevels;

	UPROPERTY(Transient)
	int32 AvailableSkillPoints = 0;

	// 배틀아이템 칸 1~4 — 아이템 ID(빈 칸 NAME_None)와 칸에 든 개수
	UPROPERTY(Transient)
	bool bHasBattleItemLoadout = false;

	UPROPERTY(Transient)
	TArray<FName> BattleItemSlotIds;

	UPROPERTY(Transient)
	TArray<int32> BattleItemSlotCounts;

	// 소지품 100칸 (UInventoryComponent가 EndPlay에서 저장, BeginPlay에서 복원)
	UPROPERTY(Transient)
	bool bHasInventory = false;

	UPROPERTY(Transient)
	TArray<FInventorySlot> InventorySlots;

	// 실링(은화)
	UPROPERTY(Transient)
	bool bHasSilling = false;

	UPROPERTY(Transient)
	int64 Silling = 0;

	// 골드 (재련 비용·레이드 보상)
	UPROPERTY(Transient)
	bool bHasGold = false;

	UPROPERTY(Transient)
	int64 Gold = 0;

	// 장비 인스턴스 전부 + 부위별 장착 GUID (UInventoryComponent / UEquipmentComponent가 EndPlay에서 저장)
	UPROPERTY(Transient)
	TMap<FGuid, FEquipmentInstance> EquipmentInstances;

	UPROPERTY(Transient)
	TArray<FGuid> EquippedIds;

	UPROPERTY(Transient)
	bool bHasStarterGear = false;

	// 시작 장신구(도전자의 5종)를 받았는지 — 장신구 이전 저장 파일에도 한 번 지급
	UPROPERTY(Transient)
	bool bHasStarterAccessories = false;

	// 시작 운명의 파편을 받았는지 — 장비 성장 이전 저장 파일에도 한 번 지급
	UPROPERTY(Transient)
	bool bHasStarterFragments = false;

	// 운명의 파편 10만 추가 지급(테스트용)을 받았는지
	UPROPERTY(Transient)
	bool bHasFragmentGift = false;

	// ── 보석·각인 (7단계) — UGemComponent / UEngravingComponent가 EndPlay에서 저장 ──
	// 시작 보석(Lv5 11개)·각인(유물 0단계 5개)을 받았는지 — 7단계 이전 저장 파일에도 한 번 지급
	UPROPERTY(Transient)
	bool bHasStarterGems = false;

	UPROPERTY(Transient)
	bool bHasStarterEngravings = false;

	UPROPERTY(Transient)
	TArray<FGemSlot> GemSlots;

	// 각인별 읽은 유물 각인서 권수 (1권 = 배움·유물 0단계)
	UPROPERTY(Transient)
	TMap<FName, int32> EngravingBooks;

	// 각인 칸 5개 — 빈 칸은 NAME_None
	UPROPERTY(Transient)
	TArray<FName> EquippedEngravings;

	// 보석 합성 창에 올려 둔 재료 — 소지품에서 빠져 있으므로 저장해 두고 다음 실행 때 돌려준다
	UPROPERTY(Transient)
	TArray<FName> GemFuseInputs;

	// 장착 보석을 한 번 Lv7로 올렸는지 (시작 보석 Lv5 → Lv7, 2026-10-23)
	UPROPERTY(Transient)
	bool bGemsUpgradedToLv7 = false;

	// 각인서 권수를 새 규칙(0/20, 5권마다 +1)으로 옮겼는지 — 예전엔 1권 = 배움이라 1권씩 많았다
	UPROPERTY(Transient)
	bool bEngravingBooksV2 = false;

	// 현재 저장 구조 버전 — ULoASaveGame::SaveVersion
	// 1 = 장비, 2 = 레이드 보상(대기 보상·첫 클리어) + 실링 상한 조정 + 골드
	static constexpr int32 CurrentSaveVersion = 2;

	// 버전 1 이하 저장 파일의 실링을 이 값으로 낮춘다 — 시작 실링 1억이면 숨결(실링 구매)을 무한정 사서 재련 확률을 올릴 수 있어서
	static constexpr int64 SillingCapOnMigrate = 2000000;

	// ── 레이드 보상 (5단계) ──
	// 소지품이 가득 차 아직 못 받은 보상 — 보스 맵 클리어 직후, 대기 지역에 들어올 때 다시 넣어 본다
	UPROPERTY(Transient)
	TArray<FLoARewardItem> PendingRewards;

	UPROPERTY(Transient)
	bool bFirstClearRewarded = false;

	/** 대기 보상을 소지품·실링에 넣는다. 들어간 만큼은 OutDelivered에, 못 넣은 건 PendingRewards에 남김. 반환 = 남은 줄 수 */
	int32 DeliverPendingRewards(class UInventoryComponent* Inventory, TArray<FLoARewardItem>& OutDelivered);

	// 보스 맵에서 사망·클리어해 대기 지역으로 돌아가는 중 — 대기 지역에서 캐릭터가 빙의되면 PlayerStart 대신 정비소 위치로 옮기고 끈다
	UPROPERTY(Transient)
	bool bReturnToRepairStation = false;

	// 페이드 아웃하며 레벨을 옮겼다 — 도착한 레벨에서 검은 화면부터 페이드 인하고 끈다
	UPROPERTY(Transient)
	bool bFadeInOnArrive = false;

	// 최고 클리어 기록 (초, 0 = 없음) / 클리어 횟수
	UPROPERTY(Transient)
	float BestClearTime = 0.f;

	UPROPERTY(Transient)
	int32 ClearCount = 0;

	// 클리어 기록 — 최고 기록을 갱신했으면 true. 디스크에 바로 저장
	bool RecordClear(float ClearSeconds);

	// 스킬 배치·레벨·포인트·클리어 기록을 디스크(SaveSlotName)에 쓴다
	void SaveToDisk();

	static const FString SaveSlotName;

private:
	void LoadFromDisk();
};
