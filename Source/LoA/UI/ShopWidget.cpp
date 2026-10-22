#include "UI/ShopWidget.h"
#include "Item/InventoryComponent.h"
#include "Item/EquipmentComponent.h"
#include "Item/ItemTypes.h"
#include "LoAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FLinearColor ShopPanelColor(0.035f, 0.04f, 0.055f, 0.95f);
	const FLinearColor ShopCellColor(0.07f, 0.075f, 0.09f, 0.95f);
	const FLinearColor ShopFrameNormal(0.17f, 0.18f, 0.21f, 1.f);
	const FLinearColor ShopFrameHover(0.30f, 0.75f, 0.85f, 1.f);
	const FLinearColor ShopTextColor(0.88f, 0.88f, 0.9f, 1.f);
	const FLinearColor ShopDimTextColor(0.55f, 0.56f, 0.6f, 1.f);
	const FLinearColor ShopBalanceColor(0.45f, 0.70f, 1.f, 1.f);
	const FLinearColor ShopShortColor(1.f, 0.35f, 0.3f, 1.f);

	UTextBlock* MakeShopText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		return Block;
	}

	// 어두운 회색 버튼 + 가운데 글자, 고정 크기 SizeBox로 감싸 반환
	USizeBox* MakeShopButton(UWidgetTree* Tree, const FString& Name, const FText& Label, float Width, float Height, int32 FontSize, UButton*& OutButton)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name + TEXT("Box"))));
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(Height);

		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*Name));
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(FLinearColor(0.16f, 0.17f, 0.2f, 1.f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(0.26f, 0.28f, 0.32f, 1.f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(0.10f, 0.11f, 0.13f, 1.f));
		Style.NormalPadding = FMargin(2.f);
		Style.PressedPadding = FMargin(2.f);
		Button->SetStyle(Style);
		Box->SetContent(Button);

		UTextBlock* Text = MakeShopText(Tree, FName(*(Name + TEXT("Text"))), FontSize, ShopTextColor, Label);
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);

		OutButton = Button;
		return Box;
	}

	UImage* MakeShopCoin(UWidgetTree* Tree, const FName& Name, UTexture2D* Texture, float Size)
	{
		UImage* Coin = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (Texture)
		{
			Coin->SetBrushFromTexture(Texture);
		}
		else
		{
			Coin->SetColorAndOpacity(FLinearColor(0.75f, 0.77f, 0.82f, 1.f));
		}
		Coin->SetDesiredSizeOverride(FVector2D(Size, Size));
		return Coin;
	}

	// 바깥 Border(테두리 색) → 1px 안쪽 Border(바탕). 바깥을 반환, 안쪽은 OutInner
	UBorder* MakeShopFramedCell(UWidgetTree* Tree, const FString& Name, UBorder*& OutInner)
	{
		UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*(Name + TEXT("Frame"))));
		Frame->SetBrushColor(ShopFrameNormal);
		Frame->SetPadding(FMargin(1.f));
		UBorder* Inner = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*(Name + TEXT("Inner"))));
		Inner->SetBrushColor(ShopCellColor);
		Inner->SetPadding(FMargin(0.f));
		Frame->SetContent(Inner);
		OutInner = Inner;
		return Frame;
	}
}

// ═══ 판매 목록 한 칸 ═════════════════════════════════════════════════════════

void UShopEntryWidget::Setup(UShopWidget* InOwner, int32 InIndex)
{
	Owner = InOwner;
	EntryIndex = InIndex;
}

TSharedRef<SWidget> UShopEntryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EntryBox"));
		Box->SetHeightOverride(54.f);
		WidgetTree->RootWidget = Box;

		UBorder* Inner = nullptr;
		Frame = MakeShopFramedCell(WidgetTree, TEXT("Entry"), Inner);
		Inner->SetPadding(FMargin(5.f, 4.f));
		Box->SetContent(Frame);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EntryRow"));
		Inner->SetContent(Row);

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EntryIconBox"));
		IconBox->SetWidthOverride(42.f);
		IconBox->SetHeightOverride(42.f);
		UOverlay* IconOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("EntryIconOverlay"));
		IconBox->SetContent(IconOverlay);
		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EntryIcon"));
		if (UOverlaySlot* S = IconOverlay->AddChildToOverlay(IconImage))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		IconCountText = MakeShopText(WidgetTree, TEXT("EntryIconCount"), 9, FLinearColor::White);
		{
			FSlateFontInfo CountFont = IconCountText->GetFont();
			CountFont.OutlineSettings.OutlineSize = 1;
			CountFont.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.95f);
			IconCountText->SetFont(CountFont);
		}
		IconCountText->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = IconOverlay->AddChildToOverlay(IconCountText))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(2.f, 0.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}

		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EntryInfo"));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Info))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		NameText = MakeShopText(WidgetTree, TEXT("EntryName"), 11, ShopTextColor);
		Info->AddChildToVerticalBox(NameText);

		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("EntryGap"));
		if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(Gap))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UHorizontalBox* PriceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EntryPriceRow"));
		if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(PriceRow))
		{
			S->SetHorizontalAlignment(HAlign_Right);
		}
		PriceText = MakeShopText(WidgetTree, TEXT("EntryPrice"), 11, ShopTextColor);
		if (UHorizontalBoxSlot* S = PriceRow->AddChildToHorizontalBox(PriceText))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
		}
		const UShopWidget* Shop = Owner.Get();
		CoinImage = MakeShopCoin(WidgetTree, TEXT("EntryCoin"), Shop ? Shop->SillingIcon.LoadSynchronous() : nullptr, 13.f);
		if (UHorizontalBoxSlot* S = PriceRow->AddChildToHorizontalBox(CoinImage))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		IconImage->SetVisibility(ESlateVisibility::Hidden);
		PriceRow->SetVisibility(ESlateVisibility::HitTestInvisible);
		ApplyFrame();
	}
	SetVisibility(ESlateVisibility::Visible);
	return Super::RebuildWidget();
}

