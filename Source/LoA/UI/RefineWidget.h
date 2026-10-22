#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/ItemTypes.h"
#include "UI/WindowDragHelper.h"
#include "RefineWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class UProgressBar;
class UScrollBox;
class USlider;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UEquipmentComponent;
class UInventoryComponent;
class URefineWidget;

/** 재련 창 왼쪽 부위 한 줄 — 아이콘 + "N 단계" + 이름(주황). 좌클릭 = 선택 */
UCLASS()
class LOA_API URefinePartRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(URefineWidget* InOwner, EEquipSlot InSlot);
	void SetPart(const FEquipmentInstance* Instance, bool bSelected);
	EEquipSlot GetPartSlot() const { return PartSlot; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void ApplyFrame();

	TWeakObjectPtr<URefineWidget> Owner;
	EEquipSlot PartSlot = EEquipSlot::MAX;
	bool bHasPart = false;
	bool bSelected = false;
	bool bHovered = false;

	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LevelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
};

/**
 * 장비 재련 창 (재련 NPC) — 로아 [장비 재련] 배치
 *  - 왼쪽: 착용 장비 6부위 목록 / 가운데: 선택한 장비 + "N 단계 ››› N+1 단계" + 장인의 기운 + 장비 성장 원(경험치 %) + [성장],
 *    재료 원 2개(돌·돌파석), 재련 비용 / 소지 금액(골드), [장비 재련] [장비 재련 10회]
 *  - 오른쪽: 재련 단계 목록(단계별 스탯, 현재 단계 강조)
 *  - 장비 성장: 단계마다 재련 전에 운명의 파편 + 실링으로 경험치를 100% 채워야 [장비 재련]이 켜진다([성장] → 성장 팝업)
 *  - [장비 재련]/[장비 재련 10회]를 누르면 재련 확인 팝업(로아 [재련] 창) — 성공률 막대(지금 / 숨결 최대), 장인의 기운(+실패 시 증가량),
 *    추가 재료 = 숨결 개수 슬라이더(− + 최대) → 팝업의 버튼으로 실제 재련. 특수 재료(책)는 넣지 않았다
 *  - [장비 재련 10회]: 재료로 2번 이상 재련할 수 있을 때만 켜짐. 성공하거나 재료가 떨어질 때까지 최대 10번 (매번 고른 숨결 개수)
 *  - 버튼을 누르면 RefineAnimDuration 동안 연출 뒤 굴린다(LoARefine) — 연출 중 창을 닫으면 아무것도 소모되지 않는다
 * WBP 없이 C++ 트리, 제목 줄을 끌면 창이 움직인다(FLoAWindowDrag)
 */
