#include "UI/BraceletEnchantWidget.h"
#include "UI/GearListRowWidget.h"
#include "Item/AccessoryPolish.h"
#include "Item/BraceletStone.h"
#include "Item/EquipmentComponent.h"
#include "Item/InventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FLinearColor BraceletUI_PanelColor(0.022f, 0.027f, 0.04f, 0.97f);
	const FLinearColor BraceletUI_ColumnColor(0.035f, 0.042f, 0.06f, 1.f);
	const FLinearColor BraceletUI_BoxColor(0.03f, 0.07f, 0.12f, 1.f);       // 로아 효과 상자 — 짙은 남색
	const FLinearColor BraceletUI_BoxFrame(0.18f, 0.35f, 0.5f, 1.f);
	const FLinearColor BraceletUI_TextColor(0.88f, 0.88f, 0.9f, 1.f);
	const FLinearColor BraceletUI_DimColor(0.5f, 0.51f, 0.55f, 1.f);
	const FLinearColor BraceletUI_LabelColor(0.4f, 0.8f, 1.f, 1.f);        // "고정 효과" 하늘색
	const FLinearColor BraceletUI_GoldColor(1.f, 0.82f, 0.4f, 1.f);
	const FLinearColor BraceletUI_YellowColor(1.f, 0.85f, 0.3f, 1.f);
	const FLinearColor BraceletUI_GreenColor(0.55f, 0.95f, 0.45f, 1.f);
	const FLinearColor BraceletUI_LockColor(1.f, 0.82f, 0.3f, 1.f);
	const FLinearColor BraceletUI_ShortColor(1.f, 0.35f, 0.3f, 1.f);
	// [효과 선택] 상자 — 기존(파랑) / 신규(금색), 고른 쪽은 밝게
	const FLinearColor BraceletUI_OldFrame(0.25f, 0.5f, 0.75f, 1.f);
	const FLinearColor BraceletUI_OldFramePicked(0.45f, 0.8f, 1.f, 1.f);
	const FLinearColor BraceletUI_NewFrame(0.7f, 0.55f, 0.25f, 1.f);
	const FLinearColor BraceletUI_NewFramePicked(1.f, 0.85f, 0.4f, 1.f);

	UTextBlock* BraceletUI_MakeText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		return Block;
	}

	UButton* BraceletUI_MakeButton(UWidgetTree* Tree, const FLinearColor& Base)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(Base);
		Style.Hovered.TintColor = FSlateColor(Base * 1.5f);
		Style.Pressed.TintColor = FSlateColor(Base * 0.7f);
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.09f, 0.095f, 0.11f, 1.f));
		Button->SetStyle(Style);
		return Button;
	}

	// 투명 버튼 — 상자 전체를 누르게 할 때(효과 선택) / 자물쇠 아이콘
	UButton* BraceletUI_MakeClearButton(UWidgetTree* Tree)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		const FLinearColor Clear(1.f, 1.f, 1.f, 0.f);
		Style.Normal.TintColor = FSlateColor(Clear);
		Style.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.06f));
		Style.Pressed.TintColor = FSlateColor(Clear);
		Style.Disabled.TintColor = FSlateColor(Clear);
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Button->SetStyle(Style);
		return Button;
	}

	// 등급 색 — 기본 효과·전투 특성은 범위 3등분, 특수 효과는 하·중·상 색
	FLinearColor BraceletUI_LineColor(const FBraceletEffectRoll& Roll)
	{
		return LoAPolish::GetGradeColor(LoABracelet::GetGrade(Roll));
	}

	// 효과 상자 — 테두리 + 짙은 남색 안 + 세로 줄 상자, 최소 높이
	UBorder* BraceletUI_MakeBox(UWidgetTree* Tree, float MinHeight, UVerticalBox*& OutLines)
	{
		UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(BraceletUI_BoxFrame);
		Frame->SetPadding(FMargin(1.f));
		USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetMinDesiredHeight(MinHeight);
		Frame->SetContent(Size);
		UBorder* Inner = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Inner->SetBrushColor(BraceletUI_BoxColor);
		Inner->SetPadding(FMargin(10.f, 6.f));
		Inner->SetVerticalAlignment(VAlign_Top);
		Size->SetContent(Inner);
		OutLines = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Inner->SetContent(OutLines);
		return Frame;
	}

	FString BraceletUI_Number(int64 Value) { return LoAItems::FormatWithCommas(Value); }
}

// ═══ 트리 ════════════════════════════════════════════════════════════════════

