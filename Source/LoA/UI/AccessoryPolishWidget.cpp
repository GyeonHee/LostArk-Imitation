#include "UI/AccessoryPolishWidget.h"
#include "UI/GearListRowWidget.h"
#include "Item/AccessoryPolish.h"
#include "Item/EquipmentComponent.h"
#include "Item/InventoryComponent.h"
#include "Stat/ArkPassiveTypes.h"
#include "LoAPlayerController.h"
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
#include "Item/BraceletStone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// 재련 창과 같은 톤 — 짙은 남색 바탕 + 금색 강조 + 주황 장신구 이름
	const FLinearColor PolishUI_PanelColor(0.022f, 0.027f, 0.04f, 0.97f);
	const FLinearColor PolishUI_ColumnColor(0.035f, 0.042f, 0.06f, 1.f);
	const FLinearColor PolishUI_RowColor(0.05f, 0.058f, 0.08f, 1.f);
	const FLinearColor PolishUI_FrameNormal(0.14f, 0.15f, 0.19f, 1.f);
	const FLinearColor PolishUI_FrameHover(0.30f, 0.75f, 0.85f, 1.f);
	const FLinearColor PolishUI_FrameSelected(1.f, 0.78f, 0.3f, 1.f);
	const FLinearColor PolishUI_FrameNext(0.3f, 0.6f, 1.f, 1.f);       // 다음에 열릴 단계 줄 — 로아의 파란 테두리
	const FLinearColor PolishUI_TextColor(0.88f, 0.88f, 0.9f, 1.f);
	const FLinearColor PolishUI_DimTextColor(0.5f, 0.51f, 0.55f, 1.f);
	const FLinearColor PolishUI_GoldColor(1.f, 0.82f, 0.4f, 1.f);
	const FLinearColor PolishUI_NameColor(1.f, 0.58f, 0.22f, 1.f);
	const FLinearColor PolishUI_NoteColor(1.f, 0.85f, 0.35f, 1.f);
	const FLinearColor PolishUI_EnoughColor(0.6f, 0.92f, 0.42f, 1.f);
	const FLinearColor PolishUI_ShortColor(1.f, 0.35f, 0.3f, 1.f);


	UTextBlock* PolishUI_MakeText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		return Block;
	}

	void PolishUI_MakeCircle(UImage* Image, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Color);
	}

	void PolishUI_SetIcon(UImage* Image, UTexture2D* Texture, const FLinearColor& Fallback)
	{
		if (!Image) return;
		if (Texture)
		{
			Image->SetBrushFromTexture(Texture);
			Image->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Image->SetBrushResourceObject(nullptr);
			Image->SetColorAndOpacity(Fallback);
		}
	}

	void PolishUI_CenterSlot(UVerticalBoxSlot* S, float Top = 0.f, float Bottom = 0.f)
	{
		if (!S) return;
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, Top, 0.f, Bottom));
	}

	FText PolishUI_Number(int64 Value)
	{
		return FText::FromString(LoAItems::FormatWithCommas(Value));
	}

	// ◆◆◇ — 연마 단계만큼 채움
	FString PolishUI_Diamonds(int32 Level)
	{
		FString Out;
		for (int32 i = 0; i < LoAPolish::MaxLevel; ++i) Out += i < Level ? TEXT("◆") : TEXT("◇");
		return Out;
	}
}

// ═══ 연마 창 — 트리 ══════════════════════════════════════════════════════════

TSharedRef<SWidget> UAccessoryPolishWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PolishRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PolishPanel"));
		Panel->SetBrushColor(FLinearColor(0.32f, 0.27f, 0.17f, 1.f));  // 얇은 금빛 테두리
		Panel->SetPadding(FMargin(1.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetPosition(FVector2D(0.f, -50.f));
		}
		USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishSize"));
		PanelSize->SetWidthOverride(1120.f);
		PanelSize->SetHeightOverride(640.f);
		Panel->SetContent(PanelSize);
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PolishBack"));
		Back->SetBrushColor(PolishUI_PanelColor);
		Back->SetPadding(FMargin(10.f));
		PanelSize->SetContent(Back);

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PolishColumns"));
		Back->SetContent(Columns);
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildLeftColumn())) S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildCenterColumn())) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildRightColumn())) S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

