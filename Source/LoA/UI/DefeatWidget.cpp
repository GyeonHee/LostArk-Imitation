#include "UI/DefeatWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

namespace
{
	UTextBlock* MakeDefeatText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, FLinearColor Color, FLinearColor OutlineColor, int32 OutlineSize)
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
}

TSharedRef<SWidget> UDefeatWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefeatRoot"));
		WidgetTree->RootWidget = Root;

		// 화면 전체를 살짝 어둡게
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			DimSlot->SetOffsets(FMargin(0.f));
		}

		// 문구 뒤 붉은 띠 — 가로 전체, 화면 세로 중앙
		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RedBand"));
		Band->SetBrushColor(BandColor);
		if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
		{
			BandSlot->SetAnchors(FAnchors(0.f, 0.5f, 1.f, 0.5f));
			BandSlot->SetAlignment(FVector2D(0.f, 0.5f));
			BandSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 130.f));
		}

		// "사망하였습니다." 패널 — 띠 위쪽
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeathPanel"));
		Panel->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.9f));
		Panel->SetPadding(FMargin(50.f, 12.f));
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
			PanelSlot->SetPosition(FVector2D(0.f, -80.f));
			PanelSlot->SetAutoSize(true);
		}
		UVerticalBox* PanelLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelLines"));
		Panel->SetContent(PanelLines);

		UTextBlock* DeathTitle = MakeDefeatText(WidgetTree, TEXT("DeathTitle"), 17, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.8f), 1);
		DeathTitle->SetText(NSLOCTEXT("Defeat", "DeathTitle", "사망하였습니다."));
		UTextBlock* NoRevive = MakeDefeatText(WidgetTree, TEXT("NoRevive"), 12, FLinearColor(0.8f, 0.8f, 0.8f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.8f), 1);
		NoRevive->SetText(NSLOCTEXT("Defeat", "NoRevive", "부활을 진행할 수 없습니다."));
		CountdownText = MakeDefeatText(WidgetTree, TEXT("CountdownText"), 12, FLinearColor(0.8f, 0.8f, 0.8f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.8f), 1);
		for (UTextBlock* Line : { DeathTitle, NoRevive, CountdownText.Get() })
		{
			if (UVerticalBoxSlot* LineSlot = PanelLines->AddChildToVerticalBox(Line))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
				LineSlot->SetPadding(FMargin(0.f, 2.f));
			}
		}

		// 큰 문구 — 띠 한가운데
		UTextBlock* Failed = MakeDefeatText(WidgetTree, TEXT("FailedText"), 44, FLinearColor(1.f, 0.96f, 0.92f, 1.f), FLinearColor(0.45f, 0.f, 0.f, 0.9f), 3);
		Failed->SetText(NSLOCTEXT("Defeat", "Failed", "공략에 실패하였습니다."));
		if (UCanvasPanelSlot* FailedSlot = Root->AddChildToCanvas(Failed))
		{
			FailedSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			FailedSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			FailedSlot->SetAutoSize(true);
		}

		// 하단 사망 원인 (노란 글씨)
		DeathCauseText = MakeDefeatText(WidgetTree, TEXT("DeathCauseText"), 13, DeathCauseColor, FLinearColor(0.f, 0.f, 0.f, 0.8f), 1);
		if (UCanvasPanelSlot* CauseSlot = Root->AddChildToCanvas(DeathCauseText))
		{
			CauseSlot->SetAnchors(FAnchors(0.5f, 0.85f));
			CauseSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CauseSlot->SetAutoSize(true);
		}
	}

	// 클릭 이동 게임 HUD 규칙 — 마우스 입력을 먹지 않게
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UDefeatWidget::SetTexts(const FText& DeathCause)
{
	if (!DeathCauseText) TakeWidget();
	if (DeathCauseText) DeathCauseText->SetText(DeathCause);
}

void UDefeatWidget::SetCountdown(int32 Seconds)
{
	if (!CountdownText) TakeWidget();
	if (CountdownText)
	{
		CountdownText->SetText(FText::Format(NSLOCTEXT("Defeat", "Countdown", "{0}초 후 정비소로 이동합니다."), FText::AsNumber(FMath::Max(0, Seconds))));
	}
}
