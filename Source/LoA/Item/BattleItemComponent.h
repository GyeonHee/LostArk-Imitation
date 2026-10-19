#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BattleItemComponent.generated.h"

class UInventoryComponent;
struct FItemDefinition;

/** 배틀아이템 칸 하나 — 인벤토리에서 옮겨 온 아이템과 개수 */
USTRUCT(BlueprintType)
struct FBattleItemSlot
{
	GENERATED_BODY()

	// 비어 있으면 NAME_None. 개수가 0이 돼도 등록은 유지된다(HUD가 빨간 반투명으로 표시)
	UPROPERTY(BlueprintReadOnly, Category = "BattleItem")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "BattleItem")
	int32 Count = 0;

	bool HasItem() const { return !ItemId.IsNone(); }
};

/**
 * 배틀아이템 칸 4개(HUD Border_1~4, 키 1~4). 처음엔 비어 있고, 인벤토리에서 드래그해 등록한다.
 * 등록하면 그 아이템을 인벤토리에서 최대 BattleEquipMax(회복약 7)개까지 꺼내 칸에 담는다 — 칸 개수와 인벤토리 개수는 별개.
 * 다 써도 칸에서 빠지지 않고 개수 0으로 남고, 정비소에서 인벤토리로부터 다시 채운다(TopUpFromInventory).
 * 칸 배치·개수는 GameInstance·SaveGame으로 유지. 쿨타임은 아이템 종류별(칸을 바꿔 껴도 유지).
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API UBattleItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBattleItemComponent();

	static constexpr int32 SlotCount = 4;

	/** 칸의 아이템 사용 — 비었거나·개수 0·쿨타임 중이거나·캐릭터가 행동불능(CC·사망·매혹 등)이면 false */
	UFUNCTION(BlueprintCallable, Category = "BattleItem")
	bool UseItem(int32 SlotIndex);

	/** 인벤토리 칸의 아이템을 배틀아이템 칸에 등록 — 배틀아이템이 아니면 false(OutFailReason).
	 *  칸에 다른 아이템이 있으면 인벤토리로 돌려보내고, 같은 아이템이 다른 칸에 있으면 그 칸은 비운다 */
	bool EquipFromInventory(int32 SlotIndex, int32 InventoryIndex, FText& OutFailReason);

	/** 칸을 비우고 남은 개수를 인벤토리로 — 인벤토리가 꽉 차서 다 못 넣으면 남은 만큼 칸에 그대로 */
	bool Unequip(int32 SlotIndex, FText& OutFailReason);

	/** 등록된 모든 칸을 인벤토리에서 최대치까지 채움 (정비소) + 쿨타임 초기화 */
	UFUNCTION(BlueprintCallable, Category = "BattleItem")
	void TopUpFromInventory();

	/** 두 칸을 통째로 맞바꾼다 — HUD 드래그 스왑 */
	UFUNCTION(BlueprintCallable, Category = "BattleItem")
	void SwapSlots(int32 A, int32 B);

	const FBattleItemSlot* GetSlot(int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex) ? &Slots[SlotIndex] : nullptr; }

	// 남은 쿨타임 비율 (1 = 방금 씀, 0 = 사용 가능) / 남은 초 — 칸에 든 아이템 기준
	UFUNCTION(BlueprintPure, Category = "BattleItem")
	float GetCooldownRatio(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "BattleItem")
	float GetCooldownRemaining(int32 SlotIndex) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FBattleItemSlot> Slots;

	// 아이템 종류별 쿨타임 끝나는 시각(월드 시간) / 길이
	TMap<FName, double> CooldownEndTimes;
	TMap<FName, float> CooldownDurations;

	UInventoryComponent* GetInventory() const;

	// 아이템 정의의 UseVFX를 캐릭터 몸에 붙여 재생 (회복약 = NS_Heal)
	void SpawnUseVFX(class ALoACharacter* Character, const FItemDefinition& Def);
};
