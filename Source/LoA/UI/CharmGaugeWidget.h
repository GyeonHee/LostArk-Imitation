#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharmGaugeWidget.generated.h"

class UImage;

/**
 * 매혹 게이지 머리 위 UI — 연꽃 이미지를 3등분한 조각 3개를 겹쳐서(흑백 배경 + 컬러 전경) 표시한다.
 * 스택 수만큼 컬러 전경(ColorImage1~3)의 Visibility를 켜고, 스택이 0이면 위젯 자체를 숨긴다.
 * WBP에서 ColorImage1/2/3(해당 조각의 컬러 텍스처, 위에 겹침) + 흑백 배경 이미지 3개(항상 보임, 이름 자유)
 * 총 6개의 Image 위젯을 만들고 컬러 이미지 3개만 이 이름 그대로(ColorImage1/2/3) BindWidget으로 연결하면 됨.
 */
UCLASS()
class LOA_API UCharmGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 현재 매혹 스택 수(0~3)에 맞춰 컬러 이미지 표시 개수와 위젯 전체 표시 여부를 갱신
	UFUNCTION(BlueprintCallable, Category = "CharmGauge")
	void SetStacks(int32 NewStacks);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "CharmGauge", meta = (BindWidget))
	TObjectPtr<UImage> ColorImage1;

	UPROPERTY(BlueprintReadOnly, Category = "CharmGauge", meta = (BindWidget))
	TObjectPtr<UImage> ColorImage2;

	UPROPERTY(BlueprintReadOnly, Category = "CharmGauge", meta = (BindWidget))
	TObjectPtr<UImage> ColorImage3;
};