UCLASS()
class LOA_API URefineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenRefine(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);
	void CloseRefine();

	bool IsCursorOverPanel() const;

	// 부위 줄이 부름
	void SelectPart(EEquipSlot PartSlot);

	UPROPERTY(EditDefaultsOnly, Category = "Refine")
	TSoftObjectPtr<UTexture2D> GoldIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Gold.T_Gold")));

	UPROPERTY(EditDefaultsOnly, Category = "Refine")
	TSoftObjectPtr<UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

	// [장비 재련]을 누른 뒤 결과가 나오기까지 연출 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Refine")
	float RefineAnimDuration = 1.2f;

	static constexpr int32 MaxMultiRefine = 10;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	/** 재료 원 하나 — 테두리 원 + 아이콘 + 아래 "보유 / 필요" */
	struct FMaterialCell
	{
		UWidget* Root = nullptr;
		UImage* Ring = nullptr;
		UImage* Icon = nullptr;
		UTextBlock* Count = nullptr;
	};

	UWidget* BuildLeftColumn();
	UWidget* BuildCenterColumn();
	UWidget* BuildRightColumn();
	UWidget* BuildGrowthPopup();
	UWidget* BuildConfirmPopup();
	UWidget* BuildSuccessPopup();
	void ShowSuccessPopup(const FEquipmentInstance& Before, int32 Tries, int32 PlannedTries, bool bMulti);
	// [장비 재련 N회]의 N — 재료로 할 수 있는 횟수를 2~10으로 자름
	static int32 GetMultiCount(int32 Affordable) { return FMath::Clamp(Affordable, 2, MaxMultiRefine); }
	UWidget* MakeMaterialCell(const FString& Name, FMaterialCell& Out, UButton** OutButton = nullptr);
	UWidget* MakeCostRow(const FString& Name, const FText& Label, UTextBlock*& OutValue);

	void Refresh();
	void RefreshParts();
	void RefreshDetail();
	void RefreshStageList(const FEquipmentInstance* Instance);
	void RefreshGrowthPopup();
	void RefreshConfirmPopup();
	void OpenConfirm(bool bMulti);
	void SetBreathUse(int32 NewUse);
	void SetMaterialCell(FMaterialCell& Cell, UTexture2D* Texture, int64 Have, int64 Need, bool bVisible, bool bActive = true);
	void Bind(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory);
	void Unbind();
	void StartRefine(bool bMulti);
	void FinishRefineAnim();
	void Notify(const FText& Title, const FText& Message = FText::GetEmpty());
	void SetResult(const FText& Text, const FLinearColor& Color);

	void HandleEquipmentChanged() { Refresh(); }
	void HandleInventoryChanged(int32 Index) { Refresh(); }
	void HandleCurrencyChanged(int64 NewAmount) { Refresh(); }

	UFUNCTION() void HandleRefineClicked();
	UFUNCTION() void HandleRefineTenClicked();
	UFUNCTION() void HandleGrowthClicked();
	UFUNCTION() void HandleGrowthConfirm();
	UFUNCTION() void HandleGrowthClose();
	UFUNCTION() void HandleConfirmRefine();
	UFUNCTION() void HandleConfirmClose();
	UFUNCTION() void HandleBreathMinus();
	UFUNCTION() void HandleBreathPlus();
	UFUNCTION() void HandleBreathMax();
	UFUNCTION() void HandleBreathSlider(float Value);
	UFUNCTION() void HandleSuccessClose();

	// ── 트리 ──
	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TArray<TObjectPtr<URefinePartRowWidget>> PartRows;

	UPROPERTY(Transient) TObjectPtr<UImage> DetailIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LevelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EnergyText;
	UPROPERTY(Transient) TObjectPtr<UImage> GrowthRing;
	UPROPERTY(Transient) TObjectPtr<UImage> GrowthCircle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GrowthPercentText;
	UPROPERTY(Transient) TObjectPtr<UButton> GrowthButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GrowthButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WarningText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CostGoldText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HaveGoldText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> AnimBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UButton> RefineButton;
	UPROPERTY(Transient) TObjectPtr<UButton> RefineTenButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RefineTenText;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> StageList;

	UPROPERTY(Transient) TObjectPtr<UWidget> GrowthPopup;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PopupNeedText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PopupCostText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PopupHaveText;
	UPROPERTY(Transient) TObjectPtr<UButton> PopupGrowButton;

	// 재련 확인 팝업
	UPROPERTY(Transient) TObjectPtr<UWidget> ConfirmPopup;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmLevelText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ConfirmChanceBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmChanceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmEnergyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmBreathMaxText;
	UPROPERTY(Transient) TObjectPtr<UWidget> ConfirmBreathRow;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmNoBreathText;
	UPROPERTY(Transient) TObjectPtr<UImage> ConfirmBreathIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmBreathOwnedText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmBreathName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmBreathPerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmBreathCountText;
	UPROPERTY(Transient) TObjectPtr<USlider> ConfirmBreathSlider;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmButtonText;

	// 재련 성공 창 — 아이콘·이름·단계·스탯 변화(이전 ››› 이후 ▲)·"재련을 N회 시도했습니다."
	UPROPERTY(Transient) TObjectPtr<UWidget> SuccessPopup;
	UPROPERTY(Transient) TObjectPtr<UImage> SuccessIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuccessName;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuccessLevel;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> SuccessRows;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuccessTriesText;

	// 0 = 돌(수호석/파괴석), 1 = 돌파석
	TArray<FMaterialCell> MaterialCells;
	FMaterialCell PopupFragmentCell;

	// 재련 단계 목록 — 부위·단계가 바뀔 때만 다시 만든다
	FGuid StageListInstance;
	int32 StageListLevel = INDEX_NONE;

	EEquipSlot SelectedSlot = EEquipSlot::Weapon;
	// 숨결 몇 개 넣을지 — 확인 팝업에서 고르고, 성공하거나 부위를 바꾸면 0
	int32 BreathUse = 0;
	bool bConfirmMulti = false;
	int32 ConfirmMultiCount = 2;   // 확인 팝업을 열 때의 N
	int32 AnimMultiCount = 2;
	FEquipmentInstance AnimBefore;  // 재련 전 장비 — 성공 창의 "이전" 값
	bool bAnimating = false;
	bool bAnimMulti = false;
	float AnimElapsed = 0.f;
	EEquipSlot AnimSlot = EEquipSlot::MAX;
	int32 AnimBreathUse = 0;

	TWeakObjectPtr<UEquipmentComponent> Equipment;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	FDelegateHandle EquipmentChangedHandle;
	FDelegateHandle InventoryChangedHandle;
	FDelegateHandle GoldChangedHandle;
	FDelegateHandle SillingChangedHandle;
	FLoAWindowDrag WindowDrag;
};