UWidget* UAccessoryPolishWidget::BuildLeftColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishLeftBox"));
	Box->SetWidthOverride(290.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PolishLeftBack"));
	Back->SetBrushColor(PolishUI_ColumnColor);
	Back->SetPadding(FMargin(6.f));
	Box->SetContent(Back);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PolishLeft"));
	Back->SetContent(Column);

	// 필터 줄 (표시만) + 목록 — 소지품의 연마가 안 끝난 장신구
	UBorder* Filter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PolishFilter"));
	Filter->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.1f, 1.f));
	Filter->SetPadding(FMargin(8.f, 4.f));
	Filter->SetContent(PolishUI_MakeText(WidgetTree, TEXT("PolishFilterText"), 10, PolishUI_TextColor, NSLOCTEXT("Polish", "Filter", "☰  전체")));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Filter)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PolishListScroll"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PolishList"));
	Scroll->AddChild(ListBox);
	return Box;
}

UWidget* UAccessoryPolishWidget::MakeCostRow(const FString& Name, const FText& Label, UTextBlock*& OutValue)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(Name + TEXT("Row"))));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name + TEXT("LabelBox"))));
	LabelBox->SetWidthOverride(90.f);
	LabelBox->SetContent(PolishUI_MakeText(WidgetTree, FName(*(Name + TEXT("Label"))), 10, PolishUI_DimTextColor, Label));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) S->SetVerticalAlignment(VAlign_Center);

	OutValue = PolishUI_MakeText(WidgetTree, FName(*(Name + TEXT("Value"))), 11, PolishUI_TextColor);
	OutValue->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutValue))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetHorizontalAlignment(HAlign_Right);
	}
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name + TEXT("Coin"))));
	PolishUI_SetIcon(Coin, GoldIcon.LoadSynchronous(), FLinearColor(0.95f, 0.72f, 0.2f, 1.f));
	Coin->SetDesiredSizeOverride(FVector2D(16.f, 16.f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Coin))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
	}
	return Row;
}

