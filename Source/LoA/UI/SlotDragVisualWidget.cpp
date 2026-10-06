#include "UI/SlotDragVisualWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"

TSharedRef<SWidget> USlotDragVisualWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconBox"));
		Box->SetWidthOverride(IconSize);
		Box->SetHeightOverride(IconSize);
		WidgetTree->RootWidget = Box;

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IconImage"));
		Box->SetContent(IconImage);
	}

	// 드래그 아이콘이 놓을 자리의 판정을 가로채지 않게
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.75f);

	return Super::RebuildWidget();
}

void USlotDragVisualWidget::SetIconBrush(const FSlateBrush& Brush)
{
	// 트리는 RebuildWidget에서 만들어진다 — 화면에 올리기 전에 불려도 버려지지 않게 먼저 만든다
	if (!IconImage)
	{
		TakeWidget();
	}
	if (IconImage)
	{
		FSlateBrush Copy = Brush;
		Copy.ImageSize = FVector2D(IconSize, IconSize);
		IconImage->SetBrush(Copy);
	}
}
