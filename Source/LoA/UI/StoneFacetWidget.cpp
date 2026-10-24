#include "UI/StoneFacetWidget.h"
#include "UI/GearListRowWidget.h"
#include "Item/BraceletStone.h"
#include "Item/BuildTypes.h"
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
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FLinearColor StoneUI_PanelColor(0.022f, 0.027f, 0.04f, 0.97f);
	const FLinearColor StoneUI_ColumnColor(0.035f, 0.042f, 0.06f, 1.f);
	const FLinearColor StoneUI_LineColor(0.05f, 0.058f, 0.08f, 1.f);
	const FLinearColor StoneUI_TextColor(0.88f, 0.88f, 0.9f, 1.f);
	const FLinearColor StoneUI_DimColor(0.5f, 0.51f, 0.55f, 1.f);
	const FLinearColor StoneUI_GoldColor(1.f, 0.82f, 0.4f, 1.f);
	const FLinearColor StoneUI_GreenColor(0.55f, 0.95f, 0.45f, 1.f);
	const FLinearColor StoneUI_PlusColor(0.35f, 0.75f, 1.f, 1.f);   // 증가 능력 — 로아 세공 창 파랑
	const FLinearColor StoneUI_MinusColor(1.f, 0.35f, 0.32f, 1.f);  // 감소 능력 — 빨강
	const FLinearColor StoneUI_FailColor(0.32f, 0.33f, 0.37f, 1.f);
	const FLinearColor StoneUI_ShortColor(1.f, 0.35f, 0.3f, 1.f);

	UTextBlock* StoneUI_MakeText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		return Block;
	}

	void StoneUI_MakeCircle(UImage* Image, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Color);
	}

	UButton* StoneUI_MakeButton(UWidgetTree* Tree, const FLinearColor& Base)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(Base);
		Style.Hovered.TintColor = FSlateColor(Base * 1.5f);
		Style.Pressed.TintColor = FSlateColor(Base * 0.7f);
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.09f, 0.095f, 0.11f, 1.f));
		Style.NormalPadding = FMargin(2.f);
		Style.PressedPadding = FMargin(2.f);
		Button->SetStyle(Style);
		return Button;
	}

	FString StoneUI_Number(int64 Value) { return LoAItems::FormatWithCommas(Value); }
}

// ═══ 트리 ════════════════════════════════════════════════════════════════════

TSharedRef<SWidget> UStoneFacetWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StoneRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StonePanel"));
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
		Size->SetWidthOverride(1080.f);
		Size->SetHeightOverride(660.f);
		Panel->SetContent(Size);
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrushColor(StoneUI_PanelColor);
		Back->SetPadding(FMargin(10.f));
		Size->SetContent(Back);

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Back->SetContent(Columns);
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildLeftColumn())) S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildCenterColumn())) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

UWidget* UStoneFacetWidget::BuildLeftColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(290.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Back->SetBrushColor(StoneUI_ColumnColor);
	Back->SetPadding(FMargin(6.f));
	Box->SetContent(Back);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Back->SetContent(Column);

	UBorder* Filter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Filter->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.1f, 1.f));
	Filter->SetPadding(FMargin(8.f, 4.f));
	Filter->SetContent(StoneUI_MakeText(WidgetTree, 10, StoneUI_TextColor, NSLOCTEXT("Stone", "Filter", "☰  전체")));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Filter)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(ListBox);
	return Box;
}

UWidget* UStoneFacetWidget::BuildSectionHeader(const FText& Title, const FLinearColor& Color, UTextBlock*& OutRight)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(StoneUI_MakeText(WidgetTree, 11, Color, Title)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	OutRight = StoneUI_MakeText(WidgetTree, 11, StoneUI_TextColor);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutRight)) S->SetVerticalAlignment(VAlign_Center);
	return Row;
}

