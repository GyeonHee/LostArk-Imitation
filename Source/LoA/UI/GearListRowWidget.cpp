#include "UI/GearListRowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

namespace
{
	const FLinearColor GearRow_FrameNormal(0.14f, 0.15f, 0.19f, 1.f);
	const FLinearColor GearRow_FrameHover(0.30f, 0.75f, 0.85f, 1.f);
	const FLinearColor GearRow_FrameSelected(1.f, 0.78f, 0.3f, 1.f);

	UTextBlock* GearRow_MakeText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}
}

TSharedRef<SWidget> UGearListRowWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GearRowBox"));
		Box->SetHeightOverride(52.f);
		WidgetTree->RootWidget = Box;

		Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GearRowFrame"));
		Frame->SetBrushColor(GearRow_FrameNormal);
		Frame->SetPadding(FMargin(1.f));
		Box->SetContent(Frame);
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GearRowInner"));
		Inner->SetBrushColor(FLinearColor(0.16f, 0.07f, 0.03f, 1.f));  // 유물 주황빛 바탕
		Inner->SetPadding(FMargin(4.f, 3.f));
		Frame->SetContent(Inner);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GearRowRow"));
		Inner->SetContent(Row);
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GearRowIconBox"));
		IconBox->SetWidthOverride(44.f);
		IconBox->SetHeightOverride(44.f);
		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GearRowIcon"));
		IconBox->SetContent(IconImage);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GearRowTexts"));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Texts))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		TopText = GearRow_MakeText(WidgetTree, 9, FLinearColor(1.f, 0.88f, 0.5f, 1.f));
		Texts->AddChildToVerticalBox(TopText);
		NameText = GearRow_MakeText(WidgetTree, 11, FLinearColor(1.f, 0.58f, 0.22f, 1.f));
		Texts->AddChildToVerticalBox(NameText);
		RightText = GearRow_MakeText(WidgetTree, 9, FLinearColor(0.8f, 0.8f, 0.82f, 1.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(RightText))
		{
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(4.f, 2.f, 2.f, 0.f));
		}
	}
	SetVisibility(ESlateVisibility::Visible);
	return Super::RebuildWidget();
}

void UGearListRowWidget::SetRow(const FGuid& InId, UTexture2D* Icon, const FText& TopLine, const FText& Name, const FText& InRightText, bool bInSelected)
{
	if (!NameText) TakeWidget();
	InstanceId = InId;
	bSelected = bInSelected;
	if (Icon)
	{
		IconImage->SetBrushFromTexture(Icon);
		IconImage->SetColorAndOpacity(FLinearColor::White);
	}
	else
	{
		IconImage->SetBrushResourceObject(nullptr);
		IconImage->SetColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.35f, 1.f));
	}
	TopText->SetText(TopLine);
	TopText->SetVisibility(TopLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	NameText->SetText(Name);
	RightText->SetText(InRightText);
	ApplyFrame();
}

void UGearListRowWidget::ApplyFrame()
{
	if (!Frame) return;
	Frame->SetBrushColor(bSelected ? GearRow_FrameSelected : (bHovered ? GearRow_FrameHover : GearRow_FrameNormal));
}

FReply UGearListRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && InstanceId.IsValid())
	{
		OnClicked.ExecuteIfBound(InstanceId);
	}
	return FReply::Handled();
}

FReply UGearListRowWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGearListRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyFrame();
}

void UGearListRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyFrame();
}