TSharedRef<SWidget> UBraceletEnchantWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BraceletRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BraceletPanel"));
		Panel->SetBrushColor(FLinearColor(0.32f, 0.27f, 0.17f, 1.f));
		Panel->SetPadding(FMargin(1.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetPosition(FVector2D(0.f, -50.f));
		}
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(1120.f);
		Size->SetHeightOverride(620.f);
		Panel->SetContent(Size);
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrushColor(BraceletUI_PanelColor);
		Back->SetPadding(FMargin(10.f));
		Size->SetContent(Back);

		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Back->SetContent(Layers);
		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UOverlaySlot* S = Layers->AddChildToOverlay(Columns)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildLeftColumn())) S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildCenterColumn())) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildRightColumn())) S->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));

		ChoicePopup = BuildChoicePopup();
		ChoicePopup->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Layers->AddChildToOverlay(ChoicePopup)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

UWidget* UBraceletEnchantWidget::BuildLeftColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(290.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Back->SetBrushColor(BraceletUI_ColumnColor);
	Back->SetPadding(FMargin(6.f));
	Box->SetContent(Back);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Back->SetContent(Column);
	UBorder* Filter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Filter->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.1f, 1.f));
	Filter->SetPadding(FMargin(8.f, 4.f));
	Filter->SetContent(BraceletUI_MakeText(WidgetTree, 10, BraceletUI_TextColor, NSLOCTEXT("Bracelet", "Filter", "☰  전체")));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Filter)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(ListBox);
	return Box;
}

UWidget* UBraceletEnchantWidget::MakeCostRow(const FText& Label, UTextBlock*& OutValue)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(110.f);
	LabelBox->SetContent(BraceletUI_MakeText(WidgetTree, 10, BraceletUI_DimColor, Label));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) S->SetVerticalAlignment(VAlign_Center);
	OutValue = BraceletUI_MakeText(WidgetTree, 11, BraceletUI_TextColor);
	OutValue->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutValue)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetHorizontalAlignment(HAlign_Right); }
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* Texture = SillingIcon.LoadSynchronous()) Coin->SetBrushFromTexture(Texture);
	Coin->SetDesiredSizeOverride(FVector2D(15.f, 15.f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Coin)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f)); }
	return Row;
}

UWidget* UBraceletEnchantWidget::BuildCenterColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeaderBox->SetHeightOverride(30.f);
	UOverlay* HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	HeaderBox->SetContent(HeaderOverlay);
	if (UOverlaySlot* S = HeaderOverlay->AddChildToOverlay(BraceletUI_MakeText(WidgetTree, 15, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Bracelet", "Title", "팔찌 효과 부여"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Column->AddChildToVerticalBox(HeaderBox);

	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(56.f);
	IconBox->SetHeightOverride(56.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconFrame->SetBrushColor(FLinearColor(1.f, 0.78f, 0.3f, 1.f));
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	BraceletIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	IconFrame->SetContent(BraceletIcon);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(IconBox)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 4.f)); }
	BraceletName = BraceletUI_MakeText(WidgetTree, 13, FLinearColor(1.f, 0.58f, 0.22f, 1.f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BraceletName)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f)); }

	Column->AddChildToVerticalBox(BraceletUI_MakeText(WidgetTree, 11, BraceletUI_LabelColor, NSLOCTEXT("Bracelet", "Fixed", "고정 효과")));
	UVerticalBox* FixedLines = nullptr;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BraceletUI_MakeBox(WidgetTree, 70.f, FixedLines))) S->SetPadding(FMargin(0.f, 3.f, 0.f, 10.f));
	FixedBox = FixedLines;

	Column->AddChildToVerticalBox(BraceletUI_MakeText(WidgetTree, 11, BraceletUI_TextColor, NSLOCTEXT("Bracelet", "Enchant", "부여 효과")));
	UVerticalBox* EnchantLinesBox = nullptr;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BraceletUI_MakeBox(WidgetTree, 80.f, EnchantLinesBox))) S->SetPadding(FMargin(0.f, 3.f, 0.f, 14.f));
	EnchantBox = EnchantLinesBox;

	USizeBox* CostBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CostBox->SetWidthOverride(320.f);
	UVerticalBox* Costs = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CostBox->SetContent(Costs);
	UTextBlock* Cost = nullptr;
	UTextBlock* Have = nullptr;
	if (UVerticalBoxSlot* S = Costs->AddChildToVerticalBox(MakeCostRow(NSLOCTEXT("Bracelet", "Cost", "효과 부여 비용"), Cost))) S->SetPadding(FMargin(0.f, 1.f));
	if (UVerticalBoxSlot* S = Costs->AddChildToVerticalBox(MakeCostRow(NSLOCTEXT("Bracelet", "Have", "소지금"), Have))) S->SetPadding(FMargin(0.f, 1.f));
	CostText = Cost;
	HaveText = Have;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CostBox)) S->SetHorizontalAlignment(HAlign_Center);

	ChancesText = BraceletUI_MakeText(WidgetTree, 12, BraceletUI_GoldColor);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ChancesText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f)); }

	USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonBox->SetWidthOverride(170.f);
	ButtonBox->SetHeightOverride(40.f);
	EnchantButton = BraceletUI_MakeButton(WidgetTree, FLinearColor(0.2f, 0.21f, 0.25f, 1.f));
	EnchantButtonText = BraceletUI_MakeText(WidgetTree, 12, BraceletUI_TextColor);
	EnchantButtonText->SetJustification(ETextJustify::Center);
	EnchantButton->SetContent(EnchantButtonText);
	EnchantButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleEnchant);
	ButtonBox->SetContent(EnchantButton);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ButtonBox)) S->SetHorizontalAlignment(HAlign_Center);

	ResultText = BraceletUI_MakeText(WidgetTree, 11, BraceletUI_GoldColor);
	ResultText->SetAutoWrapText(true);
	ResultText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ResultText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
	return Column;
}

