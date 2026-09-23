#include "BossHPWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

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

void UBossHPWidget::SetBossHP(double NewHP, double NewMaxHP, int32 TotalLines)
{
	const double LineValue = (TotalLines > 0 && NewMaxHP > 0.0)
		? NewMaxHP / static_cast<double>(TotalLines)
		: 0.0;

	// 바는 전체 HP 비율이 아니라 "현재 줄 안에서 남은 비율"이다.
	// HP가 0이면 줄도 0으로 두어 앞뒤 색이 모두 투명해지도록 한다.
	int32 CurrentLine = 0;
	float LineFill = 0.f;
	if (LineValue > 0.0 && NewHP > 0.0)
	{
		CurrentLine = FMath::Clamp(FMath::CeilToInt(NewHP / LineValue), 1, TotalLines);
		LineFill = static_cast<float>(FMath::Clamp((NewHP - (CurrentLine - 1) * LineValue) / LineValue, 0.0, 1.0));
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
		// 체력이 40억대라 int32(최대 약 21억)로 반올림하면 오버플로 — FString::FormatAsNumber가 int32 전용이라
		// int64를 받는 FText::AsNumber로 자릿수 구분
		LineText->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}    {2}")),
			FText::AsNumber(FMath::RoundToInt64(NewHP)),
			FText::AsNumber(FMath::RoundToInt64(NewMaxHP)),
			FText::AsNumber(CurrentLine)));
	}
}

void UBossHPWidget::SetEnrageTime(float RemainingSeconds, bool bEnraged)
{
	// 올림 — 시작 순간 08:59가 아니라 09:00으로 보이게
	const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(RemainingSeconds));
	if (TotalSeconds == LastShownEnrageSecond && bEnraged == bLastShownEnraged)
	{
		return;
	}
	LastShownEnrageSecond = TotalSeconds;
	bLastShownEnraged = bEnraged;

	const FSlateColor Color(bEnraged || TotalSeconds <= 60 ? EnrageWarningColor : EnrageNormalColor);

	if (EnrageLabelText)
	{
		EnrageLabelText->SetText(FText::FromString(bEnraged ? TEXT("광폭화") : TEXT("광폭화까지")));
		EnrageLabelText->SetColorAndOpacity(Color);
	}

	if (EnrageTimeText)
	{
		EnrageTimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"),
			TotalSeconds / 3600, (TotalSeconds / 60) % 60, TotalSeconds % 60)));
		EnrageTimeText->SetColorAndOpacity(Color);
	}
}

void UBossHPWidget::SetSettlementGauge(float Percent)
{
	if (FMath::IsNearlyEqual(Percent, LastShownSettlement)) return;
	LastShownSettlement = Percent;

	if (SettlementBar1)
	{
		SettlementBar1->SetPercent(FMath::Clamp(Percent / 50.f, 0.f, 1.f));
	}
	if (SettlementBar2)
	{
		SettlementBar2->SetPercent(FMath::Clamp((Percent - 50.f) / 50.f, 0.f, 1.f));
	}
	if (SettlementText)
	{
		SettlementText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Percent))));
	}
}

void UBossHPWidget::SetSettlementPaused(bool bPaused)
{
	if (bPaused == bSettlementPausedShown) return;
	bSettlementPausedShown = bPaused;

	// 처음 정지할 때 WBP에 잡혀 있던 원래 모습을 기억 — 색·텍스처를 C++에 하드코딩하지 않으려고
	if (!bCapturedSettlementLook)
	{
		bCapturedSettlementLook = true;
		if (PortraitImage) OriginalPortraitBrush = PortraitImage->GetBrush();
		if (SettlementBar1) OriginalBarFill = SettlementBar1->GetFillColorAndOpacity();
		if (SettlementText) OriginalSettlementTextColor = SettlementText->GetColorAndOpacity();
	}

	if (PortraitImage)
	{
		UTexture2D* PausedTexture = bPaused ? PortraitPausedTexture.LoadSynchronous() : nullptr;
		if (PausedTexture)
		{
			PortraitImage->SetBrushResourceObject(PausedTexture);
		}
		else
		{
			PortraitImage->SetBrush(OriginalPortraitBrush);
		}
	}

	const FLinearColor BarColor = bPaused ? SettlementPausedColor : OriginalBarFill;
	if (SettlementBar1) SettlementBar1->SetFillColorAndOpacity(BarColor);
	if (SettlementBar2) SettlementBar2->SetFillColorAndOpacity(BarColor);

	if (SettlementText)
	{
		SettlementText->SetColorAndOpacity(bPaused ? FSlateColor(SettlementPausedColor) : OriginalSettlementTextColor);
	}
}
