#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/ItemTypes.h"
#include "InventoryComponent.generated.h"

/** 칸 하나가 바뀜 — Index가 INDEX_NONE이면 전부(정렬·불러오기) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventorySlotChanged, int32 /*Index*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSillingChanged, int64 /*NewAmount*/);
DECLARE_MULTICAST_DELEGATE(FOnSellReserveChanged);

/** 상점 판매 목록에 올려 둔 아이템 — 소지품 칸에서 빠져 여기 있다가 판매되거나 원래 칸으로 돌아간다 */
struct FSellReserveEntry
{
	FName ItemId;
	int32 Count = 0;
	int32 SourceIndex = INDEX_NONE;  // 원래 있던 소지품 칸
};

/**
 * 소지품 — 10x10 고정 칸. 자료구조는 **크기 100의 고정 배열**(인덱스 = 행*10 + 열 = 화면 위치).
 * - 위치가 곧 인덱스라 칸 조회·드래그 스왑·잠금 토글이 전부 O(1)
 * - 칸은 ItemId·개수·잠금만 들고, 이름·아이콘·수치는 정의 해시맵(LoAItems::Find, O(1))에서 찾는다
 * - 정렬은 잠기지 않은 칸만 모아 같은 아이템을 합친 뒤 (카테고리, SortOrder, 이름) 순으로 O(n log n) 정렬 → 잠기지 않은 칸에 앞에서부터 채움
 * - 바뀐 칸 번호만 OnSlotChanged로 알려 UI는 그 칸만 다시 그린다(매 Tick 100칸을 훑지 않음)
 * 데이터는 레벨 이동(GameInstance)과 게임 재시작(SaveGame)을 넘어 유지된다
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	static constexpr int32 Columns = 10;
	static constexpr int32 Rows = 10;
	static constexpr int32 Capacity = Columns * Rows;

	/** 저장 데이터가 없을 때(첫 실행) 넣어줄 아이템 — 칸 번호는 무시하고 AddItem으로 들어간다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlot> StartingItems;

	const FInventorySlot& GetSlot(int32 Index) const;
	bool IsValidIndex(int32 Index) const { return Slots.IsValidIndex(Index); }

	/** 같은 아이템 칸에 먼저 쌓고 남으면 빈 칸(앞에서부터)에. 반환 = 못 넣고 남은 개수 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(FName ItemId, int32 Count);

	/** 해당 칸에서 최대 Count개 꺼냄. 반환 = 실제로 꺼낸 개수 */
	int32 RemoveFromSlot(int32 Index, int32 Count);

	/** 해당 아이템을 칸 순서 뒤쪽부터 최대 Count개 꺼냄. 반환 = 실제로 꺼낸 개수 */
	int32 RemoveItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItem(FName ItemId) const;

	/** 여러 아이템을 한꺼번에 다 넣을 자리가 있는지 (상점 구매 전 확인) — 칸 복사본에 AddItem과 같은 규칙으로 채워 본다 */
	bool CanAddItems(const TMap<FName, int32>& Items) const;

	/** 드래그 앤 드랍 — 같은 아이템이면 B로 합치고(넘치는 만큼은 A에 남음), 아니면 칸 통째로(잠금 포함) 맞바꾼다 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void MoveOrSwap(int32 From, int32 To);

	/** Ctrl+우클릭 — 아이템이 있는 칸만 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleLock(int32 Index);

	/** 정렬 — 잠긴 칸은 그 자리 그대로, 나머지는 합쳐서 위(앞 칸)부터 순서대로 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SortItems();

	/** 테스트용 — 칸에 직접 넣는다(쌓기 규칙 무시) */
	void DebugSetSlot(int32 Index, FName ItemId, int32 Count);

	FOnInventorySlotChanged OnSlotChanged;

	// ── 실링(은화) — 칸을 차지하지 않는 화폐. 나중에 상점에서 회복약 등을 산다 ──
	/** 저장 데이터에 실링이 없을 때(첫 실행) 주는 양 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Currency")
	int64 StartingSilling = 100000000;

	UFUNCTION(BlueprintPure, Category = "Inventory|Currency")
	int64 GetSilling() const { return Silling; }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Currency")
	void AddSilling(int64 Amount);

	/** 모자라면 아무것도 안 빼고 false — 상점 구매용 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Currency")
	bool SpendSilling(int64 Amount);

	FOnSillingChanged OnSillingChanged;

	// ── 상점 판매 목록 — 올리면 소지품에서 빠지고, 비우기·상점 닫기·EndPlay(저장 전)에 원래 칸으로 돌아간다 ──
	static constexpr int32 MaxSellReserve = 12;

	/** Index 칸에서 Count개를 판매 목록으로 — 잠금·판매가 0·목록 가득이면 false(OutFailReason). 같은 칸에서 또 올리면 합친다 */
	bool ReserveForSell(int32 Index, int32 Count, FText& OutFailReason);

	/** 판매 목록 한 줄을 소지품으로 되돌림 (원래 칸이 비었거나 같은 아이템이면 거기로, 아니면 빈 칸에) */
	void UnreserveSell(int32 ReserveIndex);
	void UnreserveAllSell();

	const TArray<FSellReserveEntry>& GetSellReserve() const { return SellReserve; }
	int64 GetSellReserveValue() const;

	/** 판매 목록 전부 판매 — 실링 지급 후 목록 비움. 반환 = 받은 실링 */
	int64 SellReserved();

	FOnSellReserveChanged OnSellReserveChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FInventorySlot> Slots;

	int64 Silling = 0;

	TArray<FSellReserveEntry> SellReserve;

	// 비워진 칸 정리 — 개수 0이면 아이템·잠금까지 지운다
	static void Normalize(FInventorySlot& Slot);
	static int32 GetMaxStack(FName ItemId);
};