UWidget* UStoneFacetWidget::BuildLine(int32 LineIndex, FLineWidgets& Out)
{
	const bool bNegative = LoAStone::IsNegativeLine(LineIndex);
	const FLinearColor Color = bNegative ? StoneUI_MinusColor : StoneUI_PlusColor;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// 각인 원 아이콘
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(64.f);
	IconBox->SetHeightOverride(64.f);
	UOverlay* IconLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	IconBox->SetContent(IconLayers);
	UImage* Ring = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	StoneUI_MakeCircle(Ring, FLinearColor(0.25f, 0.26f, 0.3f, 1.f));
	if (UOverlaySlot* S = IconLayers->AddChildToOverlay(Ring)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Out.Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UOverlaySlot* S = IconLayers->AddChildToOverlay(Out.Icon)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(4.f)); }
	Out.IconText = StoneUI_MakeText(WidgetTree, 12, FLinearColor::White);
	if (UOverlaySlot* S = IconLayers->AddChildToOverlay(Out.IconText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }

	// 가운데 막대 — 제목 줄 + 10칸
	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bar->SetBrushColor(StoneUI_LineColor);
	Bar->SetPadding(FMargin(10.f, 4.f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Bar)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }
	UVerticalBox* BarColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Bar->SetContent(BarColumn);
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BarColumn->AddChildToVerticalBox(TitleRow);
	Out.Title = StoneUI_MakeText(WidgetTree, 11, bNegative ? StoneUI_MinusColor : StoneUI_TextColor);
	if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(Out.Title)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Out.Count = StoneUI_MakeText(WidgetTree, 10, Color);
	TitleRow->AddChildToHorizontalBox(Out.Count);

	UHorizontalBox* CellRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = BarColumn->AddChildToVerticalBox(CellRow)) S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	for (int32 c = 0; c < LoAStone::FacetsPerLine; ++c)
	{
		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = CellRow->AddChildToHorizontalBox(Cell)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* Glyph = StoneUI_MakeText(WidgetTree, 17, Color);
		Glyph->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Glyph)) S->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Label = StoneUI_MakeText(WidgetTree, 8, StoneUI_DimColor);
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Label)) S->SetHorizontalAlignment(HAlign_Center);
		Out.Cells.Add(Glyph);
		Out.CellLabels.Add(Label);
	}

	// 세공 버튼 — "세공" + 비용
	USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonBox->SetWidthOverride(86.f);
	ButtonBox->SetHeightOverride(62.f);
	Out.Button = StoneUI_MakeButton(WidgetTree, bNegative ? FLinearColor(0.3f, 0.12f, 0.12f, 1.f) : FLinearColor(0.2f, 0.22f, 0.27f, 1.f));
	ButtonBox->SetContent(Out.Button);
	UVerticalBox* ButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Out.Button->SetContent(ButtonColumn);
	if (UVerticalBoxSlot* S = ButtonColumn->AddChildToVerticalBox(StoneUI_MakeText(WidgetTree, 16, StoneUI_TextColor, FText::FromString(TEXT("세공")))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
	Out.Cost = StoneUI_MakeText(WidgetTree, 9, StoneUI_TextColor);
	if (UVerticalBoxSlot* S = ButtonColumn->AddChildToVerticalBox(Out.Cost)) S->SetHorizontalAlignment(HAlign_Center);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ButtonBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f)); }
	if (LineIndex == 0) Out.Button->OnClicked.AddDynamic(this, &UStoneFacetWidget::HandleFacet0);
	else if (LineIndex == 1) Out.Button->OnClicked.AddDynamic(this, &UStoneFacetWidget::HandleFacet1);
	else Out.Button->OnClicked.AddDynamic(this, &UStoneFacetWidget::HandleFacet2);
	return Row;
}

