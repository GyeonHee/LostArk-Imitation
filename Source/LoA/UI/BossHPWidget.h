#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHPWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UTexture2D;

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
	void SetBossHP(double NewHP, double NewMaxHP, int32 TotalLines);

	/** 체력바 왼쪽의 광폭화 타이머 — "광폭화까지 / 00:08:56". 컨트롤러가 매 Tick 호출하지만
	 *  표시 초가 바뀔 때만 텍스트를 다시 만든다 */
	UFUNCTION(BlueprintCallable, Category = "BossHP")
	void SetEnrageTime(float RemainingSeconds, bool bEnraged);

	/** 초상화 아래 2칸 정산 게이지 — 1칸 = 0~50%, 2칸 = 50~100% */
	UFUNCTION(BlueprintCallable, Category = "BossHP")
	void SetSettlementGauge(float Percent);

	/** 정산 게이지 정지 표시 — 초상화는 흑백 텍스처로, 게이지 2칸·% 글자는 회색으로. 풀리면 원래대로 */
	UFUNCTION(BlueprintCallable, Category = "BossHP")
	void SetSettlementPaused(bool bPaused);

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

	// 광폭화 라벨/시간 — Optional이라 WBP에 없어도 컴파일은 된다(타이머만 안 보임)
	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnrageLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnrageTimeText;

	// 정산 게이지 2칸 + 수치 — Optional
	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SettlementBar1;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SettlementBar2;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SettlementText;

	UPROPERTY(BlueprintReadOnly, Category = "BossHP", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	// 정지 중 초상화 — 원래 텍스처는 WBP의 PortraitImage 브러시를 그대로 기억해 두었다가 되돌린다
	UPROPERTY(EditDefaultsOnly, Category = "BossHP")
	TSoftObjectPtr<UTexture2D> PortraitPausedTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/T_EchidnaPortrait_Gray.T_EchidnaPortrait_Gray")));

	UPROPERTY(EditDefaultsOnly, Category = "BossHP")
	FLinearColor SettlementPausedColor = FLinearColor(0.35f, 0.35f, 0.35f, 1.f);

	// 평소 타이머 색 (로아 UI의 금색)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BossHP")
	FLinearColor EnrageNormalColor = FLinearColor(0.85f, 0.65f, 0.25f);

	// 1분 이하 남았을 때와 광폭화 이후 색
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BossHP")
	FLinearColor EnrageWarningColor = FLinearColor(0.95f, 0.15f, 0.1f);

private:
	/** 줄 번호에 대응하는 색. 0 이하(= 더 깎을 줄이 없음)면 투명 */
	FLinearColor GetLineColor(int32 Line) const;

	int32 LastShownEnrageSecond = -1;

	// 정지 표시 전 원래 모습 (처음 정지할 때 캡처)
	bool bSettlementPausedShown = false;
	bool bCapturedSettlementLook = false;
	FSlateBrush OriginalPortraitBrush;
	FLinearColor OriginalBarFill = FLinearColor::White;
	FSlateColor OriginalSettlementTextColor;
	float LastShownSettlement = -1.f;
	bool bLastShownEnraged = false;
};