UWidget* UBraceletEnchantWidget::BuildRightColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(300.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Back->SetBrushColor(BraceletUI_ColumnColor);
	Back->SetPadding(FMargin(8.f));
	Box->SetContent(Back);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Back->SetContent(Column);

	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BraceletUI_MakeText(WidgetTree, 13, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Bracelet", "RightTitle", "부여 가능한 효과"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
	}
	UTextBlock* Sub = BraceletUI_MakeText(WidgetTree, 9, BraceletUI_TextColor, NSLOCTEXT("Bracelet", "RightSub", "효과 종류 별 제한 개수 이내 무작위 부여\n기본 효과 2 · 전투 특성 2 · 특수 효과 4"));
	Sub->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Sub)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// 탭 3개
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Tabs)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	const FText TabNames[3] = { NSLOCTEXT("Bracelet", "TabBase", "기본 효과"), NSLOCTEXT("Bracelet", "TabCombat", "전투 특성"), NSLOCTEXT("Bracelet", "TabSpecial", "특수 효과") };
	TabBorders.Reset();
	for (int32 i = 0; i < 3; ++i)
	{
		USizeBox* TabBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TabBox->SetHeightOverride(26.f);
		UButton* Button = BraceletUI_MakeButton(WidgetTree, FLinearColor(0.08f, 0.09f, 0.12f, 1.f));
		UBorder* Tab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tab->SetHorizontalAlignment(HAlign_Center);
		Tab->SetVerticalAlignment(VAlign_Center);
		Tab->SetContent(BraceletUI_MakeText(WidgetTree, 10, BraceletUI_TextColor, TabNames[i]));
		Button->SetContent(Tab);
		TabBox->SetContent(Button);
		if (i == 0) Button->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleTab0);
		else if (i == 1) Button->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleTab1);
		else Button->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleTab2);
		if (UHorizontalBoxSlot* S = Tabs->AddChildToHorizontalBox(TabBox)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(1.f, 0.f)); }
		TabBorders.Add(Tab);
	}

	EffectList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(EffectList)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Box;
}

