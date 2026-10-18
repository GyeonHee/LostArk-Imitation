#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lobby/ShopNPCActor.h"
#include "UI/WindowDragHelper.h"
#include "ShopWidget.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UInventoryComponent;
class UShopWidget;

/** 구매 목록 한 칸 (상점 창 아래 10칸) */
struct FShopCartSlot
{
	FName ItemId;
	int32 Count = 0;
	int64 UnitPrice = 0;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
};

/** 판매 목록 한 칸 — 아이콘 + 이름 + 가격. 우클릭 = 구매 목록에 1개, Shift+우클릭 = 묶음 구매 창 */
UCLASS()
class LOA_API UShopEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UShopWidget* InOwner, int32 InIndex);
	void SetEntry(const FShopItemEntry* Entry);
	// 판매 탭 — 아이콘 왼쪽 위에 개수, 가격 = 개수 × 판매가. ItemId가 None이면 빈 칸
	void SetSellEntry(FName ItemId, int32 Count, int64 TotalPrice);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void ApplyFrame();

	TWeakObjectPtr<UShopWidget> Owner;
	int32 EntryIndex = INDEX_NONE;
	bool bHasEntry = false;
	bool bHovered = false;

	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> IconCountText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(Transient) TObjectPtr<UImage> CoinImage;
};

/** 구매 목록 한 칸 — 아이콘 + 개수. 우클릭 = 그 칸 빼기 */
UCLASS()
class LOA_API UShopCartSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UShopWidget* InOwner, int32 InIndex, float InSize);
	void SetCartSlot(const FShopCartSlot& CartSlot);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	TWeakObjectPtr<UShopWidget> Owner;
	int32 CartIndex = INDEX_NONE;
	float SlotSize = 34.f;

	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountText;
};

/**
 * 상점 창 — 로아 상점처럼 [구매] 탭, 2열 판매 목록, 아래 구매 목록 10칸, 구매 금액 / 구매 후 잔액, [구매][비우기].
 * Shift+우클릭은 창 안에 "묶음 구매" 팝업(수량 입력 + ▲▼)을 띄운다. 헤더를 끌면 창이 움직인다(소지품과 별개).
 * WBP 없이 C++ 트리. 구매는 UInventoryComponent의 실링 차감 + AddItem
 */
UCLASS()
class LOA_API UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenShop(const TArray<FShopItemEntry>& InItems, int32 InCartSlotMax, UInventoryComponent* InInventory);
	void CloseShop();

	bool IsCursorOverPanel() const;
	void ResetPanelPlacement();

	// 자식 위젯이 부름
	void HandleEntryRightClick(int32 EntryIndex, bool bShift);
	void HandleCartSlotRightClick(int32 CartIndex);

	/** 소지품 칸 우클릭(컨트롤러 경유) — 판매 탭으로 바꾸고 그 칸을 통째로 판매 목록에. Shift면 묶음 판매 수량 선택 */
	void HandleInventoryRightClick(int32 InventoryIndex, bool bShift);

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSoftObjectPtr<UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

	static constexpr int32 CartSlotCount = 10;
	static constexpr int32 EntryColumns = 2;
	static constexpr int32 EntryRows = 6;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	enum class EShopTab : uint8 { Buy, Sell };
	enum class EPopupMode : uint8 { Buy, Sell };

	void SetTab(EShopTab Tab);
	void RefreshEntries();
	void HandleSellReserveChanged();
	UFUNCTION() void HandleBuyTabClicked();
	UFUNCTION() void HandleSellTabClicked();

	// 구매 목록에 Count개 담기 — 같은 아이템 칸부터 CartSlotMax까지 채우고 남으면 빈 칸에. 다 못 담으면 안내
	void AddToCart(int32 EntryIndex, int32 Count);
	void RefreshCart();
	void RefreshTotals();
	int64 GetCartTotal() const;

	void OpenQuantityPopup(EPopupMode Mode, int32 TargetIndex, int32 MaxQuantity);
	void CloseQuantityPopup();
	int32 GetPopupQuantity() const;
	void SetPopupQuantity(int32 Quantity);

	void Notify(const FText& Title, const FText& Message = FText::GetEmpty());
	void HandleSillingChanged(int64 NewAmount);

	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleClearClicked();
	UFUNCTION() void HandlePopupUp();
	UFUNCTION() void HandlePopupDown();
	UFUNCTION() void HandlePopupConfirm();
	UFUNCTION() void HandlePopupCancel();
	UFUNCTION() void HandlePopupTextChanged(const FText& Text);
	UFUNCTION() void HandlePopupTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UShopEntryWidget>> EntryWidgets;
	UPROPERTY(Transient) TArray<TObjectPtr<UShopCartSlotWidget>> CartWidgets;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TotalText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BalanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TotalLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BalanceLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UWidget> CategoryRow;
	UPROPERTY(Transient) TObjectPtr<UWidget> CartRowWidget;
	UPROPERTY(Transient) TObjectPtr<UButton> BuyTabButton;
	UPROPERTY(Transient) TObjectPtr<UButton> SellTabButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BuyTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SellTabText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PopupTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PopupAskText;

	UPROPERTY(Transient) TObjectPtr<UBorder> PopupLayer;
	UPROPERTY(Transient) TObjectPtr<UImage> PopupIcon;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> PopupQuantityBox;

	TArray<FShopItemEntry> Items;
	TArray<FShopCartSlot> Cart;
	int32 CartSlotMax = 99;
	int32 PopupEntryIndex = INDEX_NONE;   // 구매 = 판매 목록 줄, 판매 = 소지품 칸
	EPopupMode PopupMode = EPopupMode::Buy;
	int32 PopupMax = 99;
	EShopTab CurrentTab = EShopTab::Buy;
	FDelegateHandle SellReserveChangedHandle;
	bool bUpdatingPopupText = false;

	TWeakObjectPtr<UInventoryComponent> Inventory;
	FDelegateHandle SillingChangedHandle;
	FLoAWindowDrag WindowDrag;
};

/**
 * 상점을 연 동안 화면 아래 대화창 — NPC 이름 + 대사, 오른쪽 아래 [나가기]
 */
UCLASS()
class LOA_API UShopDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDialogue(const FText& Speaker, const FText& Line);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION() void HandleExitClicked();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeakerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LineText;
};
