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
};