UWidget* UBraceletEnchantWidget::BuildChoicePopup()
{
	// 로아 [효과 선택] — 창 전체를 어둡게 덮고 가운데: 팔찌 아이콘·이름 / "효과 선택" / 노란 안내 / 기존·신규 상자 / [선택]
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BraceletChoiceDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.78f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	Dim->SetVisibility(ESlateVisibility::Visible);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(780.f);
	Dim->SetContent(Size);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Size->SetContent(Column);

	// 팔찌 아이콘 + 이름
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(56.f);
	IconBox->SetHeightOverride(56.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconFrame->SetBrushColor(FLinearColor(1.f, 0.78f, 0.3f, 1.f));
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	ChoiceIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	IconFrame->SetContent(ChoiceIcon);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(IconBox)) S->SetHorizontalAlignment(HAlign_Center);
	ChoiceName = BraceletUI_MakeText(WidgetTree, 12, BraceletUI_TextColor);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ChoiceName)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 4.f, 0.f, 14.f)); }

	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BraceletUI_MakeText(WidgetTree, 15, FLinearColor(0.95f, 0.92f, 0.85f, 1.f), NSLOCTEXT("Bracelet", "ChoiceTitle", "효과 선택"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BraceletUI_MakeText(WidgetTree, 10, BraceletUI_YellowColor, NSLOCTEXT("Bracelet", "ChoiceSub", "효과를 변환하거나 유지할 수 있습니다."))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
	}

	UHorizontalBox* Pair = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Pair);

	// 상자 하나 — 머리 띠(제목) + 효과 줄, 상자 전체가 투명 버튼(누르면 그쪽 선택)
	auto MakeSide = [this, Pair](const FText& Title, bool bNew, UBorder*& OutFrame, UVerticalBox*& OutLines) -> UVerticalBox*
	{
		UVerticalBox* Side = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Pair->AddChildToHorizontalBox(Side)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(8.f, 0.f)); }

		UButton* Pick = BraceletUI_MakeClearButton(WidgetTree);
		if (bNew) Pick->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandlePickNew);
		else Pick->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandlePickOld);
		Side->AddChildToVerticalBox(Pick);

		OutFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		OutFrame->SetBrushColor(bNew ? BraceletUI_NewFrame : BraceletUI_OldFrame);
		OutFrame->SetPadding(FMargin(1.f));
		Pick->SetContent(OutFrame);
		UVerticalBox* Inside = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		OutFrame->SetContent(Inside);
		UBorder* Head = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Head->SetBrushColor(bNew ? FLinearColor(0.35f, 0.26f, 0.08f, 1.f) : FLinearColor(0.08f, 0.2f, 0.33f, 1.f));
		Head->SetPadding(FMargin(0.f, 5.f));
		Head->SetHorizontalAlignment(HAlign_Center);
		Head->SetContent(BraceletUI_MakeText(WidgetTree, 11, bNew ? BraceletUI_YellowColor : BraceletUI_LabelColor, Title));
		Inside->AddChildToVerticalBox(Head);
		USizeBox* LinesSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LinesSize->SetMinDesiredHeight(150.f);
		UBorder* LinesBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LinesBack->SetBrushColor(BraceletUI_BoxColor);
		LinesBack->SetPadding(FMargin(10.f, 8.f));
		LinesBack->SetVerticalAlignment(VAlign_Top);
		LinesSize->SetContent(LinesBack);
		Inside->AddChildToVerticalBox(LinesSize);
		OutLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LinesBack->SetContent(OutLines);
		return Side;
	};

	UBorder* OldFrame = nullptr;
	UBorder* NewFrame = nullptr;
	UVerticalBox* OldLines = nullptr;
	UVerticalBox* NewLines = nullptr;
	UVerticalBox* OldSide = MakeSide(NSLOCTEXT("Bracelet", "Old", "기존 효과"), false, OldFrame, OldLines);
	UVerticalBox* NewSide = MakeSide(NSLOCTEXT("Bracelet", "New", "신규 효과"), true, NewFrame, NewLines);
	ChoiceOldFrame = OldFrame;
	ChoiceNewFrame = NewFrame;
	ChoiceOldBox = OldLines;
	ChoiceNewBox = NewLines;

	// 기존 쪽 아래 — 자물쇠 + "잠긴 효과와 고정 효과는 변환 시에도 유지됩니다."
	UHorizontalBox* KeepNote = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = OldSide->AddChildToVerticalBox(KeepNote)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f)); }
	UImage* NoteLock = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* Texture = LockIcon.LoadSynchronous()) NoteLock->SetBrushFromTexture(Texture);
	NoteLock->SetDesiredSizeOverride(FVector2D(14.f, 14.f));
	if (UHorizontalBoxSlot* S = KeepNote->AddChildToHorizontalBox(NoteLock)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f)); }
	KeepNote->AddChildToHorizontalBox(BraceletUI_MakeText(WidgetTree, 10, BraceletUI_TextColor, NSLOCTEXT("Bracelet", "KeepNote", "잠긴 효과와 고정 효과는 변환 시에도 유지됩니다.")));

	// 신규 쪽 아래 — 재변환 비용 60,000 (실링) + [재변환 (N회 남음)]
	UHorizontalBox* RerollRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = NewSide->AddChildToVerticalBox(RerollRow)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	if (UHorizontalBoxSlot* S = RerollRow->AddChildToHorizontalBox(BraceletUI_MakeText(WidgetTree, 10, BraceletUI_DimColor, NSLOCTEXT("Bracelet", "RerollCost", "재변환 비용"))))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* S = RerollRow->AddChildToHorizontalBox(BraceletUI_MakeText(WidgetTree, 11, BraceletUI_YellowColor,
		FText::FromString(BraceletUI_Number(LoABracelet::RerollSillingCost)))))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* Texture = SillingIcon.LoadSynchronous()) Coin->SetBrushFromTexture(Texture);
	Coin->SetDesiredSizeOverride(FVector2D(15.f, 15.f));
	if (UHorizontalBoxSlot* S = RerollRow->AddChildToHorizontalBox(Coin)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(4.f, 0.f, 12.f, 0.f)); }
	USizeBox* RerollBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RerollBox->SetWidthOverride(170.f);
	RerollBox->SetHeightOverride(30.f);
	RerollButton = BraceletUI_MakeButton(WidgetTree, FLinearColor(0.2f, 0.21f, 0.25f, 1.f));
	RerollText = BraceletUI_MakeText(WidgetTree, 10, BraceletUI_TextColor);
	RerollText->SetJustification(ETextJustify::Center);
	RerollButton->SetContent(RerollText);
	RerollButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleReroll);
	RerollBox->SetContent(RerollButton);
	if (UHorizontalBoxSlot* S = RerollRow->AddChildToHorizontalBox(RerollBox)) S->SetVerticalAlignment(VAlign_Center);

	// [선택]
	USizeBox* SelectBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SelectBox->SetWidthOverride(130.f);
	SelectBox->SetHeightOverride(36.f);
	SelectButton = BraceletUI_MakeButton(WidgetTree, FLinearColor(0.55f, 0.42f, 0.12f, 1.f));
	UTextBlock* SelectText = BraceletUI_MakeText(WidgetTree, 12, BraceletUI_TextColor, NSLOCTEXT("Bracelet", "Select", "선택"));
	SelectText->SetJustification(ETextJustify::Center);
	SelectButton->SetContent(SelectText);
	SelectButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleSelect);
	SelectBox->SetContent(SelectButton);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(SelectBox)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f)); }
	return Dim;
}

