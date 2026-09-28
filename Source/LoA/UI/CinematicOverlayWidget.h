#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicOverlayWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * 보스 맵 진입 인트로 연출용 오버레이 — 위아래 검은 띠(레터박스) + 아래쪽 보스 이름.
 * WBP 없이 C++만으로 트리를 만든다(ScreenFogWidget과 같은 방침). 애니메이션 값은 ALoAPlayerController가 Tick에서 넣어준다.
 */
UCLASS()
class LOA_API UCinematicOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTitle(const FText& Title, const FText& Subtitle);

	// 0 = 띠 없음, 1 = BarHeight만큼 완전히 내려옴
	void SetBarAmount(float Amount);

	void SetTitleOpacity(float Opacity);

	// 띠 두께 (슬레이트 단위)
	UPROPERTY(EditDefaultsOnly, Category = "Cinematic")
	float BarHeight = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Cinematic")
	FLinearColor TitleColor = FLinearColor(1.f, 0.82f, 0.55f, 1.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TopBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BottomBar;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> TitleBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleText;
};
