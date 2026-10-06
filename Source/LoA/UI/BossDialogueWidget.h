#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossDialogueWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;

/**
 * 보스 대사창 — 화면 아래쪽 가운데에 초상화 + 이름(붉은 글씨) + 대사 여러 줄. 로아의 보스 대사 연출과 같은 모양.
 * 큰 패턴 시작 때 컨트롤러의 ShowBossDialogue()로 띄운다. WBP 없이 C++만으로 트리를 만든다(ZoneNoticeWidget과 같은 방침).
 * 나타날 때/사라질 때 FadeTime 동안 서서히 페이드.
 */
UCLASS()
class LOA_API UBossDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDialogue(const FText& Speaker, const FText& Line);

	// 초상화 교체 — nullptr이면 초상화를 숨긴다 (초상화 에셋이 없는 화자)
	void SetPortrait(UTexture2D* Texture);

	// 페이드 인 시작 / 페이드 아웃 후 스스로 화면에서 내린다
	void FadeIn();
	void FadeOut();

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	FLinearColor BackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.62f);

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	FLinearColor SpeakerColor = FLinearColor(0.95f, 0.25f, 0.25f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	FLinearColor LineColor = FLinearColor(0.92f, 0.92f, 0.92f, 1.f);

	// 화면 아래쪽에서 띄우는 거리 — 스킬 슬롯(HUD) 위에 오도록
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	float BottomMargin = 230.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	float PortraitSize = 112.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	float FadeTime = 0.35f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(Transient)
	TObjectPtr<class USizeBox> PortraitBox;

	float Opacity = 0.f;
	float TargetOpacity = 0.f;
};