// ═══ 열기·닫기 ════════════════════════════════════════════════════════════════

void UBraceletEnchantWidget::NativeDestruct()
{
	CloseEnchant();
	Super::NativeDestruct();
}

void UBraceletEnchantWidget::OpenEnchant(UInventoryComponent* InInventory, UEquipmentComponent* InEquipment)
{
	if (!Panel) TakeWidget();
	CloseEnchant();
	Inventory = InInventory;
	Equipment = InEquipment;
	if (InInventory)
	{
		InventoryChangedHandle = InInventory->OnSlotChanged.AddUObject(this, &UBraceletEnchantWidget::HandleInventoryChanged);
		SillingChangedHandle = InInventory->OnSillingChanged.AddUObject(this, &UBraceletEnchantWidget::HandleSillingChanged);
	}
	if (InEquipment)
	{
		EquipmentChangedHandle = InEquipment->OnEquipmentChanged.AddUObject(this, &UBraceletEnchantWidget::HandleEquipmentChanged);
	}
	const TArray<FGuid> Ids = LoAGearList::CollectWorkable(InInventory, EEquipSlot::Bracelet);
	SelectedId = Ids.Num() > 0 ? Ids[0] : FGuid();
	Locked.Reset();
	SetResult(FText::GetEmpty(), BraceletUI_GoldColor);
	SetTab(0);
	Refresh();
}

void UBraceletEnchantWidget::CloseEnchant()
{
	// 고르는 중에 닫으면 기존 효과 유지 (실링·횟수는 이미 씀 — 로아와 같음)
	CloseChoice();
	WindowDrag.End();
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->OnSlotChanged.Remove(InventoryChangedHandle);
		Inv->OnSillingChanged.Remove(SillingChangedHandle);
	}
	if (UEquipmentComponent* Equip = Equipment.Get()) Equip->OnEquipmentChanged.Remove(EquipmentChangedHandle);
	InventoryChangedHandle.Reset();
	SillingChangedHandle.Reset();
	EquipmentChangedHandle.Reset();
	Inventory.Reset();
	Equipment.Reset();
	SelectedId.Invalidate();
}

void UBraceletEnchantWidget::CloseChoice()
{
	bChoosing = false;
	ChoicePick = INDEX_NONE;
	ChoiceBefore.Reset();
	PendingProposal.Reset();
	if (ChoicePopup) ChoicePopup->SetVisibility(ESlateVisibility::Collapsed);
}

bool UBraceletEnchantWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UBraceletEnchantWidget::SelectBracelet(const FGuid& Id)
{
	if (bChoosing || Id == SelectedId) return;
	SelectedId = Id;
	Locked.Reset();
	SetResult(FText::GetEmpty(), BraceletUI_GoldColor);
	Refresh();
}

void UBraceletEnchantWidget::SetResult(const FText& Text, const FLinearColor& Color)
{
	if (!ResultText) return;
	ResultText->SetText(Text);
	ResultText->SetColorAndOpacity(FSlateColor(Color));
}

