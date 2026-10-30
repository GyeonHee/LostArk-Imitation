#include "UI/NPCWorldWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	UTextBlock* MakeNPCWorldText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}
}

// ─── 이름표 ─────────────────────────────────────────────────────────────────

TSharedRef<SWidget> UNPCNameWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NPCNameBox"));
		WidgetTree->RootWidget = Box;

		TitleText = MakeNPCWorldText(WidgetTree, TEXT("NPCTitle"), 11, FLinearColor(0.45f, 0.9f, 0.35f, 1.f));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(TitleText)) S->SetHorizontalAlignment(HAlign_Center);

		NameText = MakeNPCWorldText(WidgetTree, TEXT("NPCName"), 14, FLinearColor(1.f, 0.93f, 0.7f, 1.f));
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(NameText)) S->SetHorizontalAlignment(HAlign_Center);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UNPCNameWidget::SetNames(const FText& Title, const FText& Name)
{
	// 트리는 RebuildWidget에서 만들어진다 — 아직이면 먼저 만든다 (ZoneLabelWidget에서 겪은 함정)
	if (!TitleText) TakeWidget();
	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (NameText) NameText->SetText(Name);
}

// ─── [G] 대화하기 ───────────────────────────────────────────────────────────

TSharedRef<SWidget> UInteractPromptWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptBack"));
		Back->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Back->SetPadding(FMargin(4.f, 3.f, 8.f, 3.f));
		WidgetTree->RootWidget = Back;

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PromptRow"));
		Back->SetContent(Row);

		// 키 배지 — 밝은 네모 안에 검은 글자
		UBorder* KeyBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("KeyBadge"));
		KeyBadge->SetBrushColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.f));
		KeyBadge->SetPadding(FMargin(6.f, 1.f));
		KeyText = MakeNPCWorldText(WidgetTree, TEXT("KeyText"), 12, FLinearColor(0.05f, 0.05f, 0.05f, 1.f));
		FSlateFontInfo KeyFont = KeyText->GetFont();
		KeyFont.OutlineSettings.OutlineSize = 0;
		KeyText->SetFont(KeyFont);
		KeyBadge->SetContent(KeyText);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(KeyBadge))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}

		ActionText = MakeNPCWorldText(WidgetTree, TEXT("ActionText"), 13, FLinearColor(1.f, 1.f, 1.f, 1.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ActionText)) S->SetVerticalAlignment(VAlign_Center);

		KeyText->SetText(FText::FromString(TEXT("G")));
		ActionText->SetText(NSLOCTEXT("NPC", "Talk", "대화하기"));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UInteractPromptWidget::SetPrompt(const FText& Key, const FText& Action)
{
	if (!KeyText) TakeWidget();
	if (KeyText) KeyText->SetText(Key);
	if (ActionText) ActionText->SetText(Action);
}

// ─── 월드 카운트다운 ────────────────────────────────────────────────────────

TSharedRef<SWidget> UWorldCountdownWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CountText = MakeNPCWorldText(WidgetTree, TEXT("CountText"), 30, FLinearColor(0.75f, 0.9f, 1.f, 1.f));
		FSlateFontInfo Font = CountText->GetFont();
		Font.OutlineSettings.OutlineSize = 2;
		CountText->SetFont(Font);
		WidgetTree->RootWidget = CountText;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UWorldCountdownWidget::SetSeconds(int32 Seconds)
{
	if (!CountText) TakeWidget();
	if (CountText) CountText->SetText(FText::FromString(FString::Printf(TEXT("%ds"), FMath::Max(0, Seconds))));
}
