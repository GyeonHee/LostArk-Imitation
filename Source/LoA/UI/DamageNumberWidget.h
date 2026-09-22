#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageNumberWidget.generated.h"

class UTextBlock;

/**
 * 피격 데미지 숫자 위젯 — ADamageNumberActor의 WidgetComponent 안에 들어간다.
 * 숫자를 채우는 것 외엔 아무것도 하지 않는다(떠오르기·페이드·소멸은 전부 액터가 처리).
 *
 * WBP에서 DamageText(TextBlock)를 이 이름 그대로 만들어야 한다.
 */
UCLASS()
class LOA_API UDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DamageNumber")
	void SetDamage(float Damage);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "DamageNumber", meta = (BindWidget))
	TObjectPtr<UTextBlock> DamageText;
};
