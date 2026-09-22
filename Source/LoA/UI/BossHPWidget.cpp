#include "BossHPWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

UBossHPWidget::UBossHPWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LineColors = {
		FLinearColor(0.72f, 0.09f, 0.11f, 1.f),
		FLinearColor(0.90f, 0.45f, 0.10f, 1.f),
		FLinearColor(0.88f, 0.78f, 0.15f, 1.f),
		FLinearColor(0.20f, 0.68f, 0.30f, 1.f),
		FLinearColor(0.15f, 0.45f, 0.85f, 1.f),
	};
}

FLinearColor UBossHPWidget::GetLineColor(int32 Line) const
{
	if (Line <= 0 || LineColors.Num() == 0)
	{
		return FLinearColor::Transparent;
	}
	return LineColors[(Line - 1) % LineColors.Num()];
}

void UBossHPWidget::SetBossHP(float NewHP, float NewMaxHP, int32 TotalLines)
{
	const float LineValue = (TotalLines > 0 && NewMaxHP > 0.f)
		? NewMaxHP / static_cast<float>(TotalLines)
		: 0.f;

	// 바는 전체 HP 비율이 아니라 "현재 줄 안에서 남은 비율"이다.
	// HP가 0이면 줄도 0으로 두어 앞뒤 색이 모두 투명해지도록 한다.
	int32 CurrentLine = 0;
	float LineFill = 0.f;
	if (LineValue > 0.f && NewHP > 0.f)
	{
		CurrentLine = FMath::Clamp(FMath::CeilToInt(NewHP / LineValue), 1, TotalLines);
		LineFill = FMath::Clamp((NewHP - (CurrentLine - 1) * LineValue) / LineValue, 0.f, 1.f);
	}

	if (HPBar)
	{
		HPBar->SetPercent(LineFill);
		HPBar->SetFillColorAndOpacity(GetLineColor(CurrentLine));
	}

	if (NextLineImage)
	{
		// 마지막 줄이면 CurrentLine-1 == 0 이라 투명이 되어 "더 깎을 줄이 없음"이 그대로 보인다
		NextLineImage->SetColorAndOpacity(GetLineColor(CurrentLine - 1));
	}

	if (LineText)
	{
		// 체력이 천만 단위라 자릿수 구분이 없으면 읽기 어렵다
		LineText->SetText(FText::FromString(FString::Printf(TEXT("%s / %s    %d"),
			*FString::FormatAsNumber(FMath::RoundToInt(NewHP)),
			*FString::FormatAsNumber(FMath::RoundToInt(NewMaxHP)),
			CurrentLine)));
	}
}