UWidget* UStoneFacetWidget::BuildCenterColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeaderBox->SetHeightOverride(30.f);
	UOverlay* HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	HeaderBox->SetContent(HeaderOverlay);
	if (UOverlaySlot* S = HeaderOverlay->AddChildToOverlay(StoneUI_MakeText(WidgetTree, 15, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Stone", "Title", "어빌리티 스톤 세공"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Column->AddChildToVerticalBox(HeaderBox);

	// 스톤 아이콘 + 이름
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(56.f);
	IconBox->SetHeightOverride(56.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconFrame->SetBrushColor(FLinearColor(1.f, 0.78f, 0.3f, 1.f));
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	StoneIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	IconFrame->SetContent(StoneIcon);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(IconBox)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 4.f)); }
	StoneName = StoneUI_MakeText(WidgetTree, 12, StoneUI_TextColor);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StoneName)) S->SetHorizontalAlignment(HAlign_Center);

	// 소지금 (오른쪽)
	// 소지금 (실링 — 로아 세공 창의 은화)
	UHorizontalBox* MoneyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MoneyRow)) { S->SetHorizontalAlignment(HAlign_Right); S->SetPadding(FMargin(0.f, 4.f, 6.f, 6.f)); }
	if (UHorizontalBoxSlot* S = MoneyRow->AddChildToHorizontalBox(StoneUI_MakeText(WidgetTree, 10, StoneUI_TextColor, NSLOCTEXT("Stone", "Money", "소지금")))) S->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
	SillingText = StoneUI_MakeText(WidgetTree, 10, StoneUI_GreenColor);
	MoneyRow->AddChildToHorizontalBox(SillingText);
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* Texture = SillingIcon.LoadSynchronous()) Coin->SetBrushFromTexture(Texture);
	Coin->SetDesiredSizeOverride(FVector2D(15.f, 15.f));
	if (UHorizontalBoxSlot* S = MoneyRow->AddChildToHorizontalBox(Coin)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f)); }

	// 증가 능력 2줄 / 감소 능력 1줄
	Lines.SetNum(LoAStone::LineCount);
	UTextBlock* PlusRight = nullptr;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BuildSectionHeader(NSLOCTEXT("Stone", "Plus", "◈ 증가 능력"), StoneUI_PlusColor, PlusRight))) S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	PlusChanceText = PlusRight;
	for (int32 i = 0; i < 2; ++i)
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BuildLine(i, Lines[i]))) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
	UTextBlock* MinusRight = nullptr;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BuildSectionHeader(NSLOCTEXT("Stone", "Minus", "◈ 감소 능력"), StoneUI_MinusColor, MinusRight))) S->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
	MinusChanceText = MinusRight;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BuildLine(2, Lines[2]))) S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	// 아래 — 세공 성공 총 N회 / 자동 세공 / 총 Lv 달성
	UBorder* BottomBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BottomBack->SetBrushColor(StoneUI_ColumnColor);
	BottomBack->SetPadding(FMargin(14.f, 8.f));
	Column->AddChildToVerticalBox(BottomBack);
	UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BottomBack->SetContent(Bottom);

	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(Left)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }
	TotalSuccessText = StoneUI_MakeText(WidgetTree, 13, StoneUI_TextColor);
	if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(TotalSuccessText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f)); }
	for (int32 i = 0; i < 3; ++i)
	{
		UTextBlock* Line = StoneUI_MakeText(WidgetTree, 9, StoneUI_DimColor);
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(Line)) S->SetHorizontalAlignment(HAlign_Center);
		BonusLines.Add(Line);
	}

	USizeBox* AutoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AutoBox->SetWidthOverride(120.f);
	AutoBox->SetHeightOverride(96.f);
	AutoButton = StoneUI_MakeButton(WidgetTree, FLinearColor(0.08f, 0.28f, 0.5f, 1.f));
	AutoBox->SetContent(AutoButton);
	UVerticalBox* AutoColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AutoButton->SetContent(AutoColumn);
	if (UVerticalBoxSlot* S = AutoColumn->AddChildToVerticalBox(StoneUI_MakeText(WidgetTree, 24, FLinearColor(0.55f, 0.85f, 1.f, 1.f), FText::FromString(TEXT("◆"))))) S->SetHorizontalAlignment(HAlign_Center);
	if (UVerticalBoxSlot* S = AutoColumn->AddChildToVerticalBox(StoneUI_MakeText(WidgetTree, 11, StoneUI_TextColor, NSLOCTEXT("Stone", "Auto", "자동 세공")))) S->SetHorizontalAlignment(HAlign_Center);
	AutoCostText = StoneUI_MakeText(WidgetTree, 10, StoneUI_GoldColor);
	if (UVerticalBoxSlot* S = AutoColumn->AddChildToVerticalBox(AutoCostText)) S->SetHorizontalAlignment(HAlign_Center);
	AutoButton->OnClicked.AddDynamic(this, &UStoneFacetWidget::HandleAuto);
	AutoButton->SetToolTipText(NSLOCTEXT("Stone", "AutoTip", "남은 30칸을 한 번에 세공합니다. 증가 두 줄 레벨 합 5를 가장 우선으로, 못 미치면 높은 합이 나오도록 매 칸 줄을 고릅니다.\n세공을 시작한 스톤은 자동 세공을 할 수 없습니다."));
	if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(AutoBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(12.f, 0.f)); }

	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(Right)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetVerticalAlignment(VAlign_Center); }
	TotalLevelText = StoneUI_MakeText(WidgetTree, 13, StoneUI_TextColor);
	if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(TotalLevelText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f)); }
	AttackBonusText = StoneUI_MakeText(WidgetTree, 9, StoneUI_DimColor, NSLOCTEXT("Stone", "AtkBonus", "Lv.5 >> 기본 공격력 +1.50%"));
	if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(AttackBonusText)) S->SetHorizontalAlignment(HAlign_Center);

	ResultText = StoneUI_MakeText(WidgetTree, 11, StoneUI_GoldColor);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ResultText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
	return Column;
}

