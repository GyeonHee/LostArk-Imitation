#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CastBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 캐스팅/차지 진행바 — 진행 중일 때만 보이고, 경과 시간에 비례해 게이지가 찬다.
 * 시간 텍스트(CastTimeText)는 바 우측 하단에 작게 얹힌다.
 *
 * WBP에서 CastBar(ProgressBar)와 CastTimeText(TextBlock)를 이 이름 그대로 만들어야 한다.
 * 갱신은 ALoAPlayerController::Tick이 USkillManagerComponent::GetActiveCastProgress()를 폴링하는 방식 —
 * 진행 중인 스킬이 없으면 HideBar()가 불려 위젯 전체가 Collapsed 된다.
 */
UCLASS()
class LOA_API UCastBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 진행 중 갱신 — 위젯을 보이게 하고 게이지/텍스트를 채운다 */
	UFUNCTION(BlueprintCallable, Category = "CastBar")
	void SetProgress(float Elapsed, float Total);

	/** 진행 중인 스킬이 없을 때 — 위젯 전체를 숨긴다 */
	UFUNCTION(BlueprintCallable, Category = "CastBar")
	void HideBar();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "CastBar", meta = (BindWidget))
	TObjectPtr<UProgressBar> CastBar;

	UPROPERTY(BlueprintReadOnly, Category = "CastBar", meta = (BindWidget))
	TObjectPtr<UTextBlock> CastTimeText;
};