void UShopEntryWidget::SetEntry(const FShopItemEntry* Entry)
{
	if (!NameText) TakeWidget();
	const FItemDefinition* Def = Entry ? LoAItems::Find(Entry->ItemId) : nullptr;
	bHasEntry = Def != nullptr;
	if (IconCountText) IconCountText->SetVisibility(ESlateVisibility::Collapsed);

	if (bHasEntry)
	{
		if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Texture);
		}
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		NameText->SetText(Def->DisplayName);
		PriceText->SetText(FText::FromString(LoAItems::FormatWithCommas(Entry->Price)));
		CoinImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetToolTipText(FText::Format(NSLOCTEXT("Shop", "EntryTooltip", "{0}\n우클릭: 1개 담기 / Shift+우클릭: 수량 선택"), Def->DisplayName));
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		NameText->SetText(FText::GetEmpty());
		PriceText->SetText(FText::GetEmpty());
		CoinImage->SetVisibility(ESlateVisibility::Hidden);
		SetToolTipText(FText::GetEmpty());
	}
	ApplyFrame();
}

void UShopEntryWidget::SetSellEntry(FName ItemId, int32 Count, int64 TotalPrice, const FEquipmentInstance* Equipment)
{
	if (!NameText) TakeWidget();
	const FItemDefinition* Def = (ItemId.IsNone() || Count <= 0) ? nullptr : LoAItems::Find(ItemId);
	bHasEntry = Def != nullptr;

	if (bHasEntry)
	{
		if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Texture);
		}
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		// 장비는 개수 대신 재련 단계(+10), 툴팁은 장비 설명 — 소지품 칸과 같은 표기
		IconCountText->SetText(Equipment ? FText::FromString(LoAItems::GetEquipmentBadge(*Equipment)) : FText::AsNumber(Count));
		IconCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		NameText->SetText(Def->DisplayName);
		PriceText->SetText(FText::FromString(LoAItems::FormatWithCommas(TotalPrice)));
		CoinImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetToolTipText(Equipment
			? FText::Format(NSLOCTEXT("Shop", "SellEquipTooltip", "{0}\n우클릭: 판매 목록에서 빼기"), UEquipmentComponent::DescribeEquipment(*Equipment))
			: FText::Format(NSLOCTEXT("Shop", "SellEntryTooltip", "{0} x{1}\n우클릭: 판매 목록에서 빼기"), Def->DisplayName, FText::AsNumber(Count)));
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		IconCountText->SetVisibility(ESlateVisibility::Collapsed);
		NameText->SetText(FText::GetEmpty());
		PriceText->SetText(FText::GetEmpty());
		CoinImage->SetVisibility(ESlateVisibility::Hidden);
		SetToolTipText(FText::GetEmpty());
	}
	ApplyFrame();
}

void UShopEntryWidget::ApplyFrame()
{
	if (Frame) Frame->SetBrushColor(bHasEntry && bHovered ? ShopFrameHover : ShopFrameNormal);
}

FReply UShopEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bHasEntry && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Owner.IsValid())
	{
		Owner->HandleEntryRightClick(EntryIndex, InMouseEvent.IsShiftDown());
	}
	// 어떤 버튼이든 먹는다 — 창 뒤 맵으로 클릭 이동이 새지 않게
	return FReply::Handled();
}

FReply UShopEntryWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빠르게 두 번 우클릭하면 두 번째는 DoubleClick으로 온다 — 그것도 1개 담기
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UShopEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyFrame();
}

void UShopEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyFrame();
}

// ═══ 구매 목록 한 칸 ═════════════════════════════════════════════════════════

void UShopCartSlotWidget::Setup(UShopWidget* InOwner, int32 InIndex, float InSize)
{
	Owner = InOwner;
	CartIndex = InIndex;
	SlotSize = InSize;
}

TSharedRef<SWidget> UShopCartSlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CartBox"));
		Box->SetWidthOverride(SlotSize);
		Box->SetHeightOverride(SlotSize);
		WidgetTree->RootWidget = Box;

		UBorder* Inner = nullptr;
		UBorder* Frame = MakeShopFramedCell(WidgetTree, TEXT("Cart"), Inner);
		Box->SetContent(Frame);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CartOverlay"));
		Inner->SetContent(Overlay);

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CartIcon"));
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(IconImage))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(2.f));
		}

		CountText = MakeShopText(WidgetTree, TEXT("CartCount"), 9, FLinearColor::White);
		FSlateFontInfo Font = CountText->GetFont();
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.95f);
		CountText->SetFont(Font);
		CountText->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(CountText))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		}
	}
	SetVisibility(ESlateVisibility::Visible);
	return Super::RebuildWidget();
}

void UShopCartSlotWidget::SetCartSlot(const FShopCartSlot& CartSlot)
{
	if (!IconImage) TakeWidget();
	const FItemDefinition* Def = CartSlot.IsEmpty() ? nullptr : LoAItems::Find(CartSlot.ItemId);
	if (Def)
	{
		if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Texture);
		}
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		CountText->SetText(FText::AsNumber(CartSlot.Count));
		CountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetToolTipText(FText::Format(NSLOCTEXT("Shop", "CartTooltip", "{0} x{1}\n우클릭: 빼기"), Def->DisplayName, FText::AsNumber(CartSlot.Count)));
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		CountText->SetVisibility(ESlateVisibility::Collapsed);
		SetToolTipText(FText::GetEmpty());
	}
}

FReply UShopCartSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Owner.IsValid())
	{
		Owner->HandleCartSlotRightClick(CartIndex);
	}
	return FReply::Handled();
}

FReply UShopCartSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// ═══ 상점 창 ═════════════════════════════════════════════════════════════════