void UBraceletEnchantWidget::SetTab(int32 Tab)
{
	CurrentTab = FMath::Clamp(Tab, 0, 2);
	for (int32 i = 0; i < TabBorders.Num(); ++i)
	{
		TabBorders[i]->SetBrushColor(i == CurrentTab ? FLinearColor(0.2f, 0.3f, 0.42f, 1.f) : FLinearColor(0.f, 0.f, 0.f, 0.f));
	}
	RefreshEffectTab();
}

void UBraceletEnchantWidget::ToggleLock(int32 Index)
{
	if (bChoosing || !Locked.IsValidIndex(Index)) return;
	Locked[Index] = !Locked[Index];
	Refresh();
}

// ═══ 갱신 ═══════════════════════════════════════════════════════════════════

void UBraceletEnchantWidget::RefreshList()
{
	const UInventoryComponent* Inv = Inventory.Get();
	// 소지품에서 부여 횟수가 남은 팔찌만 — 다 쓴 팔찌는 목록에선 빠지고 가운데엔 결과가 남는다
	const TArray<FGuid> Ids = LoAGearList::CollectWorkable(Inv, EEquipSlot::Bracelet);
	while (ListRows.Num() < Ids.Num())
	{
		UGearListRowWidget* Row = WidgetTree->ConstructWidget<UGearListRowWidget>(UGearListRowWidget::StaticClass());
		Row->OnClicked.BindUObject(this, &UBraceletEnchantWidget::SelectBracelet);
		if (UVerticalBoxSlot* S = ListBox->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		ListRows.Add(Row);
	}
	for (int32 i = 0; i < ListRows.Num(); ++i)
	{
		UGearListRowWidget* Row = ListRows[i];
		const FEquipmentInstance* Bracelet = Ids.IsValidIndex(i) && Inv ? Inv->FindEquipment(Ids[i]) : nullptr;
		const FItemDefinition* Def = Bracelet ? LoAItems::Find(Bracelet->ItemId) : nullptr;
		if (!Def)
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Row->SetVisibility(ESlateVisibility::Visible);
		// 윗줄 — ▣ 고정 효과 수 + ◎ 부여 칸 수
		FString Top;
		for (int32 k = 0; k < LoABracelet::GetFixedCount(*Bracelet); ++k) Top += TEXT("▣");
		for (int32 k = 0; k < Bracelet->BraceletEnchantSlots; ++k) Top += TEXT("◎");
		Row->SetRow(Ids[i], Def->Icon.LoadSynchronous(), FText::FromString(Top), Def->DisplayName,
			FText::FromString(FString::Printf(TEXT("%d회 가능"), Bracelet->BraceletEnchantsLeft)), Ids[i] == SelectedId);
	}
}

void UBraceletEnchantWidget::FillLines(UVerticalBox* Box, const TArray<FBraceletEffectRoll>& Effects, int32 SlotCount, const TArray<bool>* InLocked, bool bLockButtons)
{
	Box->ClearChildren();
	UTexture2D* LockTexture = LockIcon.LoadSynchronous();
	UTexture2D* UnlockTexture = UnlockIcon.LoadSynchronous();
	const int32 Count = FMath::Max(SlotCount, Effects.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const bool bHas = Effects.IsValidIndex(i);
		const bool bFixed = bHas && Effects[i].bFixed;
		const bool bLock = bFixed || (InLocked && InLocked->IsValidIndex(i) && (*InLocked)[i] && bHas);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BraceletUI_MakeText(WidgetTree, 11, bLock ? BraceletUI_LockColor : BraceletUI_DimColor,
			FText::FromString(bFixed ? TEXT("▣") : TEXT("◎")))))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* Label = bHas
			? BraceletUI_MakeText(WidgetTree, 11, BraceletUI_LineColor(Effects[i]), FText::FromString(LoABracelet::Describe(Effects[i])))
			: BraceletUI_MakeText(WidgetTree, 11, BraceletUI_TextColor, NSLOCTEXT("Bracelet", "Empty", "(효과 부여 필요)"));
		Label->SetAutoWrapText(true);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Label))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 오른쪽 끝 자물쇠 — 효과가 있는 부여 줄만 누를 수 있음 (잠금 = 금색 잠긴 자물쇠, 해제 = 회색 열린 자물쇠)
		if (bLockButtons && bHas && !bFixed && i < 4)
		{
			UButton* LockButton = BraceletUI_MakeClearButton(WidgetTree);
			UImage* LockImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (UTexture2D* Texture = bLock ? LockTexture : UnlockTexture) LockImage->SetBrushFromTexture(Texture);
			LockImage->SetDesiredSizeOverride(FVector2D(20.f, 20.f));
			LockButton->SetContent(LockImage);
			LockButton->SetToolTipText(bLock ? NSLOCTEXT("Bracelet", "UnlockTip", "잠금 해제") : NSLOCTEXT("Bracelet", "LockTip", "잠금 — 변환해도 이 효과는 유지됩니다."));
			LockButton->SetIsEnabled(!bChoosing);
			switch (i)
			{
			case 0: LockButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleLock0); break;
			case 1: LockButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleLock1); break;
			case 2: LockButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleLock2); break;
			default: LockButton->OnClicked.AddDynamic(this, &UBraceletEnchantWidget::HandleLock3); break;
			}
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LockButton))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}
		}
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(2.f, 2.f));
	}
}

