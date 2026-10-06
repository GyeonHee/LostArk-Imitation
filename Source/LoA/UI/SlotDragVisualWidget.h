#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlotDragVisualWidget.generated.h"

class UImage;

/**
 * HUD 슬롯(스킬·아이템·배틀아이템)을 드래그하는 동안 커서를 따라다니는 반투명 아이콘.
 * 컨트롤러가 매 틱 SetPositionInViewport로 옮긴다. WBP 없이 C++ 트리(ZoneNoticeWidget과 같은 방침)
 */
UCLASS()
class LOA_API USlotDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetIconBrush(const FSlateBrush& Brush);

	UPROPERTY(EditDefaultsOnly, Category = "SlotDrag")
	float IconSize = 56.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;
};