TSharedRef<SWidget> UShopWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UTexture2D* CoinTexture = SillingIcon.LoadSynchronous();

		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPanel"));
		Panel->SetBrushColor(ShopPanelColor);
		Panel->SetPadding(FMargin(8.f, 4.f, 8.f, 8.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
		}
		ResetPanelPlacement();

		USizeBox* PanelWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopWidth"));
		PanelWidth->SetWidthOverride(400.f);
		Panel->SetContent(PanelWidth);

		// 본문 위에 묶음 구매 팝업이 겹친다
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopLayers"));
		PanelWidth->SetContent(Layers);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopColumn"));
		if (UOverlaySlot* S = Layers->AddChildToOverlay(Column))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		// ── 헤더 (끌기 손잡이) ──
		HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopHeader"));
		HeaderBox->SetHeightOverride(30.f);
		Column->AddChildToVerticalBox(HeaderBox);
		UTextBlock* Title = MakeShopText(WidgetTree, TEXT("ShopTitle"), 13, FLinearColor(0.92f, 0.87f, 0.72f, 1.f), NSLOCTEXT("Shop", "Title", "상점"));
		Title->SetJustification(ETextJustify::Center);
		UOverlay* HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopHeaderOverlay"));
		HeaderBox->SetContent(HeaderOverlay);
		if (UOverlaySlot* S = HeaderOverlay->AddChildToOverlay(Title))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopDivider"));
		Divider->SetBrushColor(FLinearColor(0.35f, 0.32f, 0.25f, 0.8f));
		Divider->SetPadding(FMargin(0.f, 0.5f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Divider)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

		// ── 탭 줄: [구매] 판매 ........ [Shift+우클릭] 구매 수량 선택 ──
		UOverlay* TabRow = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopTabRow"));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TabRow)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopTabs"));
		if (UOverlaySlot* S = TabRow->AddChildToOverlay(Tabs)) S->SetHorizontalAlignment(HAlign_Left);
		// 구매·판매 탭 = 버튼(눌러서 전환)
		{
			UButton* TabButton = nullptr;
			if (UHorizontalBoxSlot* S = Tabs->AddChildToHorizontalBox(MakeShopButton(WidgetTree, TEXT("ShopTabBuy"), NSLOCTEXT("Shop", "TabBuy", "구매"), 64.f, 26.f, 11, TabButton)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			}
			BuyTabButton = TabButton;
			BuyTabText = Cast<UTextBlock>(TabButton->GetContent());
			BuyTabButton->OnClicked.AddDynamic(this, &UShopWidget::HandleBuyTabClicked);

			if (UHorizontalBoxSlot* S = Tabs->AddChildToHorizontalBox(MakeShopButton(WidgetTree, TEXT("ShopTabSell"), NSLOCTEXT("Shop", "TabSell", "판매"), 64.f, 26.f, 11, TabButton)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			}
			SellTabButton = TabButton;
			SellTabText = Cast<UTextBlock>(TabButton->GetContent());
			SellTabButton->OnClicked.AddDynamic(this, &UShopWidget::HandleSellTabClicked);

		}
		HintText = MakeShopText(WidgetTree, TEXT("ShopHint"), 9, ShopDimTextColor, NSLOCTEXT("Shop", "Hint", "[Shift+우클릭] 구매 수량 선택"));
		if (UOverlaySlot* S = TabRow->AddChildToOverlay(HintText))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}

		// ── 분류: [물약] ──
		UBorder* Category = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopCategory"));
		Category->SetBrushColor(FLinearColor(0.16f, 0.22f, 0.32f, 1.f));
		Category->SetPadding(FMargin(12.f, 3.f));
		Category->SetContent(MakeShopText(WidgetTree, TEXT("ShopCategoryText"), 10, ShopTextColor, NSLOCTEXT("Shop", "CategoryPotion", "물약")));
		CategoryRow = Category;
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Category))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}

		// ── 판매 목록 2열 ──
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ShopGrid"));
		Grid->SetSlotPadding(FMargin(1.f));
		Column->AddChildToVerticalBox(Grid);
		EntryWidgets.Reset();
		for (int32 i = 0; i < EntryColumns * EntryRows; ++i)
		{
			UShopEntryWidget* Entry = WidgetTree->ConstructWidget<UShopEntryWidget>(UShopEntryWidget::StaticClass(), FName(*FString::Printf(TEXT("ShopEntry_%d"), i)));
			Entry->Setup(this, i);
			if (UUniformGridSlot* GS = Grid->AddChildToUniformGrid(Entry, i / EntryColumns, i % EntryColumns))
			{
				GS->SetHorizontalAlignment(HAlign_Fill);
				GS->SetVerticalAlignment(VAlign_Fill);
			}
			EntryWidgets.Add(Entry);
		}

		// ── 페이지 (아직 1쪽뿐) ──
		UTextBlock* Page = MakeShopText(WidgetTree, TEXT("ShopPage"), 10, ShopDimTextColor, FText::FromString(TEXT("≪   ‹   1/1   ›   ≫")));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Page))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 5.f));
		}

		// ── 구매 목록 10칸 ──
		UHorizontalBox* CartRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopCartRow"));
		CartRowWidget = CartRow;
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CartRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		CartWidgets.Reset();
		for (int32 i = 0; i < CartSlotCount; ++i)
		{
			UShopCartSlotWidget* CartWidget = WidgetTree->ConstructWidget<UShopCartSlotWidget>(UShopCartSlotWidget::StaticClass(), FName(*FString::Printf(TEXT("ShopCart_%d"), i)));
			CartWidget->Setup(this, i, 36.f);
			if (UHorizontalBoxSlot* S = CartRow->AddChildToHorizontalBox(CartWidget)) S->SetPadding(FMargin(1.f, 0.f));
			CartWidgets.Add(CartWidget);
		}

		// ── 아래: 구매 금액 / 구매 후 잔액 + [구매][비우기] ──
		UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopBottom"));
		Column->AddChildToVerticalBox(Bottom);

		UVerticalBox* Sums = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopSums"));
		if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(Sums))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(2.f, 0.f, 10.f, 0.f));
		}
		auto AddSumRow = [this, Sums, CoinTexture](const TCHAR* Key, const FText& Label, const FLinearColor& ValueColor, UTextBlock*& OutLabel) -> UTextBlock*
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("ShopSumRow_%s"), Key)));
			if (UVerticalBoxSlot* S = Sums->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 2.f));
			UTextBlock* LabelText = MakeShopText(WidgetTree, FName(*FString::Printf(TEXT("ShopSumLabel_%s"), Key)), 10, ShopDimTextColor, Label);
			OutLabel = LabelText;
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelText))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			UTextBlock* Value = MakeShopText(WidgetTree, FName(*FString::Printf(TEXT("ShopSumValue_%s"), Key)), 11, ValueColor, FText::FromString(TEXT("0")));
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Value))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeShopCoin(WidgetTree, FName(*FString::Printf(TEXT("ShopSumCoin_%s"), Key)), CoinTexture, 14.f)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			return Value;
		};
		UTextBlock* TotalLabelText = nullptr;
		UTextBlock* BalanceLabelText = nullptr;
		TotalText = AddSumRow(TEXT("Total"), NSLOCTEXT("Shop", "Total", "구매 금액"), ShopTextColor, TotalLabelText);
		BalanceText = AddSumRow(TEXT("Balance"), NSLOCTEXT("Shop", "Balance", "구매 후 잔액"), ShopBalanceColor, BalanceLabelText);
		TotalLabel = TotalLabelText;
		BalanceLabel = BalanceLabelText;

		UButton* BuyButton = nullptr;
		if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(MakeShopButton(WidgetTree, TEXT("ShopBuy"), NSLOCTEXT("Shop", "Buy", "구매"), 70.f, 48.f, 12, BuyButton)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		}
		BuyButton->OnClicked.AddDynamic(this, &UShopWidget::HandleBuyClicked);
		ActionButtonText = Cast<UTextBlock>(BuyButton->GetContent());
		UButton* ClearButton = nullptr;
		Bottom->AddChildToHorizontalBox(MakeShopButton(WidgetTree, TEXT("ShopClear"), NSLOCTEXT("Shop", "Clear", "비우기"), 70.f, 48.f, 12, ClearButton));
		ClearButton->OnClicked.AddDynamic(this, &UShopWidget::HandleClearClicked);

		// ── 묶음 구매 팝업 (본문 위를 어둡게 덮음) ──
		PopupLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPopupLayer"));
		PopupLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.5f));
		PopupLayer->SetHorizontalAlignment(HAlign_Center);
		PopupLayer->SetVerticalAlignment(VAlign_Center);
		PopupLayer->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Layers->AddChildToOverlay(PopupLayer))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* PopupWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopPopupWidth"));
		PopupWidth->SetWidthOverride(260.f);
		PopupLayer->SetContent(PopupWidth);
		UBorder* PopupFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPopupFrame"));
		PopupFrame->SetBrushColor(FLinearColor(0.30f, 0.30f, 0.33f, 1.f));
		PopupFrame->SetPadding(FMargin(1.f));
		PopupWidth->SetContent(PopupFrame);
		UBorder* PopupBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPopupBack"));
		PopupBack->SetBrushColor(FLinearColor(0.06f, 0.065f, 0.08f, 1.f));
		PopupBack->SetPadding(FMargin(12.f, 8.f, 12.f, 12.f));
		PopupFrame->SetContent(PopupBack);

		UVerticalBox* PopupColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopPopupColumn"));
		PopupBack->SetContent(PopupColumn);

		UTextBlock* PopupTitle = MakeShopText(WidgetTree, TEXT("ShopPopupTitle"), 12, FLinearColor(0.92f, 0.87f, 0.72f, 1.f), NSLOCTEXT("Shop", "BulkTitle", "묶음 구매"));
		PopupTitleText = PopupTitle;
		if (UVerticalBoxSlot* S = PopupColumn->AddChildToVerticalBox(PopupTitle))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		UBorder* IconInner = nullptr;
		UBorder* IconFrame = MakeShopFramedCell(WidgetTree, TEXT("ShopPopupIcon"), IconInner);
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopPopupIconBox"));
		IconBox->SetWidthOverride(56.f);
		IconBox->SetHeightOverride(56.f);
		IconBox->SetContent(IconFrame);
		PopupIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopPopupIconImage"));
		IconInner->SetPadding(FMargin(3.f));
		IconInner->SetContent(PopupIcon);
		if (UVerticalBoxSlot* S = PopupColumn->AddChildToVerticalBox(IconBox)) S->SetHorizontalAlignment(HAlign_Center);

		UTextBlock* Ask = MakeShopText(WidgetTree, TEXT("ShopPopupAsk"), 11, ShopTextColor, NSLOCTEXT("Shop", "BulkAsk", "구매하시겠습니까?"));
		PopupAskText = Ask;
		if (UVerticalBoxSlot* S = PopupColumn->AddChildToVerticalBox(Ask))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));
		}

		// 수량 입력 + ▲▼
		UHorizontalBox* QuantityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopPopupQuantityRow"));
		if (UVerticalBoxSlot* S = PopupColumn->AddChildToVerticalBox(QuantityRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		USizeBox* QuantityBoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopPopupQuantitySize"));
		QuantityBoxSize->SetWidthOverride(80.f);
		QuantityBoxSize->SetHeightOverride(36.f);
		PopupQuantityBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ShopPopupQuantity"));
		PopupQuantityBox->SetJustification(ETextJustify::Center);
		PopupQuantityBox->SetSelectAllTextWhenFocused(true);
		{
			// 기본 스타일은 흰 바탕·검은 글자 — 상점 창 바탕처럼 어둡게, 글자는 흰색으로 조금 작게
			FEditableTextBoxStyle BoxStyle = PopupQuantityBox->GetWidgetStyle();
			for (FSlateBrush* Brush : { &BoxStyle.BackgroundImageNormal, &BoxStyle.BackgroundImageHovered, &BoxStyle.BackgroundImageFocused, &BoxStyle.BackgroundImageReadOnly })
			{
				Brush->TintColor = FSlateColor(ShopPanelColor);
			}
			BoxStyle.BackgroundColor = FSlateColor(FLinearColor::White);
			BoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
			BoxStyle.FocusedForegroundColor = FSlateColor(FLinearColor::White);
			BoxStyle.ReadOnlyForegroundColor = FSlateColor(FLinearColor::White);
			BoxStyle.TextStyle.ColorAndOpacity = FSlateColor(FLinearColor::White);
			BoxStyle.TextStyle.Font.Size = 10;
			PopupQuantityBox->SetWidgetStyle(BoxStyle);
		}
		PopupQuantityBox->OnTextChanged.AddDynamic(this, &UShopWidget::HandlePopupTextChanged);
		PopupQuantityBox->OnTextCommitted.AddDynamic(this, &UShopWidget::HandlePopupTextCommitted);
		// 어두운 바탕이 팝업 바탕에 묻히지 않게 얇은 테두리
		UBorder* QuantityFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPopupQuantityFrame"));
		QuantityFrame->SetBrushColor(ShopFrameNormal);
		QuantityFrame->SetPadding(FMargin(1.f));
		QuantityFrame->SetContent(PopupQuantityBox);
		QuantityBoxSize->SetContent(QuantityFrame);
		if (UHorizontalBoxSlot* S = QuantityRow->AddChildToHorizontalBox(QuantityBoxSize)) S->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* Arrows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopPopupArrows"));
		if (UHorizontalBoxSlot* S = QuantityRow->AddChildToHorizontalBox(Arrows))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(3.f, 0.f, 0.f, 0.f));
		}
		UButton* UpButton = nullptr;
		if (UVerticalBoxSlot* S = Arrows->AddChildToVerticalBox(MakeShopButton(WidgetTree, TEXT("ShopPopupUp"), FText::FromString(TEXT("▲")), 30.f, 17.f, 9, UpButton)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		}
		UpButton->OnClicked.AddDynamic(this, &UShopWidget::HandlePopupUp);
		UButton* DownButton = nullptr;
		Arrows->AddChildToVerticalBox(MakeShopButton(WidgetTree, TEXT("ShopPopupDown"), FText::FromString(TEXT("▼")), 30.f, 17.f, 9, DownButton));
		DownButton->OnClicked.AddDynamic(this, &UShopWidget::HandlePopupDown);

		UHorizontalBox* PopupButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopPopupButtons"));
		if (UVerticalBoxSlot* S = PopupColumn->AddChildToVerticalBox(PopupButtons)) S->SetHorizontalAlignment(HAlign_Center);
		UButton* ConfirmButton = nullptr;
		if (UHorizontalBoxSlot* S = PopupButtons->AddChildToHorizontalBox(MakeShopButton(WidgetTree, TEXT("ShopPopupConfirm"), NSLOCTEXT("Shop", "Confirm", "✔ 확인"), 100.f, 32.f, 11, ConfirmButton)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		}
		ConfirmButton->OnClicked.AddDynamic(this, &UShopWidget::HandlePopupConfirm);
		UButton* CancelButton = nullptr;
		PopupButtons->AddChildToHorizontalBox(MakeShopButton(WidgetTree, TEXT("ShopPopupCancel"), NSLOCTEXT("Shop", "Cancel", "✖ 취소"), 100.f, 32.f, 11, CancelButton));
		CancelButton->OnClicked.AddDynamic(this, &UShopWidget::HandlePopupCancel);
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	SetTab(CurrentTab);
	return Result;
}

void UShopWidget::NativeDestruct()
{
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->OnSillingChanged.Remove(SillingChangedHandle);
		Inv->OnSellReserveChanged.Remove(SellReserveChangedHandle);
	}
	SillingChangedHandle.Reset();
	SellReserveChangedHandle.Reset();
	Super::NativeDestruct();
}