// ═══ 열기·닫기 ════════════════════════════════════════════════════════════════

void UStoneFacetWidget::NativeDestruct()
{
	CloseFacet();
	Super::NativeDestruct();
}

void UStoneFacetWidget::OpenFacet(UInventoryComponent* InInventory, UEquipmentComponent* InEquipment)
{
	if (!Panel) TakeWidget();
	CloseFacet();
	Inventory = InInventory;
	Equipment = InEquipment;
	if (InInventory)
	{
		InventoryChangedHandle = InInventory->OnSlotChanged.AddUObject(this, &UStoneFacetWidget::HandleInventoryChanged);
		SillingChangedHandle = InInventory->OnSillingChanged.AddUObject(this, &UStoneFacetWidget::HandleSillingChanged);
	}
	if (InEquipment)
	{
		EquipmentChangedHandle = InEquipment->OnEquipmentChanged.AddUObject(this, &UStoneFacetWidget::HandleEquipmentChanged);
	}
	// 목록 = 소지품의 세공 안 됐거나 덜 된 스톤. 처음엔 세공 중인 것 → 없으면 맨 앞
	const TArray<FGuid> Ids = LoAGearList::CollectWorkable(InInventory, EEquipSlot::AbilityStone);
	SelectedId = Ids.Num() > 0 ? Ids[0] : FGuid();
	for (const FGuid& Id : Ids)
	{
		const FEquipmentInstance* Stone = InInventory ? InInventory->FindEquipment(Id) : nullptr;
		if (Stone && LoAStone::IsStarted(*Stone))
		{
			SelectedId = Id;
			break;
		}
	}
	SetResult(FText::GetEmpty(), StoneUI_GoldColor);
	Refresh();
}