UWidget* UAccessoryPolishWidget::BuildCenterColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PolishCenter"));

	// 제목 (끌기 손잡이)
	HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishHeader"));
	HeaderBox->SetHeightOverride(30.f);
	UOverlay* HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PolishHeaderOverlay"));
	HeaderBox->SetContent(HeaderOverlay);
	if (UOverlaySlot* S = HeaderOverlay->AddChildToOverlay(PolishUI_MakeText(WidgetTree, TEXT("PolishTitle"), 15, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Polish", "Title", "장신구 연마"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Column->AddChildToVerticalBox(HeaderBox);

	// 장신구 아이콘 (금테)
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishIconBox"));
	IconBox->SetWidthOverride(56.f);
	IconBox->SetHeightOverride(56.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PolishIconFrame"));
	IconFrame->SetBrushColor(PolishUI_FrameSelected);
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	DetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PolishIcon"));
	IconFrame->SetContent(DetailIcon);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(IconBox), 6.f, 6.f);

	DetailName = PolishUI_MakeText(WidgetTree, TEXT("PolishName"), 14, PolishUI_NameColor);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(DetailName));
	StepText = PolishUI_MakeText(WidgetTree, TEXT("PolishStep"), 13, PolishUI_GoldColor);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(StepText), 2.f);
	BaseText = PolishUI_MakeText(WidgetTree, TEXT("PolishBase"), 9, PolishUI_DimTextColor);
	BaseText->SetJustification(ETextJustify::Center);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(BaseText), 2.f, 14.f);

	// 단계 줄 3개 — 열린 단계는 효과(등급 색), 다음 단계는 파란 테두리
	StageFrames.Reset();
	StageTexts.Reset();
	for (int32 i = 0; i < LoAPolish::MaxLevel; ++i)
	{
		USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("PolishStageBox%d"), i)));
		RowBox->SetWidthOverride(440.f);
		RowBox->SetHeightOverride(36.f);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("PolishStageFrame%d"), i)));
		Frame->SetBrushColor(PolishUI_FrameNormal);
		Frame->SetPadding(FMargin(1.f));
		RowBox->SetContent(Frame);
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("PolishStageInner%d"), i)));
		Inner->SetBrushColor(PolishUI_RowColor);
		Inner->SetPadding(FMargin(14.f, 0.f));
		Inner->SetVerticalAlignment(VAlign_Center);
		Frame->SetContent(Inner);
		UTextBlock* Text = PolishUI_MakeText(WidgetTree, FName(*FString::Printf(TEXT("PolishStageText%d"), i)), 11, PolishUI_DimTextColor);
		Inner->SetContent(Text);
		PolishUI_CenterSlot(Column->AddChildToVerticalBox(RowBox), 0.f, 6.f);
		StageFrames.Add(Frame);
		StageTexts.Add(Text);
	}

	PolishUI_CenterSlot(Column->AddChildToVerticalBox(PolishUI_MakeText(WidgetTree, TEXT("PolishNote"), 10, PolishUI_NoteColor,
		NSLOCTEXT("Polish", "Note", "단계 상승 시, 기본 효과가 상승하며 연마 추가 효과가 부여됩니다."))), 10.f, 10.f);

	// 재료 원 — 영롱한 보석 파편 (보유 / 필요)
	USizeBox* CircleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishShardBox"));
	CircleBox->SetWidthOverride(58.f);
	CircleBox->SetHeightOverride(58.f);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PolishShardLayers"));
	CircleBox->SetContent(Layers);
	ShardRing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PolishShardRing"));
	PolishUI_MakeCircle(ShardRing, FLinearColor(0.75f, 0.6f, 0.32f, 1.f));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(ShardRing))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	UImage* ShardInner = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PolishShardInner"));
	PolishUI_MakeCircle(ShardInner, FLinearColor(0.05f, 0.06f, 0.09f, 1.f));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(ShardInner))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(2.f));
	}
	ShardIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PolishShardIcon"));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(ShardIcon))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(8.f));
	}
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(CircleBox));
	ShardCount = PolishUI_MakeText(WidgetTree, TEXT("PolishShardCount"), 10, PolishUI_EnoughColor);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(ShardCount), 3.f, 10.f);

	// 연마 비용 / 소지 금액 (골드)
	USizeBox* CostBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishCostBox"));
	CostBox->SetWidthOverride(340.f);
	UVerticalBox* Costs = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PolishCosts"));
	CostBox->SetContent(Costs);
	UTextBlock* CostValue = nullptr;
	UTextBlock* HaveValue = nullptr;
	if (UVerticalBoxSlot* S = Costs->AddChildToVerticalBox(MakeCostRow(TEXT("PolishCost"), NSLOCTEXT("Polish", "Cost", "연마 비용"), CostValue))) S->SetPadding(FMargin(0.f, 1.f));
	if (UVerticalBoxSlot* S = Costs->AddChildToVerticalBox(MakeCostRow(TEXT("PolishHave"), NSLOCTEXT("Polish", "Have", "소지 금액"), HaveValue))) S->SetPadding(FMargin(0.f, 1.f));
	CostGoldText = CostValue;
	HaveGoldText = HaveValue;
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(CostBox), 0.f, 10.f);

	// [장신구 연마]
	USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishButtonBox"));
	ButtonBox->SetWidthOverride(200.f);
	ButtonBox->SetHeightOverride(40.f);
	PolishButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PolishButton"));
	{
		const FLinearColor Base(0.2f, 0.21f, 0.25f, 1.f);
		FButtonStyle Style = PolishButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(Base);
		Style.Hovered.TintColor = FSlateColor(Base * 1.5f);
		Style.Pressed.TintColor = FSlateColor(Base * 0.7f);
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.09f, 0.095f, 0.11f, 1.f));
		PolishButton->SetStyle(Style);
	}
	UTextBlock* ButtonText = PolishUI_MakeText(WidgetTree, TEXT("PolishButtonText"), 12, PolishUI_TextColor, NSLOCTEXT("Polish", "Button", "장신구 연마"));
	ButtonText->SetJustification(ETextJustify::Center);
	PolishButton->SetContent(ButtonText);
	PolishButton->OnClicked.AddDynamic(this, &UAccessoryPolishWidget::HandlePolishClicked);
	ButtonBox->SetContent(PolishButton);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(ButtonBox));

	ResultText = PolishUI_MakeText(WidgetTree, TEXT("PolishResult"), 11, PolishUI_GoldColor);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(ResultText), 8.f);
	return Column;
}

