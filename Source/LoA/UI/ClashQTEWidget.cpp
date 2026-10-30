#include "UI/ClashQTEWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor QteCircleFill(0.04f, 0.035f, 0.05f, 0.92f);
	const FLinearColor QteGold(1.f, 0.82f, 0.35f, 1.f);
	const FLinearColor QteRing(1.f, 0.9f, 0.55f, 0.95f);
	const FLinearColor QtePerfect(0.35f, 0.75f, 1.f, 1.f);
	const FLinearColor QteGood(0.55f, 1.f, 0.45f, 1.f);
	const FLinearColor QteMiss(1.f, 0.25f, 0.2f, 1.f);
	const FLinearColor QteDim(0.45f, 0.42f, 0.38f, 1.f);
}

TSharedRef<SWidget> UClashQTEWidget::RebuildWidget()
{
	EnsureTree();
	// 보기만 하는 화면 — 클릭 이동·키 입력을 막지 않는다 (키는 컨트롤러 InputKey가 받음)
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UClashQTEWidget::EnsureTree()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QteCanvas"));
	WidgetTree->RootWidget = Canvas;

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QteRoot"));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(RootBox))
	{
		// 화면 가운데보다 조금 아래 — 캐릭터·보스를 가리지 않게
		S->SetAnchors(FAnchors(0.5f, 0.66f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	auto AddRow = [this](UWidget* W, float Bottom)
	{
		if (UVerticalBoxSlot* S = RootBox->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		}
	};

	StageText = MakeText(TEXT("QteStage"), 22, QteGold);
	AddRow(StageText, 14.f);

	NotesRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QteNotes"));
	AddRow(NotesRow, 10.f);

	MashPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QteMash"));
	AddRow(MashPanel, 10.f);
	MashKeysRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QteMashKeys"));
	if (UVerticalBoxSlot* S = MashPanel->AddChildToVerticalBox(MashKeysRow))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QteMashBarBox"));
	BarBox->SetWidthOverride(420.f);
	BarBox->SetHeightOverride(22.f);
	MashBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("QteMashBar"));
	MashBar->SetFillColorAndOpacity(QteGold);
	BarBox->SetContent(MashBar);
	if (UVerticalBoxSlot* S = MashPanel->AddChildToVerticalBox(BarBox)) S->SetHorizontalAlignment(HAlign_Center);
	MashTimeText = MakeText(TEXT("QteMashTime"), 16, FLinearColor::White);
	if (UVerticalBoxSlot* S = MashPanel->AddChildToVerticalBox(MashTimeText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	MashPanel->SetVisibility(ESlateVisibility::Collapsed);

	JudgeText = MakeText(TEXT("QteJudge"), 34, FLinearColor::White);
	JudgeText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	JudgeText->SetRenderOpacity(0.f);
	AddRow(JudgeText, 0.f);
}

UTextBlock* UClashQTEWidget::MakeText(const FName& Name, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetJustification(ETextJustify::Center);
	return Text;
}

UImage* UClashQTEWidget::MakeCircleImage(const FName& Name, float Size, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	SetCircleColors(Image, Fill, Outline, OutlineWidth);
	return Image;
}

void UClashQTEWidget::SetCircleColors(UImage* Image, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
{
	if (!Image) return;
	// RoundedBox + 절반 높이 반지름 = 원. 채움 = TintColor, 테두리 = OutlineSettings (채움 알파 0이면 링만)
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FSlateColor(Outline), OutlineWidth);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	Image->SetBrush(Brush);
}

void UClashQTEWidget::SetStageText(const FText& Text)
{
	if (!StageText) TakeWidget();
	if (StageText) StageText->SetText(Text);
}

void UClashQTEWidget::ShowNotes(const TArray<FKey>& Keys)
{
	if (!NotesRow) TakeWidget();
	if (!NotesRow) return;

	NotesRow->ClearChildren();
	Notes.Reset();
	MashPanel->SetVisibility(ESlateVisibility::Collapsed);
	NotesRow->SetVisibility(ESlateVisibility::HitTestInvisible);

	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(NoteSize);
		Box->SetHeightOverride(NoteSize);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->SetContent(Layers);

		FNoteWidgets Note;
		Note.Root = Box;
		Note.Circle = MakeCircleImage(NAME_None, NoteSize, QteCircleFill, QteGold, 4.f);
		Note.Ring = MakeCircleImage(NAME_None, NoteSize, FLinearColor::Transparent, QteRing, 4.f);
		Note.Ring->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Note.Ring->SetRenderScale(FVector2D(RingMaxScale));
		Note.Ring->SetRenderOpacity(0.f);
		Note.KeyText = MakeText(NAME_None, 34, FLinearColor::White);
		Note.KeyText->SetText(Keys[i].GetDisplayName());

		for (UWidget* W : TArray<UWidget*>{ Note.Circle.Get(), Note.KeyText.Get(), Note.Ring.Get() })
		{
			if (UOverlaySlot* S = Layers->AddChildToOverlay(W))
			{
				const bool bText = W == Note.KeyText.Get();
				S->SetHorizontalAlignment(bText ? HAlign_Center : HAlign_Fill);
				S->SetVerticalAlignment(bText ? VAlign_Center : VAlign_Fill);
			}
		}

		if (UHorizontalBoxSlot* S = NotesRow->AddChildToHorizontalBox(Box))
		{
			// 링이 최대 크기로 커져도 옆 노트와 덜 겹치게 넉넉히
			S->SetPadding(FMargin(36.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Notes.Add(Note);
	}
}

void UClashQTEWidget::SetNoteApproach(int32 Index, float Alpha)
{
	if (!Notes.IsValidIndex(Index) || !Notes[Index].Ring) return;
	UImage* Ring = Notes[Index].Ring;
	if (Alpha <= 0.f || Alpha > 1.05f)
	{
		Ring->SetRenderOpacity(0.f);
		return;
	}
	Ring->SetRenderScale(FVector2D(FMath::Lerp(RingMaxScale, 1.f, FMath::Min(Alpha, 1.f))));
	// 들어올수록 진해짐
	Ring->SetRenderOpacity(FMath::Clamp(Alpha * 1.6f, 0.f, 1.f));
}

void UClashQTEWidget::SetNoteResult(int32 Index, int32 Result)
{
	if (!Notes.IsValidIndex(Index)) return;
	FNoteWidgets& Note = Notes[Index];
	if (Note.Ring) Note.Ring->SetRenderOpacity(Result == 0 ? Note.Ring->GetRenderOpacity() : 0.f);

	const FLinearColor Outline = Result == 1 ? QtePerfect : Result == 2 ? QteGood : Result == 3 ? QteMiss : QteGold;
	const FLinearColor Fill = Result == 0 ? QteCircleFill : Outline * FLinearColor(0.35f, 0.35f, 0.35f, 0.9f);
	SetCircleColors(Note.Circle, Fill, Outline, Result == 0 ? 4.f : 6.f);
	if (Note.KeyText && Result == 3) Note.KeyText->SetColorAndOpacity(FSlateColor(QteDim));
}

void UClashQTEWidget::ShowJudge(const FText& Text, const FLinearColor& Color)
{
	if (!JudgeText) TakeWidget();
	if (!JudgeText) return;
	JudgeText->SetText(Text);
	JudgeText->SetColorAndOpacity(FSlateColor(Color));
	JudgePop = 1.f;
}

void UClashQTEWidget::ShowMash(const FKey& KeyA, const FKey& KeyB)
{
	if (!MashPanel) TakeWidget();
	if (!MashPanel) return;

	NotesRow->ClearChildren();
	NotesRow->SetVisibility(ESlateVisibility::Collapsed);
	Notes.Reset();
	MashKeysRow->ClearChildren();
	MashKeys.Reset();
	MashPanel->SetVisibility(ESlateVisibility::HitTestInvisible);

	for (const FKey& Key : { KeyA, KeyB })
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(NoteSize);
		Box->SetHeightOverride(NoteSize);
		Box->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->SetContent(Layers);

		FNoteWidgets Entry;
		Entry.Root = Box;
		Entry.Circle = MakeCircleImage(NAME_None, NoteSize, QteCircleFill, QteGold, 4.f);
		Entry.KeyText = MakeText(NAME_None, 34, FLinearColor::White);
		Entry.KeyText->SetText(Key.GetDisplayName());
		if (UOverlaySlot* S = Layers->AddChildToOverlay(Entry.Circle))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* S = Layers->AddChildToOverlay(Entry.KeyText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = MashKeysRow->AddChildToHorizontalBox(Box))
		{
			S->SetPadding(FMargin(28.f, 0.f));
		}
		MashKeys.Add(Entry);
	}
	MashNextIndex = 0;
	MashPop = 0.f;
}

void UClashQTEWidget::SetMash(float Gauge, int32 NextKeyIndex, float TimeLeft)
{
	if (!MashBar) return;
	MashBar->SetPercent(FMath::Clamp(Gauge, 0.f, 1.f));
	// 거의 다 차면 금색 → 흰빛
	MashBar->SetFillColorAndOpacity(FMath::Lerp(QteGold, FLinearColor(1.f, 1.f, 0.9f, 1.f), FMath::Clamp((Gauge - 0.7f) / 0.3f, 0.f, 1.f)));
	if (MashTimeText)
	{
		MashTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Max(0.f, TimeLeft))));
	}
	if (NextKeyIndex != MashNextIndex)
	{
		MashNextIndex = NextKeyIndex;
	}
	for (int32 i = 0; i < MashKeys.Num(); ++i)
	{
		const bool bNext = i == MashNextIndex;
		SetCircleColors(MashKeys[i].Circle, QteCircleFill, bNext ? QteRing : QteDim, bNext ? 6.f : 3.f);
		if (MashKeys[i].KeyText) MashKeys[i].KeyText->SetColorAndOpacity(FSlateColor(bNext ? FLinearColor::White : QteDim));
	}
}

void UClashQTEWidget::PulseMash()
{
	MashPop = 1.f;
}

void UClashQTEWidget::HideAll()
{
	if (RootBox) RootBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UClashQTEWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 판정 글자 — 크게 튀어나왔다가 줄며 사라짐
	if (JudgeText && JudgePop > 0.f)
	{
		JudgePop = FMath::Max(0.f, JudgePop - InDeltaTime * 1.4f);
		JudgeText->SetRenderScale(FVector2D(1.f + 0.45f * FMath::Square(JudgePop)));
		JudgeText->SetRenderOpacity(FMath::Clamp(JudgePop * 2.f, 0.f, 1.f));
	}

	// 연타 — 다음 키가 커져 있고, 맞을 때마다 게이지 줄이 출렁
	MashPop = FMath::Max(0.f, MashPop - InDeltaTime * 8.f);
	for (int32 i = 0; i < MashKeys.Num(); ++i)
	{
		if (UWidget* Box = MashKeys[i].Root.Get())
		{
			Box->SetRenderScale(FVector2D(i == MashNextIndex ? 1.15f + 0.1f * MashPop : 0.9f));
		}
	}
	if (MashBar)
	{
		MashBar->SetRenderScale(FVector2D(1.f, 1.f + 0.35f * MashPop));
	}
}
