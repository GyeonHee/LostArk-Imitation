#include "UI/CinematicOverlayWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UCinematicOverlayWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CinematicRoot"));
		WidgetTree->RootWidget = Root;

		auto MakeBar = [&](const TCHAR* Name, float AnchorY) -> UBorder*
		{
			UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
			Bar->SetBrushColor(FLinearColor::Black);
			if (UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(Bar))
			{
				// 가로로 꽉 채우고, 위 띠는 위쪽 모서리 / 아래 띠는 아래쪽 모서리에 붙인다
				BarSlot->SetAnchors(FAnchors(0.f, AnchorY, 1.f, AnchorY));
				BarSlot->SetAlignment(FVector2D(0.f, AnchorY));
				BarSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
			}
			return Bar;
		};
		TopBar = MakeBar(TEXT("TopBar"), 0.f);
		BottomBar = MakeBar(TEXT("BottomBar"), 1.f);

		TitleBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TitleBox"));
		if (UCanvasPanelSlot* TitleSlot = Root->AddChildToCanvas(TitleBox))
		{
			// 아래 띠 바로 위, 가로 중앙
			TitleSlot->SetAnchors(FAnchors(0.5f, 1.f));
			TitleSlot->SetAlignment(FVector2D(0.5f, 1.f));
			TitleSlot->SetPosition(FVector2D(0.f, -(BarHeight + 30.f)));
			TitleSlot->SetAutoSize(true);
		}

		auto MakeText = [&](const TCHAR* Name, int32 Size, FLinearColor Color) -> UTextBlock*
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			FSlateFontInfo Font = Text->GetFont();
			Font.Size = Size;
			Font.OutlineSettings.OutlineSize = 2;
			Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			Text->SetFont(Font);
			Text->SetColorAndOpacity(FSlateColor(Color));
			Text->SetJustification(ETextJustify::Center);
			if (UVerticalBoxSlot* LineSlot = TitleBox->AddChildToVerticalBox(Text))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
			}
			return Text;
		};
		SubtitleText = MakeText(TEXT("SubtitleText"), 16, FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
		TitleText = MakeText(TEXT("TitleText"), 40, TitleColor);
	}

	// 클릭 이동 게임이라 연출 위젯이 마우스 입력을 먹으면 안 된다
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UCinematicOverlayWidget::SetTitle(const FText& Title, const FText& Subtitle)
{
	// 트리는 RebuildWidget에서 만들어진다 — AddToViewport 전에 불려도 되게 먼저 만든다
	if (!TitleText) TakeWidget();
	if (TitleText) TitleText->SetText(Title);
	if (SubtitleText)
	{
		SubtitleText->SetText(Subtitle);
		SubtitleText->SetVisibility(Subtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UCinematicOverlayWidget::SetBarAmount(float Amount)
{
	const float Height = BarHeight * FMath::Clamp(Amount, 0.f, 1.f);
	for (UBorder* Bar : { TopBar.Get(), BottomBar.Get() })
	{
		if (!Bar) continue;
		if (UCanvasPanelSlot* BarSlot = Cast<UCanvasPanelSlot>(Bar->Slot))
		{
			// 좌우 앵커가 벌어져 있으므로 Offsets = (Left, Top, Right, Bottom) 중 Bottom 자리가 곧 높이
			BarSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, Height));
		}
	}
}

void UCinematicOverlayWidget::SetTitleOpacity(float Opacity)
{
	if (TitleBox)
	{
		TitleBox->SetRenderOpacity(FMath::Clamp(Opacity, 0.f, 1.f));
	}
}
