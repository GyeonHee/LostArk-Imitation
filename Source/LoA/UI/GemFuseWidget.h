#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GemFuseWidget.generated.h"

class UBorder;
class UCanvasPanel;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UWidget;
class UGemComponent;

/**
 * 보석 합성 창 — 소지품 창 아래 띠의 보석 버튼으로 연다. 로아 [보석 합성] 배치, 소지품 창 왼쪽에 같은 높이로 붙는다(매 Tick 맞춤).
 *   위: 재료 3칸(왼쪽 위·오른쪽 위·아래) + 가운데 결과 칸
 *   등록: 창이 열린 채로 소지품의 보석을 우클릭하거나 이 창 위로 끌어다 놓기. 올린 보석은 소지품에서 빠진다(UGemComponent::FuseInputs).
 *         재료 칸 우클릭 = 소지품으로 되돌림, 창을 닫으면 전부 되돌림
 *   [합성 목표 레벨 선택] 2~9레벨: 누르는 순간 한 번 적용(목표보다 낮은 레벨 중 3개 이상 있는 가장 낮은 레벨의 보석 3개를 랜덤으로 올림)
 *         — 버튼은 누르는 동안만 밝아지고 떼면 원래대로(선택 상태를 남기지 않음)
 *   [합성하기]: 올린 3개를 1번 합성
 */
UCLASS()
class LOA_API UGemFuseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 열기 — AttachTo = 옆에 붙을 소지품 창 패널 */
	void Open(UGemComponent* InGems, UWidget* AttachTo);

	/** 닫기 — 올린 재료를 소지품으로 되돌린다 */
	void Close();

	/** 소지품 우클릭·드랍 — 재료 칸에 1개 */
	void AddInput(FName GemItemId);

	bool IsCursorOverPanel() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	enum class EHit : uint8 { Close, Input, Target, Fuse };
	struct FHitTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		EHit Kind = EHit::Close;
		int32 Index = INDEX_NONE;
	};
	TArray<FHitTarget> Targets;
	TArray<FHitTarget> InputTargets;

	void RefreshBoard();
	void Notify(const FText& Message, bool bError = true) const;
	void HandleInputsChanged() { RefreshBoard(); }
	void ReleasePressedButton();

	TWeakObjectPtr<UGemComponent> Gems;
	TWeakObjectPtr<UWidget> AttachPanel;
	FDelegateHandle InputsHandle;
	FName LastResult;  // 가운데 칸 — 방금 만든 보석
	FVector2D LastBoardSize = FVector2D::ZeroVector;

	TWeakObjectPtr<UBorder> PressedButton;  // 누르고 있는 목표 레벨 버튼 (떼면 원래 색)

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> PanelSize;
	UPROPERTY(Transient) TObjectPtr<UBorder> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Board;
	UPROPERTY(Transient) TObjectPtr<UBorder> FuseButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
};