void UBraceletEnchantWidget::RefreshEffectTab()
{
	if (!EffectList) return;
	EffectList->ClearChildren();
	const LoABracelet::ECategory Category = static_cast<LoABracelet::ECategory>(CurrentTab);
	for (const EBraceletEffect Effect : LoABracelet::GetEffectsIn(Category))
	{
		const FString Text = FString::Printf(TEXT("• %s"), *LoABracelet::DescribeRange(Effect));
		UTextBlock* Line = BraceletUI_MakeText(WidgetTree, 10, BraceletUI_TextColor, FText::FromString(Text));
		Line->SetAutoWrapText(true);
		if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(EffectList->AddChild(Line))) S->SetPadding(FMargin(4.f, 2.f));
	}
}

void UBraceletEnchantWidget::Refresh()
{
	if (!Panel) return;
	RefreshList();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Bracelet = Inv ? Inv->FindEquipment(SelectedId) : nullptr;
	const FItemDefinition* Def = Bracelet ? LoAItems::Find(Bracelet->ItemId) : nullptr;
	HaveText->SetText(FText::FromString(BraceletUI_Number(Inv ? Inv->GetSilling() : 0)));
	HaveText->SetColorAndOpacity(FSlateColor(BraceletUI_GreenColor));
	if (!Def)
	{
		BraceletIcon->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.f));
		BraceletName->SetText(NSLOCTEXT("Bracelet", "None", "효과를 부여할 팔찌가 없습니다."));
		FixedBox->ClearChildren();
		EnchantBox->ClearChildren();
		CostText->SetText(FText::GetEmpty());
		ChancesText->SetText(FText::GetEmpty());
		EnchantButton->SetIsEnabled(false);
		EnchantButtonText->SetText(NSLOCTEXT("Bracelet", "Do", "효과 부여"));
		return;
	}

	if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
	{
		BraceletIcon->SetBrushFromTexture(Texture);
		BraceletIcon->SetColorAndOpacity(FLinearColor::White);
	}
	BraceletName->SetText(Def->DisplayName);

	FillLines(FixedBox, Bracelet->BraceletEffects.FilterByPredicate([](const FBraceletEffectRoll& R) { return R.bFixed; }), 0, nullptr, false);
	const TArray<FBraceletEffectRoll> Enchants = LoABracelet::GetEnchants(*Bracelet);
	Locked.SetNum(Bracelet->BraceletEnchantSlots);
	FillLines(EnchantBox, Enchants, Bracelet->BraceletEnchantSlots, &Locked, true);

	CostText->SetText(FText::FromString(BraceletUI_Number(LoABracelet::EnchantSillingCost)));
	CostText->SetColorAndOpacity(FSlateColor(Inv->GetSilling() >= LoABracelet::EnchantSillingCost ? BraceletUI_GoldColor : BraceletUI_ShortColor));
	ChancesText->SetText(FText::FromString(FString::Printf(TEXT("%d회 부여 가능"), Bracelet->BraceletEnchantsLeft)));
	EnchantButtonText->SetText(Enchants.Num() > 0 ? NSLOCTEXT("Bracelet", "Convert", "효과 변환") : NSLOCTEXT("Bracelet", "Do", "효과 부여"));
	EnchantButton->SetIsEnabled(!bChoosing && Bracelet->BraceletEnchantsLeft > 0 && Bracelet->BraceletEnchantSlots > 0);
	if (bChoosing) RefreshChoice();
}