void UStoneFacetWidget::CloseFacet()
{
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

bool UStoneFacetWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UStoneFacetWidget::SelectStone(const FGuid& Id)
{
	if (Id == SelectedId) return;
	SelectedId = Id;
	SetResult(FText::GetEmpty(), StoneUI_GoldColor);
	Refresh();
}

void UStoneFacetWidget::SetResult(const FText& Text, const FLinearColor& Color)
{
	if (!ResultText) return;
	ResultText->SetText(Text);
	ResultText->SetColorAndOpacity(FSlateColor(Color));
}

// ═══ 갱신 ═══════════════════════════════════════════════════════════════════

void UStoneFacetWidget::RefreshList()
{
	const UInventoryComponent* Inv = Inventory.Get();
	// 소지품에서 세공이 안 됐거나 덜 된 것만 — 방금 다 깎은 스톤은 목록에선 빠지고 가운데엔 결과가 남는다
	const TArray<FGuid> Ids = LoAGearList::CollectWorkable(Inv, EEquipSlot::AbilityStone);

	// 줄 위젯은 모자란 만큼만 새로 만들고, 남으면 숨긴다
	while (ListRows.Num() < Ids.Num())
	{
		UGearListRowWidget* Row = WidgetTree->ConstructWidget<UGearListRowWidget>(UGearListRowWidget::StaticClass());
		Row->OnClicked.BindUObject(this, &UStoneFacetWidget::SelectStone);
		if (UVerticalBoxSlot* S = ListBox->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		ListRows.Add(Row);
	}
	for (int32 i = 0; i < ListRows.Num(); ++i)
	{
		UGearListRowWidget* Row = ListRows[i];
		const FEquipmentInstance* Stone = Ids.IsValidIndex(i) && Inv ? Inv->FindEquipment(Ids[i]) : nullptr;
		const FItemDefinition* Def = Stone ? LoAItems::Find(Stone->ItemId) : nullptr;
		if (!Def)
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Row->SetVisibility(ESlateVisibility::Visible);
		const FString Top = LoAStone::IsStarted(*Stone) ? TEXT("세공 중") : FString();
		const FString Right = FString::Printf(TEXT("%d칸 남음"), LoAStone::GetRemainingFacets(*Stone));
		Row->SetRow(Ids[i], Def->Icon.LoadSynchronous(), FText::FromString(Top), Def->DisplayName, FText::FromString(Right), Ids[i] == SelectedId);
	}
}

void UStoneFacetWidget::RefreshLine(int32 LineIndex, const FEquipmentInstance* Stone)
{
	FLineWidgets& W = Lines[LineIndex];
	const bool bNegative = LoAStone::IsNegativeLine(LineIndex);
	const FLinearColor Color = bNegative ? StoneUI_MinusColor : StoneUI_PlusColor;
	if (!Stone || !Stone->StoneLines.IsValidIndex(LineIndex))
	{
		W.Title->SetText(FText::GetEmpty());
		W.Count->SetText(FText::GetEmpty());
		for (int32 c = 0; c < W.Cells.Num(); ++c) { W.Cells[c]->SetText(FText::GetEmpty()); W.CellLabels[c]->SetText(FText::GetEmpty()); }
		W.Button->SetIsEnabled(false);
		W.Cost->SetText(FText::GetEmpty());
		W.Icon->SetVisibility(ESlateVisibility::Hidden);
		W.IconText->SetText(FText::GetEmpty());
		return;
	}
	const FStoneLine& Line = Stone->StoneLines[LineIndex];
	const int32 Level = LoAStone::GetLevel(Line.Successes, bNegative);

	// 원 아이콘 — 각인 아이콘(T_Eng_<Id>), 감소 줄은 빨간 원 + 글자
	UTexture2D* Icon = nullptr;
	if (!bNegative)
	{
		if (const FEngravingDefinition* Def = LoAEngravings::Find(Line.Id))
		{
			Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/LostArk/UI/Engraving/%s.%s"), *Def->IconName, *Def->IconName))).LoadSynchronous();
		}
	}
	if (Icon)
	{
		W.Icon->SetBrushFromTexture(Icon);
		W.Icon->SetColorAndOpacity(FLinearColor::White);
		W.Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		W.IconText->SetText(FText::GetEmpty());
	}
	else
	{
		StoneUI_MakeCircle(W.Icon, bNegative ? FLinearColor(0.35f, 0.08f, 0.08f, 1.f) : FLinearColor(0.1f, 0.25f, 0.45f, 1.f));
		W.Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		W.IconText->SetText(FText::FromString(LoAStone::GetLineName(Line, bNegative).Left(2)));
	}

	W.Title->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d %s"), Level, *LoAStone::GetLineName(Line, bNegative))));
	W.Count->SetText(FText::FromString(FString::Printf(TEXT("◆ x %d"), Line.Successes)));

	// 칸 — 성공 ◆(색) · 실패 ◆(회색) · 남음 ◇. 칸 아래 Lv: 이미 오른 단계는 그 칸(금색), 아직 남은 단계는 다 성공했을 때 닿는 칸(흐리게)
	for (int32 c = 0; c < W.Cells.Num(); ++c)
	{
		W.CellLabels[c]->SetText(FText::GetEmpty());
		if (Line.Results.IsValidIndex(c))
		{
			W.Cells[c]->SetText(FText::FromString(TEXT("◆")));
			W.Cells[c]->SetColorAndOpacity(FSlateColor(Line.Results[c] ? Color : StoneUI_FailColor));
		}
		else
		{
			W.Cells[c]->SetText(FText::FromString(TEXT("◇")));
			W.Cells[c]->SetColorAndOpacity(FSlateColor(Color * 0.75f));
		}
	}
	const TArray<int32> Thresholds = LoAStone::GetLevelThresholds(bNegative);
	for (int32 t = 0; t < Thresholds.Num(); ++t)
	{
		const int32 Need = Thresholds[t];
		int32 CellIndex = INDEX_NONE;
		bool bReached = false;
		if (Line.Successes >= Need)
		{
			// Need번째 성공이 나온 칸
			int32 Seen = 0;
			for (int32 c = 0; c < Line.Results.Num(); ++c)
			{
				if (Line.Results[c] && ++Seen == Need) { CellIndex = c; break; }
			}
			bReached = true;
		}
		else
		{
			const int32 Cell = Line.Attempts + (Need - Line.Successes) - 1;
			if (Cell < LoAStone::FacetsPerLine) CellIndex = Cell;
		}
		if (W.CellLabels.IsValidIndex(CellIndex))
		{
			W.CellLabels[CellIndex]->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), t + 1)));
			W.CellLabels[CellIndex]->SetColorAndOpacity(FSlateColor(bReached ? StoneUI_GoldColor : StoneUI_DimColor));
		}
	}

	const UInventoryComponent* Inv = Inventory.Get();
	W.Cost->SetText(FText::FromString(StoneUI_Number(LoAStone::FacetSillingCost) + TEXT(" 실링")));
	W.Button->SetIsEnabled(LoAStone::CanFacet(*Stone, LineIndex) && Inv && Inv->GetSilling() >= LoAStone::FacetSillingCost);
}

