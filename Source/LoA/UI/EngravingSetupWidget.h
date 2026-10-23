#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WindowDragHelper.h"
#include "EngravingSetupWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UWidget;
class UEngravingComponent;
class UInventoryComponent;

/**
 * 각인 효과 창 — P창 [각인] 탭의 [각인 효과 설정하기]로 연다. 로아 [각인 효과] 배치(알림 설정·추천 세팅·상세정보 버튼은 뺌).
 *   왼쪽: 각인 카드 목록(5열) — 이름·아이콘·◆단계, 안 배운 각인은 흐리게(각인서 1권 필요). 우클릭 = 빈 칸에 장착
 *   오른쪽: 프리셋 줄(1번만) + 각인 판 5칸 — 우클릭 = 빼기 / [저장]
 * 바꾼 내용은 [저장]을 눌러야 적용된다(로아와 같음) — 그 전엔 Pending에만 있다. 닫으면 버림
 */
UCLASS()
class LOA_API UEngravingSetupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 열 때마다 — 지금 장착 상태로 Pending을 초기화 */
	void Open(UEngravingComponent* InEngravings, UInventoryComponent* InInventory);

	bool IsCursorOverPanel() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	enum class EHit : uint8 { Card, Slot, Save, Close };
	struct FHitTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		EHit Kind = EHit::Card;
		int32 Index = INDEX_NONE;
		FName Id;
	};
	TArray<FHitTarget> Targets;

	void Refresh();
	void Notify(const FText& Message, bool bError = true) const;

	TWeakObjectPtr<UEngravingComponent> Engravings;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TArray<FName> Pending;

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UBorder> SaveButton;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> CardGrid;
	UPROPERTY(Transient) TObjectPtr<USizeBox> BoardBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	FLoAWindowDrag WindowDrag;
};
