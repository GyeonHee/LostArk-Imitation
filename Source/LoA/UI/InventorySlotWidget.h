#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventorySlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UInventoryWidget;
struct FInventorySlot;

/**
 * 소지품 한 칸 — 아이콘 + 개수(우하단) + 테두리(잠금이면 파랑). WBP 없이 C++ 트리.
 * 좌클릭 드래그 = 위치 이동/스왑, Ctrl+우클릭 = 잠금 토글. 실제 처리는 UInventoryWidget에 넘긴다
 * 마우스를 올리거나 아이템을 끌고 지나가면 빛이 왼쪽 위 → 오른쪽 아래로 훑고 지나간다(로아 소지품) —
 *   UI 머티리얼 M_SlotShine의 Progress 파라미터를 올린다. 콘솔 LoA.SlotShine 0이면 끔
 */
UCLASS()
class LOA_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UInventoryWidget* InOwner, int32 InIndex, float InSize);
	void Refresh(const FInventorySlot& InSlot);

	int32 GetSlotIndex() const { return SlotIndex; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplyFrame();

	TWeakObjectPtr<UInventoryWidget> Owner;
	int32 SlotIndex = INDEX_NONE;
	float SlotSize = 40.f;

	bool bHasItem = false;
	bool bLocked = false;
	bool bHovered = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Frame;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountText;

	// ── 빛 훑기 ──
	void StartShine();
	void UpdateShine(float Alpha);

	float ShineElapsed = -1.f;  // < 0 = 안 움직임

	UPROPERTY(Transient)
	TObjectPtr<UImage> ShineMatImage;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> ShineMID;
};