UWidget* UAccessoryPolishWidget::BuildRightColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PolishRightBox"));
	Box->SetWidthOverride(320.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PolishRightBack"));
	Back->SetBrushColor(PolishUI_ColumnColor);
	Back->SetPadding(FMargin(8.f));
	Box->SetContent(Back);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PolishRight"));
	Back->SetContent(Column);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(PolishUI_MakeText(WidgetTree, TEXT("PolishRightTitle"), 13, FLinearColor(0.95f, 0.9f, 0.75f, 1.f),
		NSLOCTEXT("Polish", "RightTitle", "부여 가능한 효과"))), 2.f, 8.f);
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(PolishUI_MakeText(WidgetTree, TEXT("PolishRightSub"), 10, PolishUI_TextColor,
		NSLOCTEXT("Polish", "RightSub", "연마 시 아래 효과 중 하나 무작위 부여 (중복 불가)"))));
	PolishUI_CenterSlot(Column->AddChildToVerticalBox(PolishUI_MakeText(WidgetTree, TEXT("PolishRightChance"), 9, PolishUI_DimTextColor,
		NSLOCTEXT("Polish", "RightChance", "등급 확률  하 63% · 중 30% · 상 7%"))), 2.f, 8.f);

	EffectList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PolishEffectList"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(EffectList)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UTextBlock* Foot = PolishUI_MakeText(WidgetTree, TEXT("PolishRightFoot"), 8, PolishUI_DimTextColor,
		FText::GetEmpty());
	Foot->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Foot)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	return Box;
}

// ═══ 열기·닫기·바인딩 ═════════════════════════════════════════════════════════

void UAccessoryPolishWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UAccessoryPolishWidget::Bind(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory)
{
	Unbind();
	Equipment = InEquipment;
	Inventory = InInventory;
	if (InEquipment)
	{
		EquipmentChangedHandle = InEquipment->OnEquipmentChanged.AddUObject(this, &UAccessoryPolishWidget::HandleEquipmentChanged);
	}
	if (InInventory)
	{
		InventoryChangedHandle = InInventory->OnSlotChanged.AddUObject(this, &UAccessoryPolishWidget::HandleInventoryChanged);
		GoldChangedHandle = InInventory->OnGoldChanged.AddUObject(this, &UAccessoryPolishWidget::HandleGoldChanged);
	}
}

void UAccessoryPolishWidget::Unbind()
{
	if (UEquipmentComponent* Equip = Equipment.Get()) Equip->OnEquipmentChanged.Remove(EquipmentChangedHandle);
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->OnSlotChanged.Remove(InventoryChangedHandle);
		Inv->OnGoldChanged.Remove(GoldChangedHandle);
	}
	EquipmentChangedHandle.Reset();
	InventoryChangedHandle.Reset();
	GoldChangedHandle.Reset();
	Equipment.Reset();
	Inventory.Reset();
}

void UAccessoryPolishWidget::OpenPolish(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory)
{
	if (!Panel) TakeWidget();
	Bind(InEquipment, InInventory);
	SetResult(FText::GetEmpty(), PolishUI_GoldColor);
	EffectListLevel = INDEX_NONE;

	// 처음엔 목록 맨 앞
	const TArray<FGuid> Ids = LoAPolish::CollectPolishable(InInventory);
	SelectedId = Ids.Num() > 0 ? Ids[0] : FGuid();
	Refresh();
}

void UAccessoryPolishWidget::ClosePolish()
{
	WindowDrag.End();
	Unbind();
}

bool UAccessoryPolishWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UAccessoryPolishWidget::SelectAccessory(const FGuid& Id)
{
	if (Id == SelectedId) return;
	SelectedId = Id;
	SetResult(FText::GetEmpty(), PolishUI_GoldColor);
	Refresh();
}

