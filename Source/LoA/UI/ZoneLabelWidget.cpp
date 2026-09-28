#include "UI/ZoneLabelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UZoneLabelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
		WidgetTree->RootWidget = LabelText;

		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = FontSize;
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		LabelText->SetFont(Font);
		LabelText->SetJustification(ETextJustify::Center);
	}

	// 클릭 이동 게임이라 이름표가 마우스 입력을 먹으면 안 된다
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UZoneLabelWidget::SetLabel(const FText& Text, FLinearColor Color)
{
	// 트리는 RebuildWidget에서 만들어지는데, WidgetComponent가 화면에 올리기 전(BeginPlay)에 불리면 아직 없다 → 먼저 만들게 한다
	if (!LabelText)
	{
		TakeWidget();
	}
	if (!LabelText) return;
	LabelText->SetText(Text);
	LabelText->SetColorAndOpacity(FSlateColor(Color));
}
