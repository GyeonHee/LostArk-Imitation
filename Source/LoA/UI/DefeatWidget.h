#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefeatWidget.generated.h"

class UTextBlock;

/**
 * 플레이어 사망 시 패배 화면 — 어두운 배경 + 가운데 붉은 띠 + "사망하였습니다." 패널 + 큰 "공략에 실패하였습니다." + 하단 사망 원인.
 * WBP 없이 C++만으로 트리를 만든다(ScreenFogWidget과 같은 방침). 흑백 화면·카운트다운·레벨 이동은 ALoAPlayerController가 담당.
 */
UCLASS()
class LOA_API UDefeatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTexts(const FText& DeathCause);

	// 패널 두 번째 줄 — "N초 후 정비소로 이동합니다."
	void SetCountdown(int32 Seconds);

	UPROPERTY(EditDefaultsOnly, Category = "Defeat")
	FLinearColor BandColor = FLinearColor(0.55f, 0.02f, 0.02f, 0.45f);

	UPROPERTY(EditDefaultsOnly, Category = "Defeat")
	FLinearColor DeathCauseColor = FLinearColor(1.f, 0.85f, 0.2f, 1.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeathCauseText;
};