void UShopWidget::ResetPanelPlacement()
{
	// 화면 가운데 기준 왼쪽 — 소지품 창이 가운데 오른쪽에 붙는다
	if (UCanvasPanelSlot* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr)
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(1.f, 0.5f));
		PanelSlot->SetPosition(FVector2D(-4.f, -40.f));
	}
}

void UShopWidget::OpenShop(const TArray<FShopItemEntry>& InItems, int32 InCartSlotMax, UInventoryComponent* InInventory)
{
	if (!Panel) TakeWidget();

	Items = InItems;
	CartSlotMax = FMath::Max(1, InCartSlotMax);
	Cart.Reset();
	Cart.SetNum(CartSlotCount);

	if (UInventoryComponent* Old = Inventory.Get())
	{
		Old->OnSillingChanged.Remove(SillingChangedHandle);
		Old->OnSellReserveChanged.Remove(SellReserveChangedHandle);
	}
	SillingChangedHandle.Reset();
	SellReserveChangedHandle.Reset();
	Inventory = InInventory;
	if (InInventory)
	{
		SillingChangedHandle = InInventory->OnSillingChanged.AddUObject(this, &UShopWidget::HandleSillingChanged);
		SellReserveChangedHandle = InInventory->OnSellReserveChanged.AddUObject(this, &UShopWidget::HandleSellReserveChanged);
	}

	CloseQuantityPopup();
	ResetPanelPlacement();
	SetTab(EShopTab::Buy);
}

