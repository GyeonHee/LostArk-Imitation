#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClearWidget.generated.h"

class UTextBlock;

/**
 * 보스 처치 시 클리어 화면 — 상단 안내 + 카운트다운, 가운데 금빛 띠 + 큰 "던전 클리어", 아래 결과 줄(클리어 시간).
 * WBP 없이 C++만으로 트리를 만든다(DefeatWidget과 같은 방침). 카운트다운·레벨 이동은 ALoAPlayerController가 담당.
 */
UCLASS()
class LOA_API UClearWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTexts(const FText& Headline, const FText& ResultLine);

	// 상단 카운트다운 숫자 — "3초"
	void SetCountdown(int32 Seconds);

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FLinearColor GlowColor = FLinearColor(1.f, 0.45f, 0.05f, 0.35f);

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FLinearColor GlowCoreColor = FLinearColor(1.f, 0.7f, 0.25f, 0.55f);

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FLinearColor AccentColor = FLinearColor(1.f, 0.72f, 0.3f, 1.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadlineText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;
};