void UStoneFacetWidget::Refresh()
{
	if (!Panel) return;
	RefreshList();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Stone = Inv ? Inv->FindEquipment(SelectedId) : nullptr;
	const FItemDefinition* Def = Stone ? LoAItems::Find(Stone->ItemId) : nullptr;
	SillingText->SetText(FText::FromString(StoneUI_Number(Inv ? Inv->GetSilling() : 0)));

	if (!Def)
	{
		StoneIcon->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.f));
		StoneName->SetText(NSLOCTEXT("Stone", "NoStone", "세공할 어빌리티 스톤이 없습니다."));
		PlusChanceText->SetText(FText::GetEmpty());
		MinusChanceText->SetText(FText::GetEmpty());
		for (int32 i = 0; i < Lines.Num(); ++i) RefreshLine(i, nullptr);
		TotalSuccessText->SetText(FText::GetEmpty());
		TotalLevelText->SetText(FText::GetEmpty());
		for (UTextBlock* Line : BonusLines) Line->SetText(FText::GetEmpty());
		AutoButton->SetIsEnabled(false);
		AutoCostText->SetText(FText::GetEmpty());
		return;
	}

	if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
	{
		StoneIcon->SetBrushFromTexture(Texture);
		StoneIcon->SetColorAndOpacity(FLinearColor::White);
	}
	StoneName->SetText(Def->DisplayName);
	const bool bComplete = LoAStone::IsComplete(*Stone);
	const FText Chance = bComplete ? NSLOCTEXT("Stone", "Done", "세공 완료") : FText::FromString(FString::Printf(TEXT("%d%%"), Stone->StoneChance));
	PlusChanceText->SetText(FText::Format(NSLOCTEXT("Stone", "PlusChance", "성공 확률  {0}"), Chance));
	MinusChanceText->SetText(FText::Format(NSLOCTEXT("Stone", "MinusChance", "균열 확률  {0}"), Chance));
	for (int32 i = 0; i < Lines.Num(); ++i) RefreshLine(i, Stone);

	// 세공 성공 총 N회 — 다음 체력 보너스 3줄 (이미 받은 건 초록)
	const int32 Total = LoAStone::GetTotalSuccesses(*Stone);
	TotalSuccessText->SetText(FText::FromString(FString::Printf(TEXT("세공 성공 총 %d회"), Total)));
	const int32 FirstShown = FMath::Max(LoAStone::VitalityFirstSuccesses, FMath::Min(Total, 20 - 2));
	for (int32 i = 0; i < BonusLines.Num(); ++i)
	{
		const int32 Need = FirstShown + i;
		BonusLines[i]->SetText(FText::FromString(FString::Printf(TEXT("%d회 >> 체력 +%s"), Need, *LoAItems::FormatWithCommas(LoAStone::GetVitalityBonus(Need)))));
		BonusLines[i]->SetColorAndOpacity(FSlateColor(Total >= Need ? StoneUI_GreenColor : StoneUI_DimColor));
	}

	const int32 TotalLevel = LoAStone::GetTotalLevel(*Stone);
	TotalLevelText->SetText(FText::FromString(FString::Printf(TEXT("총 Lv.%d 달성"), TotalLevel)));
	AttackBonusText->SetColorAndOpacity(FSlateColor(TotalLevel >= LoAStone::AttackBonusLevel ? StoneUI_GreenColor : StoneUI_DimColor));

	const int64 AutoCost = LoAStone::FacetSillingCost * LoAStone::GetRemainingFacets(*Stone);
	AutoCostText->SetText(FText::FromString(StoneUI_Number(AutoCost) + TEXT(" 실링")));
	AutoButton->SetIsEnabled(!LoAStone::IsStarted(*Stone) && AutoCost > 0 && Inv->GetSilling() >= AutoCost);
}

