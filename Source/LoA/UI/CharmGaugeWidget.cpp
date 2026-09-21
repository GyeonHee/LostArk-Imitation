#include "UI/CharmGaugeWidget.h"
#include "Components/Image.h"

void UCharmGaugeWidget::SetStacks(int32 NewStacks)
{
	SetVisibility(NewStacks > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (ColorImage1)
	{
		ColorImage1->SetVisibility(NewStacks >= 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ColorImage2)
	{
		ColorImage2->SetVisibility(NewStacks >= 2 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ColorImage3)
	{
		ColorImage3->SetVisibility(NewStacks >= 3 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