void UShopWidget::CloseShop()
{
	// 판매 목록에 올려 둔 아이템은 소지품으로 되돌린다
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->UnreserveAllSell();
	}
	CloseQuantityPopup();
	Cart.Reset();
	Cart.SetNum(CartSlotCount);
	RefreshCart();
	WindowDrag.End();
}

bool UShopWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

// ── 구매 목록 ──

void UShopWidget::HandleEntryRightClick(int32 EntryIndex, bool bShift)
{
	if (PopupLayer && PopupLayer->GetVisibility() != ESlateVisibility::Collapsed) return;  // 팝업 중엔 목록 조작 안 받음

	// 판매 탭 — 그 줄을 소지품으로 되돌림
	if (CurrentTab == EShopTab::Sell)
	{
		if (UInventoryComponent* Inv = Inventory.Get())
		{
			Inv->UnreserveSell(EntryIndex);
		}
		return;
	}

	if (!Items.IsValidIndex(EntryIndex)) return;

	if (bShift)
	{
		OpenQuantityPopup(EPopupMode::Buy, EntryIndex, CartSlotMax);
	}
	else
	{
		AddToCart(EntryIndex, 1);
	}
}

void UShopWidget::HandleCartSlotRightClick(int32 CartIndex)
{
	if (!Cart.IsValidIndex(CartIndex) || Cart[CartIndex].IsEmpty()) return;
	Cart[CartIndex] = FShopCartSlot();
	RefreshCart();
}

