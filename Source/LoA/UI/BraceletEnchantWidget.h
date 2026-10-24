#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/ItemTypes.h"
#include "UI/WindowDragHelper.h"
#include "BraceletEnchantWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UInventoryComponent;
class UEquipmentComponent;
class UGearListRowWidget;

/**
 * 팔찌 효과 부여 창 ([팔찌 전문가] NPC) — 로아 [팔찌 효과 부여] 배치
 *  - 왼쪽: 소지품의 부여 횟수가 남은 팔찌만 (착용 중·횟수 다 쓴 건 안 띄움) — ▣ 고정 · ◎ 부여 칸, "N회 가능"
 *  - 가운데: 아이콘·이름 / 고정 효과(▣) / 부여 효과 — 줄 오른쪽 끝 자물쇠 버튼으로 잠금·해제(잠근 효과는 변환되지 않음, 무료) /
 *    효과 부여 비용·소지금(실링 24,000) / "N회 부여 가능" / [효과 부여] (이미 부여했으면 [효과 변환])
 *  - 오른쪽: 부여 가능한 효과 — [기본 효과][전투 특성][특수 효과] 탭, 효과마다 수치 범위
 *  - 변환하면 [효과 선택] 창(로아 화면 그대로): 기존 효과(파랑) / 신규 효과(금색) 상자를 눌러 고르고 [선택].
 *    신규 쪽 아래 재변환 비용(실링 60,000) + [재변환 (N회 남음)] — 신규 효과만 다시 굴림. 고르기 전에 창을 닫으면 기존 유지
 * 규칙은 LoABracelet. 부여할 때마다 즉시 저장. WBP 없이 C++ 트리, 제목 줄을 끌면 창이 움직인다
 */
UCLASS()
class LOA_API UBraceletEnchantWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenEnchant(UInventoryComponent* InInventory, UEquipmentComponent* InEquipment);
	void CloseEnchant();
	bool IsCursorOverPanel() const;

	UPROPERTY(EditDefaultsOnly, Category = "Bracelet")
	TSoftObjectPtr<UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

	UPROPERTY(EditDefaultsOnly, Category = "Bracelet")
	TSoftObjectPtr<UTexture2D> LockIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Icon_Lock.T_Icon_Lock")));

	UPROPERTY(EditDefaultsOnly, Category = "Bracelet")
	TSoftObjectPtr<UTexture2D> UnlockIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Icon_Unlock.T_Icon_Unlock")));

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
	UWidget* BuildChoicePopup();
	UWidget* MakeCostRow(const FText& Label, UTextBlock*& OutValue);

	void Refresh();
	void RefreshList();
	void RefreshEffectTab();
	void RefreshChoice();
	void SelectBracelet(const FGuid& Id);
	void SetTab(int32 Tab);
	void SetResult(const FText& Text, const FLinearColor& Color);
	void ToggleLock(int32 Index);
	void CloseChoice();
	/** 효과 줄들을 상자에 채운다 — bLockButtons면 부여 효과 줄 오른쪽 끝에 자물쇠 버튼 */
	void FillLines(UVerticalBox* Box, const TArray<FBraceletEffectRoll>& Effects, int32 SlotCount, const TArray<bool>* InLocked, bool bLockButtons);

	void HandleInventoryChanged(int32 Index) { Refresh(); }
	void HandleSillingChanged(int64 NewAmount) { Refresh(); }
	void HandleEquipmentChanged() { Refresh(); }

	UFUNCTION() void HandleEnchant();
	UFUNCTION() void HandlePickOld() { ChoicePick = 0; RefreshChoice(); }
	UFUNCTION() void HandlePickNew() { ChoicePick = 1; RefreshChoice(); }
	UFUNCTION() void HandleSelect();
	UFUNCTION() void HandleReroll();
	UFUNCTION() void HandleTab0() { SetTab(0); }
	UFUNCTION() void HandleTab1() { SetTab(1); }
	UFUNCTION() void HandleTab2() { SetTab(2); }
	UFUNCTION() void HandleLock0() { ToggleLock(0); }
	UFUNCTION() void HandleLock1() { ToggleLock(1); }
	UFUNCTION() void HandleLock2() { ToggleLock(2); }
	UFUNCTION() void HandleLock3() { ToggleLock(3); }

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ListBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UGearListRowWidget>> ListRows;
	UPROPERTY(Transient) TObjectPtr<UImage> BraceletIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BraceletName;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> FixedBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EnchantBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HaveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChancesText;
	UPROPERTY(Transient) TObjectPtr<UButton> EnchantButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EnchantButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> TabBorders;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> EffectList;

	// [효과 선택] 창
	UPROPERTY(Transient) TObjectPtr<UWidget> ChoicePopup;
	UPROPERTY(Transient) TObjectPtr<UImage> ChoiceIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChoiceName;
	UPROPERTY(Transient) TObjectPtr<UBorder> ChoiceOldFrame;
	UPROPERTY(Transient) TObjectPtr<UBorder> ChoiceNewFrame;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ChoiceOldBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ChoiceNewBox;
	UPROPERTY(Transient) TObjectPtr<UButton> RerollButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RerollText;
	UPROPERTY(Transient) TObjectPtr<UButton> SelectButton;

	FGuid SelectedId;
	TArray<bool> Locked;                         // 부여 칸별 잠금 — 팔찌를 바꾸면 초기화
	TArray<FBraceletEffectRoll> ChoiceBefore;    // [효과 선택]의 기존 효과
	TArray<FBraceletEffectRoll> PendingProposal; // [효과 선택]의 신규 효과
	bool bChoosing = false;
	int32 ChoicePick = INDEX_NONE;               // 0 = 기존, 1 = 신규
	int32 CurrentTab = 0;

	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UEquipmentComponent> Equipment;
	FDelegateHandle InventoryChangedHandle;
	FDelegateHandle SillingChangedHandle;
	FDelegateHandle EquipmentChangedHandle;
	FLoAWindowDrag WindowDrag;
};