void UAccessoryPolishWidget::SetResult(const FText& Text, const FLinearColor& Color)
{
	if (!ResultText) return;
	ResultText->SetText(Text);
	ResultText->SetColorAndOpacity(FSlateColor(Color));
}

// ═══ 갱신 ═══════════════════════════════════════════════════════════════════

void UAccessoryPolishWidget::Refresh()
{
	if (!Panel) return;
	RefreshList();
	RefreshDetail();
}

void UAccessoryPolishWidget::RefreshList()
{
	// 소지품의 연마가 안 끝난 장신구만 — 방금 3단계를 채운 건 목록에서 빠지고 가운데엔 결과가 남는다
	const UInventoryComponent* Inv = Inventory.Get();
	const TArray<FGuid> Ids = LoAPolish::CollectPolishable(Inv);
	while (ListRows.Num() < Ids.Num())
	{
		UGearListRowWidget* Row = WidgetTree->ConstructWidget<UGearListRowWidget>(UGearListRowWidget::StaticClass());
		Row->OnClicked.BindUObject(this, &UAccessoryPolishWidget::SelectAccessory);
		if (UVerticalBoxSlot* S = ListBox->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		ListRows.Add(Row);
	}
	for (int32 i = 0; i < ListRows.Num(); ++i)
	{
		UGearListRowWidget* Row = ListRows[i];
		const FEquipmentInstance* Instance = Ids.IsValidIndex(i) && Inv ? Inv->FindEquipment(Ids[i]) : nullptr;
		const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
		if (!Def)
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Row->SetVisibility(ESlateVisibility::Visible);
		Row->SetRow(Ids[i], Def->Icon.LoadSynchronous(), FText::FromString(PolishUI_Diamonds(Instance->PolishLevel)), Def->DisplayName,
			FText::FromString(FString::Printf(TEXT("%d단계"), Instance->PolishLevel)), Ids[i] == SelectedId);
	}
}

void UAccessoryPolishWidget::RefreshDetail()
{
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Inv ? Inv->FindEquipment(SelectedId) : nullptr;
	const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
	RefreshEffectList(Def ? Instance : nullptr, Def ? Def->EquipSlot : EEquipSlot::Necklace);

	if (!Def || !Inv)
	{
		PolishUI_SetIcon(DetailIcon, nullptr, FLinearColor(0.f, 0.f, 0.f, 0.f));
		DetailName->SetText(NSLOCTEXT("Polish", "NoAccessory", "연마할 장신구가 없습니다."));
		StepText->SetText(FText::GetEmpty());
		BaseText->SetText(FText::GetEmpty());
		for (int32 i = 0; i < StageTexts.Num(); ++i)
		{
			StageTexts[i]->SetText(FText::GetEmpty());
			StageFrames[i]->SetBrushColor(PolishUI_FrameNormal);
		}
		ShardCount->SetText(FText::GetEmpty());
		CostGoldText->SetText(FText::GetEmpty());
		HaveGoldText->SetText(FText::GetEmpty());
		PolishButton->SetIsEnabled(false);
		return;
	}

	PolishUI_SetIcon(DetailIcon, Def->Icon.LoadSynchronous(), FLinearColor(0.3f, 0.3f, 0.35f, 1.f));
	DetailName->SetText(Def->DisplayName);

	const int32 Level = Instance->PolishLevel;
	const bool bMax = Level >= LoAPolish::MaxLevel;
	if (bMax)
	{
		StepText->SetText(FText::Format(NSLOCTEXT("Polish", "StepMax", "{0}단계 (최대)"), FText::AsNumber(Level)));
	}
	else
	{
		const FText From = Level == 0 ? NSLOCTEXT("Polish", "Before", "연마 전") : FText::Format(NSLOCTEXT("Polish", "StepN", "{0}단계"), FText::AsNumber(Level));
		StepText->SetText(FText::Format(NSLOCTEXT("Polish", "Step", "{0}  ›››  {1}단계"), From, FText::AsNumber(Level + 1)));
	}

	// 기본 효과 — 단계가 오르면 주 스탯·체력·깨달음 포인트가 오른다 (다음 단계 값을 같이)
	{
		const FGearStats Now = LoAItems::GetAccessoryStats(Def->EquipSlot, Level);
		FEquipmentInstance Next = *Instance;
		Next.PolishLevel = FMath::Min(Level + 1, LoAPolish::MaxLevel);
		const FGearStats After = LoAItems::GetAccessoryStats(Def->EquipSlot, Next.PolishLevel);
		const int32 PointsNow = LoAArkPassive::GetAccessoryEnlightenment(*Instance);
		const int32 PointsAfter = LoAArkPassive::GetAccessoryEnlightenment(Next);
		FString Line = bMax
			? FString::Printf(TEXT("지능 +%s · 체력 +%s · 깨달음 포인트 +%d"),
				*LoAItems::FormatWithCommas(static_cast<int64>(Now.PrimaryStat)), *LoAItems::FormatWithCommas(static_cast<int64>(Now.Vitality)), PointsNow)
			: FString::Printf(TEXT("지능 +%s → +%s · 체력 +%s → +%s · 깨달음 포인트 +%d → +%d"),
				*LoAItems::FormatWithCommas(static_cast<int64>(Now.PrimaryStat)), *LoAItems::FormatWithCommas(static_cast<int64>(After.PrimaryStat)),
				*LoAItems::FormatWithCommas(static_cast<int64>(Now.Vitality)), *LoAItems::FormatWithCommas(static_cast<int64>(After.Vitality)),
				PointsNow, PointsAfter);
		BaseText->SetText(FText::FromString(Line));
	}

	// 단계 줄
	for (int32 i = 0; i < StageTexts.Num(); ++i)
	{
		if (Instance->PolishEffects.IsValidIndex(i))
		{
			const FPolishEffectRoll& Roll = Instance->PolishEffects[i];
			const bool bEffective = LoAPolish::GetDef(Roll.Effect).bHasEffect;
			StageTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("◆  %d단계   %s   [%s]%s"), i + 1, *LoAPolish::Describe(Roll),
				LoAPolish::GetGradeName(Roll.Grade), TEXT(""))));
			StageTexts[i]->SetColorAndOpacity(FSlateColor(LoAPolish::GetGradeColor(Roll.Grade)));
			StageFrames[i]->SetBrushColor(PolishUI_FrameNormal);
		}
		else
		{
			StageTexts[i]->SetText(FText::Format(NSLOCTEXT("Polish", "StageLocked", "◇  {0}단계 장신구 연마 시 효과 개방"), FText::AsNumber(i + 1)));
			const bool bNext = i == Level;
			StageTexts[i]->SetColorAndOpacity(FSlateColor(bNext ? PolishUI_TextColor : PolishUI_DimTextColor));
			StageFrames[i]->SetBrushColor(bNext ? PolishUI_FrameNext : PolishUI_FrameNormal);
		}
	}

	// 재료·비용
	const FItemDefinition* ShardDef = LoAItems::Find(LoAItems::PolishShard);
	PolishUI_SetIcon(ShardIcon, ShardDef ? ShardDef->Icon.LoadSynchronous() : nullptr, FLinearColor(0.6f, 0.35f, 0.9f, 1.f));
	const int64 HaveShards = Inv->CountItem(LoAItems::PolishShard);
	if (bMax)
	{
		ShardCount->SetText(FText::FromString(LoAItems::FormatWithCommas(HaveShards)));
		ShardCount->SetColorAndOpacity(FSlateColor(PolishUI_DimTextColor));
		CostGoldText->SetText(FText::FromString(TEXT("-")));
		HaveGoldText->SetText(PolishUI_Number(Inv->GetGold()));
		HaveGoldText->SetColorAndOpacity(FSlateColor(PolishUI_TextColor));
		ShardRing->SetColorAndOpacity(FLinearColor(0.22f, 0.22f, 0.25f, 1.f));
		PolishButton->SetIsEnabled(false);
		return;
	}
	const LoAPolish::FCost Cost = LoAPolish::GetCost(Level + 1);
	ShardRing->SetColorAndOpacity(FLinearColor(0.75f, 0.6f, 0.32f, 1.f));
	ShardCount->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"), *LoAItems::FormatWithCommas(HaveShards), *LoAItems::FormatWithCommas(Cost.Shards))));
	ShardCount->SetColorAndOpacity(FSlateColor(HaveShards >= Cost.Shards ? PolishUI_EnoughColor : PolishUI_ShortColor));
	ShardIcon->SetToolTipText(ShardDef ? ShardDef->DisplayName : FText::GetEmpty());
	CostGoldText->SetText(PolishUI_Number(Cost.Gold));
	HaveGoldText->SetText(PolishUI_Number(Inv->GetGold()));
	HaveGoldText->SetColorAndOpacity(FSlateColor(Inv->GetGold() >= Cost.Gold ? PolishUI_TextColor : PolishUI_ShortColor));
	PolishButton->SetIsEnabled(true);  // 모자라면 누를 때 안내
}