void UShopWidget::AddToCart(int32 EntryIndex, int32 Count)
{
	if (!Items.IsValidIndex(EntryIndex) || Count <= 0) return;
	const FShopItemEntry& Entry = Items[EntryIndex];
	if (Cart.Num() != CartSlotCount) Cart.SetNum(CartSlotCount);

	int32 Remaining = Count;
	// 1) 같은 아이템 칸부터 칸당 최대치까지
	for (FShopCartSlot& CartSlot : Cart)
	{
		if (Remaining <= 0) break;
		if (CartSlot.ItemId != Entry.ItemId || CartSlot.Count >= CartSlotMax) continue;
		const int32 Add = FMath::Min(Remaining, CartSlotMax - CartSlot.Count);
		CartSlot.Count += Add;
		Remaining -= Add;
	}
	// 2) 남으면 빈 칸에
	for (FShopCartSlot& CartSlot : Cart)
	{
		if (Remaining <= 0) break;
		if (!CartSlot.IsEmpty()) continue;
		CartSlot.ItemId = Entry.ItemId;
		CartSlot.UnitPrice = Entry.Price;
		CartSlot.Count = FMath::Min(Remaining, CartSlotMax);
		Remaining -= CartSlot.Count;
	}

	if (Remaining > 0)
	{
		Notify(NSLOCTEXT("Shop", "CartFull", "구매 목록이 가득 찼습니다."));
	}
	RefreshCart();
}

int64 UShopWidget::GetCartTotal() const
{
	int64 Total = 0;
	for (const FShopCartSlot& CartSlot : Cart)
	{
		if (!CartSlot.IsEmpty()) Total += CartSlot.UnitPrice * CartSlot.Count;
	}
	return Total;
}

void UShopWidget::RefreshCart()
{
	for (int32 i = 0; i < CartWidgets.Num(); ++i)
	{
		if (CartWidgets[i]) CartWidgets[i]->SetCartSlot(Cart.IsValidIndex(i) ? Cart[i] : FShopCartSlot());
	}
	RefreshTotals();
}

void UShopWidget::RefreshTotals()
{
	if (!TotalText || !BalanceText) return;
	const bool bSell = CurrentTab == EShopTab::Sell;
	const int64 Total = bSell ? (Inventory.IsValid() ? Inventory->GetSellReserveValue() : 0) : GetCartTotal();
	const int64 Silling = Inventory.IsValid() ? Inventory->GetSilling() : 0;
	const int64 Balance = bSell ? Silling + Total : Silling - Total;
	TotalText->SetText(FText::FromString(LoAItems::FormatWithCommas(Total)));
	BalanceText->SetText(FText::FromString(LoAItems::FormatWithCommas(Balance)));
	// 모자라면 빨갛게
	BalanceText->SetColorAndOpacity(FSlateColor(Balance < 0 ? ShopShortColor : ShopBalanceColor));
}

void UShopWidget::HandleSillingChanged(int64 NewAmount)
{
	RefreshTotals();
}

void UShopWidget::HandleBuyClicked()
{
	if (CurrentTab == EShopTab::Sell)
	{
		UInventoryComponent* SellInv = Inventory.Get();
		if (!SellInv) return;
		if (SellInv->GetSellReserve().Num() == 0)
		{
			Notify(NSLOCTEXT("Shop", "SellEmpty", "판매할 아이템을 올려 주세요."), NSLOCTEXT("Shop", "SellEmptyHow", "소지품에서 아이템을 우클릭하면 판매 목록에 올라갑니다."));
			return;
		}
		const int64 Earned = SellInv->SellReserved();
		UE_LOG(LogTemp, Log, TEXT("[Shop] 판매 — %s 실링, 잔액 %s"), *LoAItems::FormatWithCommas(Earned), *LoAItems::FormatWithCommas(SellInv->GetSilling()));
		Notify(NSLOCTEXT("Shop", "Sold", "판매했습니다."));
		return;
	}

	UInventoryComponent* Inv = Inventory.Get();
	const int64 Total = GetCartTotal();
	if (!Inv || Total <= 0)
	{
		if (Inv && Total <= 0)
		{
			// 0실링 아이템만 담긴 경우도 있으니 목록이 비었을 때만 안내
			bool bAny = false;
			for (const FShopCartSlot& CartSlot : Cart) bAny |= !CartSlot.IsEmpty();
			if (!bAny)
			{
				Notify(NSLOCTEXT("Shop", "CartEmpty", "구매할 아이템을 담아 주세요."), NSLOCTEXT("Shop", "CartEmptyHow", "목록을 우클릭하면 구매 목록에 담깁니다."));
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (Inv->GetSilling() < Total)
	{
		Notify(NSLOCTEXT("Shop", "NotEnough", "실링이 부족합니다."));
		return;
	}

	TMap<FName, int32> Totals;
	for (const FShopCartSlot& CartSlot : Cart)
	{
		if (!CartSlot.IsEmpty()) Totals.FindOrAdd(CartSlot.ItemId) += CartSlot.Count;
	}
	if (!Inv->CanAddItems(Totals))
	{
		Notify(NSLOCTEXT("Shop", "NoSpace", "소지품 공간이 부족합니다."));
		return;
	}

	Inv->SpendSilling(Total);
	for (const TPair<FName, int32>& Pair : Totals)
	{
		Inv->AddItem(Pair.Key, Pair.Value);
	}

	UE_LOG(LogTemp, Log, TEXT("[Shop] 구매 — %s 실링, 잔액 %s"), *LoAItems::FormatWithCommas(Total), *LoAItems::FormatWithCommas(Inv->GetSilling()));
	Notify(NSLOCTEXT("Shop", "Bought", "구매했습니다."));

	Cart.Reset();
	Cart.SetNum(CartSlotCount);
	RefreshCart();
}

void UShopWidget::HandleClearClicked()
{
	if (CurrentTab == EShopTab::Sell)
	{
		if (UInventoryComponent* Inv = Inventory.Get()) Inv->UnreserveAllSell();
		return;
	}
	Cart.Reset();
	Cart.SetNum(CartSlotCount);
	RefreshCart();
}

void UShopWidget::Notify(const FText& Title, const FText& Message)
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->ShowTimedNotice(Title, Message, 1.5f);
	}
}

// ── 묶음 구매 팝업 ──

void UShopWidget::OpenQuantityPopup(EPopupMode Mode, int32 TargetIndex, int32 MaxQuantity)
{
	if (!PopupLayer) return;

	FName ItemId;
	if (Mode == EPopupMode::Buy)
	{
		if (!Items.IsValidIndex(TargetIndex)) return;
		ItemId = Items[TargetIndex].ItemId;
	}
	else
	{
		const UInventoryComponent* Inv = Inventory.Get();
		if (!Inv || !Inv->IsValidIndex(TargetIndex)) return;
		ItemId = Inv->GetSlot(TargetIndex).ItemId;
	}

	PopupMode = Mode;
	PopupEntryIndex = TargetIndex;
	PopupMax = FMath::Max(1, MaxQuantity);

	if (PopupTitleText)
	{
		PopupTitleText->SetText(Mode == EPopupMode::Buy ? NSLOCTEXT("Shop", "BulkTitle", "묶음 구매") : NSLOCTEXT("Shop", "BulkSellTitle", "묶음 판매"));
	}
	if (PopupAskText)
	{
		PopupAskText->SetText(Mode == EPopupMode::Buy ? NSLOCTEXT("Shop", "BulkAsk", "구매하시겠습니까?") : NSLOCTEXT("Shop", "BulkSellAsk", "판매하시겠습니까?"));
	}
	if (const FItemDefinition* Def = LoAItems::Find(ItemId))
	{
		if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
		{
			PopupIcon->SetBrushFromTexture(Texture);
		}
	}
	// 판매는 칸에 있는 전부가 기본값 (로아와 같이), 구매는 1개
	SetPopupQuantity(Mode == EPopupMode::Sell ? PopupMax : 1);
	PopupLayer->SetVisibility(ESlateVisibility::Visible);
	if (PopupQuantityBox)
	{
		PopupQuantityBox->SetKeyboardFocus();
	}
}

void UShopWidget::CloseQuantityPopup()
{
	PopupEntryIndex = INDEX_NONE;
	if (PopupLayer) PopupLayer->SetVisibility(ESlateVisibility::Collapsed);
}

int32 UShopWidget::GetPopupQuantity() const
{
	if (!PopupQuantityBox) return 1;
	const FString Text = PopupQuantityBox->GetText().ToString();
	return FMath::Clamp(Text.IsEmpty() ? 1 : FCString::Atoi(*Text), 1, PopupMax);
}

void UShopWidget::SetPopupQuantity(int32 Quantity)
{
	if (!PopupQuantityBox) return;
	bUpdatingPopupText = true;
	PopupQuantityBox->SetText(FText::AsNumber(FMath::Clamp(Quantity, 1, PopupMax), &FNumberFormattingOptions::DefaultNoGrouping()));
	bUpdatingPopupText = false;
}

void UShopWidget::HandlePopupTextChanged(const FText& Text)
{
	if (bUpdatingPopupText) return;

	// 숫자만 남기고, 칸당 최대치를 넘으면 최대치로 — 빈 칸은 입력 중일 수 있어 그대로 둔다
	const FString Raw = Text.ToString();
	FString Digits;
	for (const TCHAR C : Raw)
	{
		if (FChar::IsDigit(C)) Digits.AppendChar(C);
	}
	if (!Digits.IsEmpty() && FCString::Atoi(*Digits) > PopupMax)
	{
		Digits = FString::FromInt(PopupMax);
	}
	if (Digits != Raw)
	{
		bUpdatingPopupText = true;
		PopupQuantityBox->SetText(FText::FromString(Digits));
		bUpdatingPopupText = false;
	}
}

void UShopWidget::HandlePopupTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandlePopupConfirm();
	}
	else if (PopupEntryIndex != INDEX_NONE)
	{
		SetPopupQuantity(GetPopupQuantity());
	}
}