// ═══ 세공 ═══════════════════════════════════════════════════════════════════

void UStoneFacetWidget::DoFacet(int32 LineIndex)
{
	UInventoryComponent* Inv = Inventory.Get();
	FEquipmentInstance* Stone = Inv ? Inv->FindEquipmentMutable(SelectedId) : nullptr;
	if (!Stone) return;
	const LoAStone::EFacetResult Result = LoAStone::Facet(*Stone, LineIndex, *Inv);
	if (Result == LoAStone::EFacetResult::Success || Result == LoAStone::EFacetResult::Fail)
	{
		const bool bSuccess = Result == LoAStone::EFacetResult::Success;
		SetResult(bSuccess ? NSLOCTEXT("Stone", "Success", "세공 성공") : NSLOCTEXT("Stone", "Fail", "세공 실패"),
			bSuccess ? (LoAStone::IsNegativeLine(LineIndex) ? StoneUI_MinusColor : StoneUI_PlusColor) : StoneUI_DimColor);
		LoAGearList::CommitChange(Inv, Equipment.Get(), SelectedId);
	}
	else
	{
		SetResult(LoAStone::GetFacetResultText(Result), StoneUI_ShortColor);
	}
	Refresh();
}

void UStoneFacetWidget::HandleAuto()
{
	UInventoryComponent* Inv = Inventory.Get();
	FEquipmentInstance* Stone = Inv ? Inv->FindEquipmentMutable(SelectedId) : nullptr;
	if (!Stone) return;
	const LoAStone::EFacetResult Result = LoAStone::AutoFacet(*Stone, *Inv);
	if (Result == LoAStone::EFacetResult::Success || Result == LoAStone::EFacetResult::Fail)
	{
		SetResult(FText::FromString(FString::Printf(TEXT("자동 세공 완료 — %s / %s / %s (총 Lv.%d)"),
			*LoAStone::DescribeLine(Stone->StoneLines[0], 0), *LoAStone::DescribeLine(Stone->StoneLines[1], 1),
			*LoAStone::DescribeLine(Stone->StoneLines[2], 2), LoAStone::GetTotalLevel(*Stone))), StoneUI_GoldColor);
		LoAGearList::CommitChange(Inv, Equipment.Get(), SelectedId);
	}
	else
	{
		SetResult(LoAStone::GetFacetResultText(Result), StoneUI_ShortColor);
	}
	Refresh();
}

// ── 창 끌기 / 클릭 차단 ──

FReply UStoneFacetWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UStoneFacetWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UStoneFacetWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UStoneFacetWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UStoneFacetWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
