#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHPWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;

/**
 * 보스 HP 바 — 로스트아크식 "줄" 표시.
 * 바는 전체 HP가 아니라 **현재 줄 안의 잔량**만 보여주고, 줄이 하나 깎일 때마다 색이 통째로 바뀐다.
 * HPBar 뒤에는 NextLineImage가 다음 줄 색으로 깔려 있어서, 현재 줄이 비어갈수록 다음 줄 색이 드러난다.
 * 마지막 한 줄에서는 뒤에 남은 줄이 없으므로 NextLineImage가 투명이 되어 빈 칸이 보인다.
 *
 * WBP에서 HPBar(ProgressBar) / NextLineImage(Image) / LineText(TextBlock)를 이 이름 그대로 만들어야 한다.
 * NextLineImage가 HPBar보다 뒤에 깔리도록 Overlay 자식 순서에서 먼저 와야 하고,
 * HPBar의 WidgetStyle 배경은 투명이어야 뒤가 비쳐 보인다.
 * 갱신은 ALoAPlayerController가 AEchidnaBoss::OnHPChanged를 받아 SetBossHP()를 호출하는 방식.
 */
UCLASS()
class LOA_API UBossHPWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBossHPWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "BossHP")
	void SetBossHP(float NewHP, float NewMaxHP, int32 TotalLines);

protected:
	/** 줄마다 돌아가며 쓰는 색 — 줄 L의 색은 LineColors[(L-1) % Num].
	 *  인접한 줄이 같은 색이 되지 않도록 최소 2개, 되도록 3개 이상 넣을 것 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BossHP")
	TArray<FLinearColor> LineColors;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidget))
	TObjectPtr<UProgressBar> HPBar;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidget))
	TObjectPtr<UImage> NextLineImage;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidget))
	TObjectPtr<UTextBlock> LineText;

private:
	/** 줄 번호에 대응하는 색. 0 이하(= 더 깎을 줄이 없음)면 투명 */
	FLinearColor GetLineColor(int32 Line) const;
};
