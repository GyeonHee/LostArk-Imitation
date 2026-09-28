#include "UI/ZoneNoticeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UZoneNoticeWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NoticeRoot"));
		WidgetTree->RootWidget = Root;

		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NoticeBand"));
		Band->SetBrushColor(BackgroundColor);
		Band->SetPadding(FMargin(60.f, 8.f));
		if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
		{
			// 상단 중앙 앵커 + 내용 크기에 맞춤
			BandSlot->SetAnchors(FAnchors(0.5f, 0.f));
			BandSlot->SetAlignment(FVector2D(0.5f, 0.f));
			BandSlot->SetPosition(FVector2D(0.f, TopMargin));
			BandSlot->SetAutoSize(true);
		}

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NoticeLines"));
		Band->SetContent(Lines);

		auto MakeLine = [&](const TCHAR* Name, int32 Size, FLinearColor Color) -> UTextBlock*
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			FSlateFontInfo Font = Text->GetFont();
			Font.Size = Size;
			Font.OutlineSettings.OutlineSize = 1;
			Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
			Text->SetFont(Font);
			Text->SetColorAndOpacity(FSlateColor(Color));
			Text->SetJustification(ETextJustify::Center);
			if (UVerticalBoxSlot* LineSlot = Lines->AddChildToVerticalBox(Text))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
				LineSlot->SetPadding(FMargin(0.f, 2.f));
			}
			return Text;
		};

		TitleText = MakeLine(TEXT("TitleText"), 18, TitleColor);
		MessageText = MakeLine(TEXT("MessageText"), 13, FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
		CounterText = MakeLine(TEXT("CounterText"), 22, CounterColor);
	}

	// HUD 위젯 규칙 — 클릭 이동을 막지 않게
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UZoneNoticeWidget::SetNotice(const FText& Title, const FText& Message)
{
	if (TitleText) TitleText->SetText(Title);
	if (MessageText)
	{
		MessageText->SetText(Message);
		MessageText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UZoneNoticeWidget::SetCounter(const FText& Counter)
{
	if (!CounterText) return;
	CounterText->SetText(Counter);
	CounterText->SetVisibility(Counter.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
