#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoneNoticeWidget.generated.h"

class UTextBlock;

/**
 * 화면 상단 중앙 알림 띠 — "에키드나 2관문 / 잠시 후 다음 지역으로 이동됩니다. / 5초" 같은 3줄 표시.
 * 보스 입장 카운트다운과 정비 완료 알림이 같이 쓴다. WBP 없이 C++만으로 트리를 만든다(ScreenFogWidget과 같은 방침).
 */
UCLASS()
class LOA_API UZoneNoticeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetNotice(const FText& Title, const FText& Message);

	// 비우면 숫자 줄을 숨긴다
	void SetCounter(const FText& Counter);

	UPROPERTY(EditDefaultsOnly, Category = "Notice")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.04f, 0.72f);

	UPROPERTY(EditDefaultsOnly, Category = "Notice")
	FLinearColor TitleColor = FLinearColor(1.f, 0.8f, 0.35f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Notice")
	FLinearColor CounterColor = FLinearColor(1.f, 0.85f, 0.4f, 1.f);

	// 화면 위쪽에서 떨어진 거리 (슬레이트 단위)
	UPROPERTY(EditDefaultsOnly, Category = "Notice")
	float TopMargin = 110.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CounterText;
};