void UAccessoryPolishWidget::RefreshEffectList(const FEquipmentInstance* Instance, EEquipSlot Part)
{
	if (!EffectList) return;
	const FGuid Id = Instance ? Instance->InstanceId : FGuid();
	const int32 Level = Instance ? Instance->PolishLevel : INDEX_NONE;
	if (Id == EffectListInstance && Level == EffectListLevel && EffectList->GetChildrenCount() > 0) return;
	EffectListInstance = Id;
	EffectListLevel = Level;
	EffectList->ClearChildren();
	if (!Instance) return;

	int32 Index = 0;
	for (const EPolishEffect Effect : LoAPolish::GetEffectsFor(Part))
	{
		const LoAPolish::FEffectDef& Def = LoAPolish::GetDef(Effect);
		const bool bOwned = Instance->PolishEffects.ContainsByPredicate([Effect](const FPolishEffectRoll& R) { return R.Effect == Effect; });
		for (int32 Grade = 0; Grade < 3; ++Grade, ++Index)
		{
			UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
			Line->SetBrushColor(Index % 2 == 0 ? FLinearColor(0.045f, 0.052f, 0.072f, 1.f) : FLinearColor(0.035f, 0.042f, 0.06f, 1.f));
			Line->SetPadding(FMargin(8.f, 5.f));
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), NAME_None);
			Line->SetContent(Row);

			const FLinearColor NameColor = bOwned ? PolishUI_DimTextColor * 0.7f : PolishUI_TextColor;
			Row->AddChildToHorizontalBox(PolishUI_MakeText(WidgetTree, NAME_None, 10, NameColor,
				FText::FromString(FString::Printf(TEXT("◈ %s "), Def.Name))));
			const FString Value = LoAPolish::DescribeValue(Effect, Grade).RightChop(FCString::Strlen(Def.Name) + 1);  // "+1.60%"
			FLinearColor ValueColor = LoAPolish::GetGradeColor(Grade);
			if (bOwned) ValueColor = ValueColor * 0.45f;
			Row->AddChildToHorizontalBox(PolishUI_MakeText(WidgetTree, NAME_None, 10, ValueColor,
				FText::FromString(Value)));
			if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(EffectList->AddChild(Line))) S->SetPadding(FMargin(0.f, 0.f, 0.f, 1.f));
		}
	}
}

// ═══ 연마 ═══════════════════════════════════════════════════════════════════

void UAccessoryPolishWidget::HandlePolishClicked()
{
	UInventoryComponent* Inv = Inventory.Get();
	if (!Inv) return;
	FPolishEffectRoll Roll;
	const LoAPolish::EResult Result = LoAPolish::TryPolish(*Inv, Equipment.Get(), SelectedId, Roll);
	if (Result != LoAPolish::EResult::Success)
	{
		SetResult(LoAPolish::GetResultText(Result), PolishUI_ShortColor);
		return;
	}
	const FEquipmentInstance* Instance = Inv->FindEquipment(SelectedId);
	SetResult(FText::FromString(FString::Printf(TEXT("연마 %d단계 성공 — %s [%s]"), Instance ? Instance->PolishLevel : 0,
		*LoAPolish::Describe(Roll), LoAPolish::GetGradeName(Roll.Grade))), LoAPolish::GetGradeColor(Roll.Grade));
	Refresh();
}

// ── 창 끌기 / 클릭 차단 ──

FReply UAccessoryPolishWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UAccessoryPolishWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAccessoryPolishWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UAccessoryPolishWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UAccessoryPolishWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