void UBraceletEnchantWidget::RefreshChoice()
{
	if (!ChoicePopup) return;
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Bracelet = Inv ? Inv->FindEquipment(SelectedId) : nullptr;
	const FItemDefinition* Def = Bracelet ? LoAItems::Find(Bracelet->ItemId) : nullptr;
	if (Def)
	{
		if (UTexture2D* Texture = Def->Icon.LoadSynchronous()) ChoiceIcon->SetBrushFromTexture(Texture);
		ChoiceName->SetText(Def->DisplayName);
	}
	const int32 Slots = Bracelet ? Bracelet->BraceletEnchantSlots : 0;
	FillLines(ChoiceOldBox, ChoiceBefore, Slots, &Locked, false);
	FillLines(ChoiceNewBox, PendingProposal, Slots, &Locked, false);
	ChoiceOldFrame->SetBrushColor(ChoicePick == 0 ? BraceletUI_OldFramePicked : BraceletUI_OldFrame);
	ChoiceOldFrame->SetPadding(FMargin(ChoicePick == 0 ? 3.f : 1.f));
	ChoiceNewFrame->SetBrushColor(ChoicePick == 1 ? BraceletUI_NewFramePicked : BraceletUI_NewFrame);
	ChoiceNewFrame->SetPadding(FMargin(ChoicePick == 1 ? 3.f : 1.f));
	const int32 Left = Bracelet ? Bracelet->BraceletEnchantsLeft : 0;
	RerollText->SetText(FText::FromString(FString::Printf(TEXT("재변환 (%d회 남음)"), Left)));
	RerollButton->SetIsEnabled(Left > 0 && Inv && Inv->GetSilling() >= LoABracelet::RerollSillingCost);
	SelectButton->SetIsEnabled(ChoicePick != INDEX_NONE);
}

// ═══ 부여 ═══════════════════════════════════════════════════════════════════

void UBraceletEnchantWidget::HandleEnchant()
{
	UInventoryComponent* Inv = Inventory.Get();
	FEquipmentInstance* Bracelet = Inv ? Inv->FindEquipmentMutable(SelectedId) : nullptr;
	if (!Bracelet || bChoosing) return;
	const TArray<FBraceletEffectRoll> Before = LoABracelet::GetEnchants(*Bracelet);
	TArray<FBraceletEffectRoll> Proposal;
	const LoABracelet::EEnchantResult Result = LoABracelet::Enchant(*Bracelet, Locked, *Inv, Proposal);
	if (Result == LoABracelet::EEnchantResult::Applied)
	{
		SetResult(FText::FromString(FString::Printf(TEXT("효과 부여 완료 — 효과 변환 %d회 가능합니다."), Bracelet->BraceletEnchantsLeft)), BraceletUI_GoldColor);
		LoAGearList::CommitChange(Inv, Equipment.Get(), SelectedId);
	}
	else if (Result == LoABracelet::EEnchantResult::NeedChoice)
	{
		// 실링·횟수는 이미 썼다 — 저장해 두고 [효과 선택]
		LoAGearList::CommitChange(Inv, Equipment.Get(), SelectedId);
		ChoiceBefore = Before;
		PendingProposal = Proposal;
		ChoicePick = INDEX_NONE;
		bChoosing = true;
		ChoicePopup->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		SetResult(LoABracelet::GetEnchantResultText(Result), BraceletUI_ShortColor);
	}
	Refresh();
}

void UBraceletEnchantWidget::HandleReroll()
{
	UInventoryComponent* Inv = Inventory.Get();
	FEquipmentInstance* Bracelet = Inv ? Inv->FindEquipmentMutable(SelectedId) : nullptr;
	if (!Bracelet || !bChoosing) return;
	TArray<FBraceletEffectRoll> Proposal;
	const LoABracelet::EEnchantResult Result = LoABracelet::Reroll(*Bracelet, Locked, *Inv, Proposal);
	if (Result == LoABracelet::EEnchantResult::NeedChoice)
	{
		LoAGearList::CommitChange(Inv, Equipment.Get(), SelectedId);
		PendingProposal = Proposal;
		ChoicePick = INDEX_NONE;
	}
	else
	{
		SetResult(LoABracelet::GetEnchantResultText(Result), BraceletUI_ShortColor);
	}
	Refresh();
}

void UBraceletEnchantWidget::HandleSelect()
{
	UInventoryComponent* Inv = Inventory.Get();
	FEquipmentInstance* Bracelet = Inv ? Inv->FindEquipmentMutable(SelectedId) : nullptr;
	if (!bChoosing || ChoicePick == INDEX_NONE) return;
	if (Bracelet && ChoicePick == 1)
	{
		LoABracelet::ApplyChoice(*Bracelet, PendingProposal, true);
		LoAGearList::CommitChange(Inv, Equipment.Get(), SelectedId);
		SetResult(NSLOCTEXT("Bracelet", "Taken", "신규 효과를 적용했습니다."), BraceletUI_GreenColor);
	}
	else
	{
		SetResult(NSLOCTEXT("Bracelet", "Kept", "기존 효과를 유지했습니다."), BraceletUI_TextColor);
	}
	CloseChoice();
	Refresh();
}

// ── 창 끌기 / 클릭 차단 ──

FReply UBraceletEnchantWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox && !bChoosing
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UBraceletEnchantWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBraceletEnchantWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UBraceletEnchantWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UBraceletEnchantWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
