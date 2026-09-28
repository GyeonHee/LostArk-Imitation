#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoneLabelWidget.generated.h"

class UTextBlock;

/**
 * 대기 지역 구역(정비소·보스 입장) 위에 떠 있는 이름표.
 * WBP 없이 C++만으로 TextBlock 하나를 만든다 (ScreenFogWidget과 같은 방침) — ALobbyZoneActor의 WidgetComponent가 바로 쓴다.
 */
UCLASS()
class LOA_API UZoneLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLabel(const FText& Text, FLinearColor Color);

	UPROPERTY(EditDefaultsOnly, Category = "Label")
	int32 FontSize = 18;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;
};
