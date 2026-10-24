#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/ItemTypes.h"
#include "UI/WindowDragHelper.h"
#include "AccessoryPolishWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UEquipmentComponent;
class UInventoryComponent;
class UAccessoryPolishWidget;
class UGearListRowWidget;
class UVerticalBox;

/**
 * 장신구 연마 창 (연마 NPC) — 로아 [장신구 연마] 배치
 *  - 왼쪽: 소지품의 연마가 안 끝난 장신구만 (착용 중·연마 완료는 안 띄움, 유저 요청) — ◆ = 연마 단계
 *  - 가운데: 선택한 장신구 + "연마 전 ››› 1단계" + 단계별 효과 줄 3개(열린 효과는 등급 색) + 재료(영롱한 보석 파편) + 연마 비용 / 소지 금액(골드) + [장신구 연마]
 *  - 오른쪽: 부여 가능한 효과 — 그 부위 10종 × 하/중/상, 이미 붙은 종류는 흐리게(중복 불가)
 * 연마 규칙은 LoAPolish. WBP 없이 C++ 트리, 제목 줄을 끌면 창이 움직인다(FLoAWindowDrag)
 */
UCLASS()
class LOA_API UAccessoryPolishWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenPolish(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);
	void ClosePolish();

	bool IsCursorOverPanel() const;

	// 목록 줄이 부름
	void SelectAccessory(const FGuid& Id);

	UPROPERTY(EditDefaultsOnly, Category = "Polish")
	TSoftObjectPtr<UTexture2D> GoldIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Gold.T_Gold")));

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UWidget* BuildLeftColumn();
	UWidget* BuildCenterColumn();
	UWidget* BuildRightColumn();
	UWidget* MakeCostRow(const FString& Name, const FText& Label, UTextBlock*& OutValue);

	void Refresh();
	void RefreshList();
	void RefreshDetail();
	void RefreshEffectList(const FEquipmentInstance* Instance, EEquipSlot Part);
	void Bind(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);
	void Unbind();
	void SetResult(const FText& Text, const FLinearColor& Color);

	void HandleEquipmentChanged() { Refresh(); }
	void HandleInventoryChanged(int32 Index) { Refresh(); }
	void HandleGoldChanged(int64 NewAmount) { Refresh(); }

	UFUNCTION() void HandlePolishClicked();

	// ── 트리 ──
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ListBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UGearListRowWidget>> ListRows;

	UPROPERTY(Transient) TObjectPtr<UImage> DetailIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StepText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BaseText;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> StageFrames;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> StageTexts;
	UPROPERTY(Transient) TObjectPtr<UImage> ShardRing;
	UPROPERTY(Transient) TObjectPtr<UImage> ShardIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ShardCount;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostGoldText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HaveGoldText;
	UPROPERTY(Transient) TObjectPtr<UButton> PolishButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> EffectList;

	// 오른쪽 목록은 부위·연마 단계가 바뀔 때만 다시 만든다
	FGuid EffectListInstance;
	int32 EffectListLevel = INDEX_NONE;

	FGuid SelectedId;

	TWeakObjectPtr<UEquipmentComponent> Equipment;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	FDelegateHandle EquipmentChangedHandle;
	FDelegateHandle InventoryChangedHandle;
	FDelegateHandle GoldChangedHandle;
	FLoAWindowDrag WindowDrag;
};
