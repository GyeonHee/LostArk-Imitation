#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GearListRowWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnGearRowClicked, const FGuid& /*InstanceId*/);

/**
 * 팔찌 효과 부여 · 스톤 세공 창 왼쪽 목록 한 줄 — 아이콘 + 윗줄(상태·잠금 표시) + 이름(주황) + 오른쪽 작은 글자("4회 가능")
 * 좌클릭 = 선택(OnClicked). 로아처럼 등급 색 바탕(유물 = 주황빛)
 */
UCLASS()
class LOA_API UGearListRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRow(const FGuid& InId, UTexture2D* Icon, const FText& TopLine, const FText& Name, const FText& RightText, bool bSelected);
	const FGuid& GetInstanceId() const { return InstanceId; }

	FOnGearRowClicked OnClicked;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	void ApplyFrame();

	FGuid InstanceId;
	bool bSelected = false;
	bool bHovered = false;

	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UImage> IconImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TopText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RightText;
};