void UShopWidget::HandlePopupUp()
{
	SetPopupQuantity(GetPopupQuantity() + 1);
}

void UShopWidget::HandlePopupDown()
{
	SetPopupQuantity(GetPopupQuantity() - 1);
}

void UShopWidget::HandlePopupConfirm()
{
	if (PopupEntryIndex == INDEX_NONE) return;
	const int32 Target = PopupEntryIndex;
	const int32 Quantity = GetPopupQuantity();
	const EPopupMode Mode = PopupMode;
	CloseQuantityPopup();

	if (Mode == EPopupMode::Buy)
	{
		AddToCart(Target, Quantity);
	}
	else if (UInventoryComponent* Inv = Inventory.Get())
	{
		FText Reason;
		if (!Inv->ReserveForSell(Target, Quantity, Reason) && !Reason.IsEmpty())
		{
			Notify(Reason);
		}
	}
}

void UShopWidget::HandlePopupCancel()
{
	CloseQuantityPopup();
}

// ── 탭 / 판매 ──

void UShopWidget::HandleBuyTabClicked()
{
	SetTab(EShopTab::Buy);
}

void UShopWidget::HandleSellTabClicked()
{
	SetTab(EShopTab::Sell);
}

void UShopWidget::SetTab(EShopTab Tab)
{
	CurrentTab = Tab;
	CloseQuantityPopup();
	const bool bSell = Tab == EShopTab::Sell;

	auto StyleTab = [](UButton* Button, UTextBlock* Text, bool bActive)
	{
		if (!Button) return;
		FButtonStyle Style = Button->GetStyle();
		const FLinearColor Base = bActive ? FLinearColor(0.20f, 0.36f, 0.45f, 1.f) : FLinearColor(0.12f, 0.13f, 0.15f, 1.f);
		Style.Normal.TintColor = FSlateColor(Base);
		Style.Hovered.TintColor = FSlateColor(Base * 1.35f);
		Style.Pressed.TintColor = FSlateColor(Base * 0.75f);
		Button->SetStyle(Style);
		if (Text) Text->SetColorAndOpacity(FSlateColor(bActive ? ShopTextColor : ShopDimTextColor));
	};
	StyleTab(BuyTabButton, BuyTabText, !bSell);
	StyleTab(SellTabButton, SellTabText, bSell);

	if (HintText)
	{
		HintText->SetText(bSell ? NSLOCTEXT("Shop", "HintSell", "[Shift+우클릭] 판매 수량 선택") : NSLOCTEXT("Shop", "Hint", "[Shift+우클릭] 구매 수량 선택"));
	}
	// 판매 탭엔 분류·구매 목록 칸이 없다 — 자리는 남겨 창 크기가 바뀌지 않게 Hidden
	if (CategoryRow) CategoryRow->SetVisibility(bSell ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	if (CartRowWidget) CartRowWidget->SetVisibility(bSell ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	if (TotalLabel) TotalLabel->SetText(bSell ? NSLOCTEXT("Shop", "SellTotal", "판매 금액") : NSLOCTEXT("Shop", "Total", "구매 금액"));
	if (BalanceLabel) BalanceLabel->SetText(bSell ? NSLOCTEXT("Shop", "SellBalance", "판매 후 잔액") : NSLOCTEXT("Shop", "Balance", "구매 후 잔액"));
	if (ActionButtonText) ActionButtonText->SetText(bSell ? NSLOCTEXT("Shop", "Sell", "판매") : NSLOCTEXT("Shop", "Buy", "구매"));

	RefreshEntries();
	RefreshCart();
}

void UShopWidget::RefreshEntries()
{
	if (CurrentTab == EShopTab::Sell)
	{
		static const TArray<FSellReserveEntry> NoReserve;
		const TArray<FSellReserveEntry>& Reserve = Inventory.IsValid() ? Inventory->GetSellReserve() : NoReserve;
		for (int32 i = 0; i < EntryWidgets.Num(); ++i)
		{
			if (!EntryWidgets[i]) continue;
			if (Reserve.IsValidIndex(i))
			{
				const FItemDefinition* Def = LoAItems::Find(Reserve[i].ItemId);
				const FEquipmentInstance* Equipment = Reserve[i].EquipmentId.IsValid() ? Inventory->FindEquipment(Reserve[i].EquipmentId) : nullptr;
				EntryWidgets[i]->SetSellEntry(Reserve[i].ItemId, Reserve[i].Count, Def ? Def->SellPrice * Reserve[i].Count : 0, Equipment);
			}
			else
			{
				EntryWidgets[i]->SetSellEntry(NAME_None, 0, 0);
			}
		}
	}
	else
	{
		for (int32 i = 0; i < EntryWidgets.Num(); ++i)
		{
			if (EntryWidgets[i]) EntryWidgets[i]->SetEntry(Items.IsValidIndex(i) ? &Items[i] : nullptr);
		}
	}
}

void UShopWidget::HandleSellReserveChanged()
{
	if (CurrentTab == EShopTab::Sell)
	{
		RefreshEntries();
	}
	RefreshTotals();
}

void UShopWidget::HandleInventoryRightClick(int32 InventoryIndex, bool bShift)
{
	UInventoryComponent* Inv = Inventory.Get();
	if (!Inv || !Inv->IsValidIndex(InventoryIndex)) return;
	if (PopupLayer && PopupLayer->GetVisibility() != ESlateVisibility::Collapsed) return;

	const FInventorySlot& InvSlot = Inv->GetSlot(InventoryIndex);
	if (InvSlot.IsEmpty()) return;

	if (CurrentTab != EShopTab::Sell)
	{
		SetTab(EShopTab::Sell);
	}

	// 팝업 띄우기 전에 팔 수 있는지 먼저 — 확인을 누른 뒤에야 거절되면 번거롭다
	const FItemDefinition* Def = LoAItems::Find(InvSlot.ItemId);
	if (InvSlot.bLocked)
	{
		Notify(NSLOCTEXT("Inventory", "SellLocked", "잠긴 아이템은 판매할 수 없습니다."));
		return;
	}
	if (!Def || Def->SellPrice <= 0)
	{
		Notify(NSLOCTEXT("Inventory", "SellNotAllowed", "판매할 수 없는 아이템입니다."));
		return;
	}

	if (bShift && InvSlot.Count > 1)
	{
		OpenQuantityPopup(EPopupMode::Sell, InventoryIndex, InvSlot.Count);
		return;
	}

	// 우클릭은 1개씩 — 여러 개는 Shift+우클릭(묶음 판매)
	FText Reason;
	if (!Inv->ReserveForSell(InventoryIndex, 1, Reason) && !Reason.IsEmpty())
	{
		Notify(Reason);
	}
}

// ── 창 끌기 / 클릭 차단 ──

FReply UShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UShopWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UShopWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UShopWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UShopWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

// ═══ 하단 대화창 ═════════════════════════════════════════════════════════════

TSharedRef<SWidget> UShopDialogueWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopDialogueRoot"));
		WidgetTree->RootWidget = Root;

		// 화면 아래 전체 폭 어두운 띠
		UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopDialogueBar"));
		Bar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Bar->SetHorizontalAlignment(HAlign_Center);
		Bar->SetVerticalAlignment(VAlign_Center);
		Bar->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Bar))
		{
			S->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			S->SetAlignment(FVector2D(0.f, 1.f));
			S->SetOffsets(FMargin(0.f, 0.f, 0.f, 110.f));
		}

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopDialogueLines"));
		Bar->SetContent(Lines);
		SpeakerText = MakeShopText(WidgetTree, TEXT("ShopDialogueSpeaker"), 15, FLinearColor(1.f, 0.82f, 0.35f, 1.f));
		if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(SpeakerText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		LineText = MakeShopText(WidgetTree, TEXT("ShopDialogueLine"), 17, FLinearColor(0.95f, 0.95f, 0.95f, 1.f));
		LineText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(LineText)) S->SetHorizontalAlignment(HAlign_Center);

		// [나가기] — 오른쪽 아래
		UButton* ExitButton = nullptr;
		USizeBox* ExitBox = MakeShopButton(WidgetTree, TEXT("ShopExit"), NSLOCTEXT("Shop", "Exit", "나가기"), 120.f, 38.f, 12, ExitButton);
		ExitButton->OnClicked.AddDynamic(this, &UShopDialogueWidget::HandleExitClicked);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(ExitBox))
		{
			S->SetAnchors(FAnchors(1.f, 1.f));
			S->SetAlignment(FVector2D(1.f, 1.f));
			S->SetPosition(FVector2D(-30.f, -36.f));
			S->SetAutoSize(true);
		}
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Super::RebuildWidget();
}

void UShopDialogueWidget::SetDialogue(const FText& Speaker, const FText& Line)
{
	if (!SpeakerText) TakeWidget();
	if (SpeakerText) SpeakerText->SetText(Speaker);
	if (LineText) LineText->SetText(Line);
}

FReply UShopDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 대화창 띠 위 클릭은 게임으로 안 넘김
	return FReply::Handled();
}

FReply UShopDialogueWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

void UShopDialogueWidget::HandleExitClicked()
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->CloseNPCMenu();
	}
}
