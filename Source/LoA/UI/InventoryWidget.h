#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WindowDragHelper.h"
#include "InventoryWidget.generated.h"

class UBorder;
class UInventoryComponent;
class UInventorySlotWidget;

/**
 * 소지품 창 (I키) — 화면 오른쪽, 10x10 칸 + 상단 제목 / 정렬 버튼(네모 4개) / 닫기.
 * 칸 위젯 100개는 처음 한 번만 만들고, UInventoryComponent::OnSlotChanged로 바뀐 칸만 다시 그린다.
 * 루트는 SelfHitTestInvisible — 창 밖 클릭은 게임(클릭 이동)으로, 창 안 클릭은 여기서 먹는다
 */
UCLASS()
class LOA_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetInventory(UInventoryComponent* InInventory);
	UInventoryComponent* GetInventory() const { return Inventory.Get(); }

	// 커서가 창 위에 있는지 — 컨트롤러가 클릭 이동·HUD 드래그 처리에 쓴다
	bool IsCursorOverPanel() const;

	// 창 테두리 — 보석 합성 창이 이 옆에 같은 높이로 붙는다
	UWidget* GetPanel() const;

	/** 창 위치를 지정 (CanvasSlot 앵커·정렬·위치) — 상점이 열릴 때 상점 오른쪽에 붙인다 */
	void SetPanelPlacement(FVector2D Anchor, FVector2D Alignment, FVector2D Position);

	/** 기본 위치(화면 오른쪽 가운데)로 되돌림 */
	void ResetPanelPlacement();

	// 칸 위젯이 부름
	void HandleSlotDrop(int32 From, int32 To);
	void HandleSlotCtrlRightClick(int32 Index);
	void HandleSlotRightClick(int32 Index, bool bShift);
	void HandleDragCancelled(int32 SourceIndex);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	float SlotSize = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	float SlotGap = 2.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	UFUNCTION()
	void HandleSortClicked();

	UFUNCTION()
	void HandleCloseClicked();

	// 아래 띠의 보석 합성 버튼 — UGemFuseWidget 열기/닫기
	UFUNCTION()
	void HandleGemFuseClicked();

	void HandleInventoryChanged(int32 Index);
	void HandleSillingChanged(int64 NewAmount);
	void HandleGoldChanged(int64 NewAmount);
	/** 착용 장비가 바뀜 — 장비 칸 툴팁의 ▲▼(착용 중 장비와 비교)를 다시 계산 */
	void HandleEquipmentChanged();
	void RefreshSlot(int32 Index);
	void RefreshAll();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	// 창 이동 손잡이 — 헤더(제목 줄)를 좌클릭으로 끌면 창이 따라온다 (정렬·닫기 버튼은 자기 클릭을 먼저 먹음)
	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> HeaderBox;

	FLoAWindowDrag WindowDrag;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> SlotWidgets;

	TWeakObjectPtr<UInventoryComponent> Inventory;
	FDelegateHandle InventoryChangedHandle;
	FDelegateHandle SillingChangedHandle;
	FDelegateHandle GoldChangedHandle;
	FDelegateHandle EquipmentChangedHandle;
	TWeakObjectPtr<class UEquipmentComponent> WatchedEquipment;

	// 하단 화폐 줄 — 은화 아이콘 + 실링 (세 자리 쉼표)
	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> SillingText;

	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> GoldText;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSoftObjectPtr<class UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSoftObjectPtr<class UTexture2D> GoldIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Gold.T_Gold")));
};
