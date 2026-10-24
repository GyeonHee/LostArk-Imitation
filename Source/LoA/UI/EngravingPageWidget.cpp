#include "UI/EngravingPageWidget.h"
#include "UI/BuildUIHelpers.h"
#include "Stat/EngravingComponent.h"
#include "LoAPlayerController.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

using namespace LoABuildUI;

namespace
{
	constexpr float EngPage_LeftWidth = 560.f;
	constexpr float EngPage_Board = 400.f;
}

TSharedRef<SWidget> UEngravingPageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EngravingPageRoot"));
		WidgetTree->RootWidget = Body;

		USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LeftSize->SetWidthOverride(EngPage_LeftWidth);
		Body->AddChildToHorizontalBox(LeftSize);
		UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftSize->SetContent(Left);

		Left->AddChildToVerticalBox(MakePresetBar(WidgetTree, 10));
		UBorder* PresetLabel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PresetLabel->SetBrushColor(SectionColor);
		PresetLabel->SetPadding(FMargin(8.f, 3.f));
		PresetLabel->SetContent(MakeText(WidgetTree, TEXT("각인 프리셋"), 10, UiLabel));
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(PresetLabel)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		BoardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BoardBox->SetWidthOverride(EngPage_Board);
		BoardBox->SetHeightOverride(EngPage_Board);
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(BoardBox)) S->SetHorizontalAlignment(HAlign_Center);

		SetupButton = MakeButton(WidgetTree, TEXT("각인 효과 설정하기"), 11, ButtonColor);
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(SetupButton))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Right))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		UTextBlock* Title = MakeText(WidgetTree, TEXT("장착 중인 각인"), 13, FLinearColor(0.92f, 0.87f, 0.72f));
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(Title))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
		}
		EffectList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(EffectList)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

void UEngravingPageWidget::SetEngravings(UEngravingComponent* InEngravings)
{
	if (Engravings.Get() == InEngravings) return;
	Unbind();
	Engravings = InEngravings;
	if (InEngravings) Handle = InEngravings->OnEngravingsChanged.AddUObject(this, &UEngravingPageWidget::HandleChanged);
	Refresh();
}

void UEngravingPageWidget::Unbind()
{
	if (UEngravingComponent* E = Engravings.Get()) E->OnEngravingsChanged.Remove(Handle);
	Handle.Reset();
}

void UEngravingPageWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UEngravingPageWidget::Refresh()
{
	if (!BoardBox || !EffectList) return;
	EffectList->ClearChildren();
	const UEngravingComponent* E = Engravings.Get();
	TArray<UWidget*> SlotWidgets;
	BoardBox->SetContent(MakeEngravingBoard(WidgetTree, EngPage_Board, E ? E->GetEquipped() : TArray<FName>(), E, SlotWidgets));
	if (!E) return;

	for (const FName& Id : E->GetEquipped())
	{
		const FEngravingDefinition* Def = LoAEngravings::Find(Id);
		if (!Def) continue;
		const int32 Level = FMath::Max(0, E->GetLevel(Id));

		// 머리 줄 — 주황 이름 | ◆ x 단계
		UBorder* Head = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Head->SetBrushColor(FLinearColor(0.16f, 0.07f, 0.05f, 1.f));
		Head->SetPadding(FMargin(8.f, 4.f));
		UHorizontalBox* HeadRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Head->SetContent(HeadRow);
		if (UHorizontalBoxSlot* S = HeadRow->AddChildToHorizontalBox(MakeText(WidgetTree, Def->Name.ToString(), 11, UiOrange)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		HeadRow->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("◆ x %d"), Level), 11, UiOrange));
		const int32 StoneLevel = E->GetStoneLevel(Id);
		if (StoneLevel > 0)
		{
			// 어빌리티 스톤 각인 레벨 — 로아 각인 창의 스톤 표시
			if (UHorizontalBoxSlot* S = HeadRow->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("  스톤 Lv.%d"), StoneLevel), 10, FLinearColor(0.45f, 0.8f, 1.f))))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
		if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(EffectList->AddChild(Head))) S->SetPadding(FMargin(0.f, 4.f, 6.f, 0.f));

		TArray<TPair<FString, FString>> Lines;
		LoAEngravings::DescribeLines(Id, Level, Lines, StoneLevel);
		if (Id == LoAEngravings::Adrenaline) Lines.Emplace(TEXT("현재 중첩"), FString::Printf(TEXT("%d / %d"), E->GetAdrenalineStacks(), LoAEngravings::AdrenalineMaxStacks));
		if (Id == LoAEngravings::MagickStream) Lines.Emplace(TEXT("현재 중첩"), FString::Printf(TEXT("%d / %d"), E->GetMagickStreamStacks(), LoAEngravings::MagickStreamMaxStacks));
		for (const TPair<FString, FString>& Line : Lines)
		{
			UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Row->SetBrushColor(RowColor);
			Row->SetPadding(FMargin(8.f, 4.f));
			UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Row->SetContent(RowBox);
			if (UHorizontalBoxSlot* S = RowBox->AddChildToHorizontalBox(MakeText(WidgetTree, Line.Key, 10, UiLabel)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			RowBox->AddChildToHorizontalBox(MakeText(WidgetTree, Line.Value, 10, UiLabel));
			if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(EffectList->AddChild(Row))) S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
	}
}

FReply UEngravingPageWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && SetupButton
		&& SetupButton->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>()) PC->SetEngravingSetupOpen(true);
	}
	return FReply::Handled();
}

FReply UEngravingPageWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}
