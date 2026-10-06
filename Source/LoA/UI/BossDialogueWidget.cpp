#include "UI/BossDialogueWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UBossDialogueWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueRoot"));
		WidgetTree->RootWidget = Root;

		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueBand"));
		Band->SetBrushColor(BackgroundColor);
		Band->SetPadding(FMargin(24.f, 12.f, 48.f, 12.f));
		if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
		{
			// 하단 중앙 앵커 + 내용 크기에 맞춤
			BandSlot->SetAnchors(FAnchors(0.5f, 1.f));
			BandSlot->SetAlignment(FVector2D(0.5f, 1.f));
			BandSlot->SetPosition(FVector2D(0.f, -BottomMargin));
			BandSlot->SetAutoSize(true);
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DialogueRow"));
		Band->SetContent(Row);

		// 초상화 — 크기 고정(텍스처 원본 해상도로 커지지 않게)
		PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitBox"));
		PortraitBox->SetWidthOverride(PortraitSize);
		PortraitBox->SetHeightOverride(PortraitSize);
		PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
		PortraitBox->SetContent(PortraitImage);
		if (UHorizontalBoxSlot* PortraitSlot = Row->AddChildToHorizontalBox(PortraitBox))
		{
			PortraitSlot->SetVerticalAlignment(VAlign_Center);
			PortraitSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		}

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueLines"));
		if (UHorizontalBoxSlot* LinesSlot = Row->AddChildToHorizontalBox(Lines))
		{
			LinesSlot->SetVerticalAlignment(VAlign_Center);
		}

		auto MakeText = [&](const TCHAR* Name, int32 Size, FLinearColor Color) -> UTextBlock*
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			FSlateFontInfo Font = Text->GetFont();
			Font.Size = Size;
			Font.OutlineSettings.OutlineSize = 1;
			Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
			Text->SetFont(Font);
			Text->SetColorAndOpacity(FSlateColor(Color));
			if (UVerticalBoxSlot* TextSlot = Lines->AddChildToVerticalBox(Text))
			{
				TextSlot->SetPadding(FMargin(0.f, 2.f));
			}
			return Text;
		};

		SpeakerText = MakeText(TEXT("SpeakerText"), 13, SpeakerColor);
		LineText = MakeText(TEXT("LineText"), 15, LineColor);
	}

	// HUD 위젯 규칙 — 클릭 이동을 막지 않게
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(Opacity);

	return Super::RebuildWidget();
}

void UBossDialogueWidget::SetDialogue(const FText& Speaker, const FText& Line)
{
	// 트리는 RebuildWidget에서 만들어진다 — 화면에 올리기 전에 불려도 글자가 버려지지 않게 먼저 만든다
	if (!SpeakerText)
	{
		TakeWidget();
	}
	if (SpeakerText) SpeakerText->SetText(Speaker);
	if (LineText) LineText->SetText(Line);
}

void UBossDialogueWidget::SetPortrait(UTexture2D* Texture)
{
	if (!PortraitImage)
	{
		TakeWidget();
	}
	if (!PortraitImage || !PortraitBox) return;

	if (Texture)
	{
		PortraitImage->SetBrushFromTexture(Texture, false);
		PortraitBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		PortraitBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBossDialogueWidget::FadeIn()
{
	TargetOpacity = 1.f;
}

void UBossDialogueWidget::FadeOut()
{
	TargetOpacity = 0.f;
}

void UBossDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FMath::IsNearlyEqual(Opacity, TargetOpacity)) return;

	const float Speed = FadeTime > 0.f ? 1.f / FadeTime : 1000.f;
	Opacity = FMath::FInterpConstantTo(Opacity, TargetOpacity, InDeltaTime, Speed);
	SetRenderOpacity(Opacity);

	if (Opacity <= 0.f && TargetOpacity <= 0.f)
	{
		RemoveFromParent();
	}
}
