#include "UI/ClearWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

namespace
{
	UTextBlock* MakeClearText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, FLinearColor Color, FLinearColor OutlineColor, int32 OutlineSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = OutlineSize;
		Font.OutlineSettings.OutlineColor = OutlineColor;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}

	// 가로 전체를 덮는 띠 — 화면 세로 AnchorY 위치에 Height 두께
	UBorder* AddClearBand(UWidgetTree* Tree, UCanvasPanel* Root, const TCHAR* Name, FLinearColor Color, float AnchorY, float Height)
	{
		UBorder* Band = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Band->SetBrushColor(Color);
		if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
		{
			BandSlot->SetAnchors(FAnchors(0.f, AnchorY, 1.f, AnchorY));
			BandSlot->SetAlignment(FVector2D(0.f, 0.5f));
			BandSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, Height));
		}
		return Band;
	}
}

TSharedRef<SWidget> UClearWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ClearRoot"));
		WidgetTree->RootWidget = Root;

		// 상단 안내 — 반투명 어두운 띠 위에 3줄 (처치 문구 / 이동까지 남은 시간 / N초)
		UBorder* TopBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBand"));
		TopBand->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		TopBand->SetPadding(FMargin(80.f, 10.f));
		if (UCanvasPanelSlot* TopSlot = Root->AddChildToCanvas(TopBand))
		{
			TopSlot->SetAnchors(FAnchors(0.5f, 0.15f));
			TopSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TopSlot->SetAutoSize(true);
		}
		UVerticalBox* TopLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TopLines"));
		TopBand->SetContent(TopLines);

		const FLinearColor Shadow(0.f, 0.f, 0.f, 0.8f);
		HeadlineText = MakeClearText(WidgetTree, TEXT("HeadlineText"), 13, FLinearColor(0.85f, 0.85f, 0.85f, 1.f), Shadow, 1);
		UTextBlock* RemainLabel = MakeClearText(WidgetTree, TEXT("RemainLabel"), 13, FLinearColor(0.85f, 0.85f, 0.85f, 1.f), Shadow, 1);
		RemainLabel->SetText(NSLOCTEXT("Clear", "RemainLabel", "정비소 이동까지 남은 시간"));
		CountdownText = MakeClearText(WidgetTree, TEXT("CountdownText"), 15, AccentColor, Shadow, 1);
		for (UTextBlock* Line : { HeadlineText.Get(), RemainLabel, CountdownText.Get() })
		{
			if (UVerticalBoxSlot* LineSlot = TopLines->AddChildToVerticalBox(Line))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
				LineSlot->SetPadding(FMargin(0.f, 2.f));
			}
		}

		// 가운데 금빛 광채 — 넓고 옅은 띠 위에 좁고 밝은 띠를 겹쳐 빛이 번지는 느낌
		AddClearBand(WidgetTree, Root, TEXT("GlowWide"), GlowColor, 0.5f, 170.f);
		AddClearBand(WidgetTree, Root, TEXT("GlowCore"), GlowCoreColor, 0.5f, 6.f);

		// 큰 문구
		UTextBlock* ClearText = MakeClearText(WidgetTree, TEXT("ClearText"), 56, FLinearColor(1.f, 0.98f, 0.94f, 1.f), FLinearColor(0.55f, 0.28f, 0.02f, 0.95f), 3);
		ClearText->SetText(NSLOCTEXT("Clear", "DungeonClear", "던전 클리어"));
		if (UCanvasPanelSlot* ClearSlot = Root->AddChildToCanvas(ClearText))
		{
			ClearSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ClearSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ClearSlot->SetAutoSize(true);
		}

		// 결과 줄 — 어두운 띠 위 한 줄 (레퍼런스의 전리품 줄 자리)
		UBorder* ResultBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultBand"));
		ResultBand->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.8f));
		ResultBand->SetPadding(FMargin(60.f, 10.f));
		if (UCanvasPanelSlot* ResultSlot = Root->AddChildToCanvas(ResultBand))
		{
			ResultSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ResultSlot->SetAlignment(FVector2D(0.5f, 0.f));
			ResultSlot->SetPosition(FVector2D(0.f, 110.f));
			ResultSlot->SetAutoSize(true);
		}
		ResultText = MakeClearText(WidgetTree, TEXT("ResultText"), 14, FLinearColor(0.92f, 0.92f, 0.92f, 1.f), Shadow, 1);
		ResultBand->SetContent(ResultText);
	}

	// 클릭 이동 게임 HUD 규칙 — 마우스 입력을 먹지 않게
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UClearWidget::SetTexts(const FText& Headline, const FText& ResultLine)
{
	if (!HeadlineText) TakeWidget();
	if (HeadlineText) HeadlineText->SetText(Headline);
	if (ResultText) ResultText->SetText(ResultLine);
}

void UClearWidget::SetCountdown(int32 Seconds)
{
	if (!CountdownText) TakeWidget();
	if (CountdownText)
	{
		CountdownText->SetText(FText::Format(NSLOCTEXT("Clear", "Countdown", "{0}초"), FText::AsNumber(FMath::Max(0, Seconds))));
	}
}
