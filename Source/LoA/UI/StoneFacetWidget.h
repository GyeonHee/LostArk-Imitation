#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WindowDragHelper.h"
#include "StoneFacetWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UInventoryComponent;
class UEquipmentComponent;
class UGearListRowWidget;

/**
 * 어빌리티 스톤 세공 창 ([어빌리티 스톤 세공사] NPC) — 로아 [어빌리티 스톤 세공] 배치
 *  - 왼쪽: 소지품의 세공 안 됐거나 덜 된 스톤만 (착용 중·세공 완료는 안 띄움) — "세공 중", "N칸 남음"
 *  - 가운데: 스톤 아이콘·이름·소지금(실링)
 *    증가 능력(성공 확률) 줄 2개 / 감소 능력(균열 확률) 줄 1개 — 각인 원 아이콘 + "Lv.N 이름" + 성공 수 + 10칸(◆ 성공 · ◆ 실패(회색) · ◇ 남음) + 칸 아래 Lv 표시 + [세공 1,925 실링]
 *  - 아래: 세공 성공 총 N회(다음 체력 보너스 3줄) / [자동 세공 57,750] / 총 Lv.N 달성(Lv.5 >> 기본 공격력 +1.50%)
 * 규칙은 LoAStone. 세공마다 즉시 저장. WBP 없이 C++ 트리, 제목 줄을 끌면 창이 움직인다
 */
UCLASS()
class LOA_API UStoneFacetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenFacet(UInventoryComponent* InInventory, UEquipmentComponent* InEquipment);
	void CloseFacet();
	bool IsCursorOverPanel() const;

	UPROPERTY(EditDefaultsOnly, Category = "Stone")
	TSoftObjectPtr<UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	/** 세공 줄 하나의 위젯들 — 트리가 소유, 여기선 참조만 */
	struct FLineWidgets
	{
		UImage* Icon = nullptr;
		UTextBlock* IconText = nullptr;   // 감소 줄 원 안 글자 (각인 아이콘이 없을 때)
		UTextBlock* Title = nullptr;
		UTextBlock* Count = nullptr;
		TArray<UTextBlock*> Cells;
		TArray<UTextBlock*> CellLabels;
		UButton* Button = nullptr;
		UTextBlock* Cost = nullptr;
	};

	UWidget* BuildLeftColumn();
	UWidget* BuildCenterColumn();
	UWidget* BuildLine(int32 LineIndex, FLineWidgets& Out);
	UWidget* BuildSectionHeader(const FText& Title, const FLinearColor& Color, UTextBlock*& OutRight);

	void Refresh();
	void RefreshList();
	void RefreshLine(int32 LineIndex, const struct FEquipmentInstance* Stone);
	void SelectStone(const FGuid& Id);
	void DoFacet(int32 LineIndex);
	void SetResult(const FText& Text, const FLinearColor& Color);

	void HandleInventoryChanged(int32 Index) { Refresh(); }
	void HandleSillingChanged(int64 NewAmount) { Refresh(); }
	void HandleEquipmentChanged() { Refresh(); }

	UFUNCTION() void HandleFacet0() { DoFacet(0); }
	UFUNCTION() void HandleFacet1() { DoFacet(1); }
	UFUNCTION() void HandleFacet2() { DoFacet(2); }
	UFUNCTION() void HandleAuto();

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ListBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UGearListRowWidget>> ListRows;
	UPROPERTY(Transient) TObjectPtr<UImage> StoneIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StoneName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SillingText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlusChanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MinusChanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TotalSuccessText;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> BonusLines;
	UPROPERTY(Transient) TObjectPtr<UButton> AutoButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AutoCostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TotalLevelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AttackBonusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;

	TArray<FLineWidgets> Lines;
	FGuid SelectedId;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UEquipmentComponent> Equipment;
	FDelegateHandle InventoryChangedHandle;
	FDelegateHandle SillingChangedHandle;
	FDelegateHandle EquipmentChangedHandle;
	FLoAWindowDrag WindowDrag;
};
