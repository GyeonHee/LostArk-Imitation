#include "CastBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UCastBarWidget::SetProgress(float Elapsed, float Total)
{
	if (Total <= 0.f)
	{
		HideBar();
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (CastBar)
	{
		CastBar->SetPercent(FMath::Clamp(Elapsed / Total, 0.f, 1.f));
	}

	if (CastTimeText)
	{
		CastTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.1f초"), Elapsed, Total)));
	}
}

void UCastBarWidget::HideBar()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
