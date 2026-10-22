#include "UI/RefineWidget.h"
#include "Item/EquipmentComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/RefineLibrary.h"
#include "Stat/StatComponent.h"
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
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// 로아 재련 창 톤 — 짙은 남색 바탕 + 금색 강조 + 주황 장비 이름
	const FLinearColor Refine_PanelColor(0.022f, 0.027f, 0.04f, 0.97f);
	const FLinearColor Refine_ColumnColor(0.035f, 0.042f, 0.06f, 1.f);
	const FLinearColor Refine_RowColor(0.05f, 0.058f, 0.08f, 1.f);
	const FLinearColor Refine_FrameNormal(0.14f, 0.15f, 0.19f, 1.f);
	const FLinearColor Refine_FrameHover(0.30f, 0.75f, 0.85f, 1.f);
	const FLinearColor Refine_FrameSelected(1.f, 0.78f, 0.3f, 1.f);
	const FLinearColor Refine_TextColor(0.88f, 0.88f, 0.9f, 1.f);
	const FLinearColor Refine_DimTextColor(0.55f, 0.56f, 0.6f, 1.f);
	const FLinearColor Refine_GoldColor(1.f, 0.82f, 0.4f, 1.f);
	const FLinearColor Refine_NameColor(1.f, 0.58f, 0.22f, 1.f);
	const FLinearColor Refine_EnoughColor(0.6f, 0.92f, 0.42f, 1.f);
	const FLinearColor Refine_ShortColor(1.f, 0.35f, 0.3f, 1.f);
	const FLinearColor Refine_FailColor(0.62f, 0.63f, 0.66f, 1.f);
	const FLinearColor Refine_AnimColor(1.f, 0.6f, 0.2f, 1.f);

	UTextBlock* Refine_MakeText(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		return Block;
	}

	// 원 — 텍스처 없이 RoundedBox(반지름 = 높이 절반)로 그린다. 색은 SetColorAndOpacity
	void Refine_MakeCircle(UImage* Image, const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Color);
	}

	// 버튼 + 가운데 글자, 고정 크기 SizeBox로 감싸 반환
	USizeBox* Refine_MakeButton(UWidgetTree* Tree, const FString& Name, const FText& Label, float Width, float Height, int32 FontSize,
		const FLinearColor& Base, UButton*& OutButton, UTextBlock** OutText = nullptr)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name + TEXT("Box"))));
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(Height);
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*Name));
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(Base);
		Style.Hovered.TintColor = FSlateColor(Base * 1.5f);
		Style.Pressed.TintColor = FSlateColor(Base * 0.7f);
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.09f, 0.095f, 0.11f, 1.f));
		Style.NormalPadding = FMargin(2.f);
		Style.PressedPadding = FMargin(2.f);
		Button->SetStyle(Style);
		Box->SetContent(Button);
		UTextBlock* Text = Refine_MakeText(Tree, FName(*(Name + TEXT("Text"))), FontSize, Refine_TextColor, Label);
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);
		OutButton = Button;
		if (OutText) *OutText = Text;
		return Box;
	}

	UProgressBar* Refine_MakeBar(UWidgetTree* Tree, const FName& Name, const FLinearColor& Fill)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.08f, 0.09f, 0.11f, 1.f));
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(Fill);
		Bar->SetPercent(0.f);
		return Bar;
	}

	void Refine_SetIcon(UImage* Image, UTexture2D* Texture, const FLinearColor& Fallback)
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

	FText Refine_Percent(float Ratio)
	{
		FNumberFormattingOptions Fmt;
		Fmt.MinimumFractionalDigits = 2;
		Fmt.MaximumFractionalDigits = 2;
		return FText::Format(NSLOCTEXT("Refine", "Percent", "{0}%"), FText::AsNumber(Ratio * 100.f, &Fmt));
	}

	FText Refine_Number(int64 Value)
	{
		return FText::FromString(LoAItems::FormatWithCommas(Value));
	}

	// 장비를 낀 캐릭터의 주 스탯 이름 (소서리스 = 지능)
	FString Refine_PrimaryStatName(const UEquipmentComponent* Equip)
	{
		const UStatComponent* Stats = (Equip && Equip->GetOwner()) ? Equip->GetOwner()->FindComponentByClass<UStatComponent>() : nullptr;
		const EPrimaryStat Type = Stats ? Stats->GetPrimaryStatType() : EPrimaryStat::Intelligence;
		return StaticEnum<EPrimaryStat>()->GetDisplayNameTextByValue(static_cast<int64>(Type)).ToString();
	}

	void Refine_CenterSlot(UVerticalBoxSlot* S, float Top = 0.f, float Bottom = 0.f)
	{
		if (!S) return;
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, Top, 0.f, Bottom));
	}
}

// ═══ 부위 한 줄 ══════════════════════════════════════════════════════════════

void URefinePartRowWidget::Setup(URefineWidget* InOwner, EEquipSlot InSlot)
{
	Owner = InOwner;
	PartSlot = InSlot;
}

TSharedRef<SWidget> URefinePartRowWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PartBox"));
		Box->SetHeightOverride(54.f);
		WidgetTree->RootWidget = Box;

		Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PartFrame"));
		Frame->SetBrushColor(Refine_FrameNormal);
		Frame->SetPadding(FMargin(1.f));
		Box->SetContent(Frame);
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PartInner"));
		Inner->SetBrushColor(Refine_RowColor);
		Inner->SetPadding(FMargin(4.f, 3.f));
		Frame->SetContent(Inner);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PartRow"));
		Inner->SetContent(Row);

		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PartIconBox"));
		IconBox->SetWidthOverride(44.f);
		IconBox->SetHeightOverride(44.f);
		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PartIcon"));
		IconBox->SetContent(IconImage);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PartTexts"));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Texts))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		LevelText = Refine_MakeText(WidgetTree, TEXT("PartLevel"), 9, Refine_GoldColor);
		if (UVerticalBoxSlot* S = Texts->AddChildToVerticalBox(LevelText)) S->SetHorizontalAlignment(HAlign_Center);
		NameText = Refine_MakeText(WidgetTree, TEXT("PartName"), 11, Refine_NameColor);
		if (UVerticalBoxSlot* S = Texts->AddChildToVerticalBox(NameText)) S->SetHorizontalAlignment(HAlign_Center);
	}
	SetVisibility(ESlateVisibility::Visible);
	return Super::RebuildWidget();
}

void URefinePartRowWidget::SetPart(const FEquipmentInstance* Instance, bool bInSelected)
{
	if (!NameText) TakeWidget();
	const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
	bHasPart = Def != nullptr;
	bSelected = bInSelected;

	if (bHasPart)
	{
		Refine_SetIcon(IconImage, Def->Icon.LoadSynchronous(), FLinearColor(0.3f, 0.3f, 0.35f, 1.f));
		LevelText->SetText(FText::Format(NSLOCTEXT("Refine", "PartLevel", "{0} 단계"), FText::AsNumber(Instance->RefineLevel)));
		NameText->SetText(Def->DisplayName);
		NameText->SetColorAndOpacity(FSlateColor(Refine_NameColor));
	}
	else
	{
		Refine_SetIcon(IconImage, nullptr, FLinearColor(0.f, 0.f, 0.f, 0.f));
		LevelText->SetText(FText::GetEmpty());
		NameText->SetText(FText::Format(NSLOCTEXT("Refine", "PartEmpty", "{0} — 미착용"),
			StaticEnum<EEquipSlot>()->GetDisplayNameTextByValue(static_cast<int64>(PartSlot))));
		NameText->SetColorAndOpacity(FSlateColor(Refine_DimTextColor));
	}
	ApplyFrame();
}

void URefinePartRowWidget::ApplyFrame()
{
	if (!Frame) return;
	Frame->SetBrushColor(bSelected ? Refine_FrameSelected : (bHasPart && bHovered ? Refine_FrameHover : Refine_FrameNormal));
}

FReply URefinePartRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bHasPart && Owner.IsValid())
	{
		Owner->SelectPart(PartSlot);
	}
	return FReply::Handled();
}

FReply URefinePartRowWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URefinePartRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyFrame();
}

void URefinePartRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyFrame();
}

// ═══ 재련 창 — 트리 ══════════════════════════════════════════════════════════

TSharedRef<SWidget> URefineWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RefineRoot"));
		WidgetTree->RootWidget = Root;

		// 창 — 1200×700. 처음 1040×550은 가운데 열 내용(약 600px)보다 낮아 아래 버튼이 삐져나왔음
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefinePanel"));
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
		USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineSize"));
		PanelSize->SetWidthOverride(1200.f);
		PanelSize->SetHeightOverride(700.f);
		Panel->SetContent(PanelSize);
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefineBack"));
		Back->SetBrushColor(Refine_PanelColor);
		Back->SetPadding(FMargin(10.f));
		PanelSize->SetContent(Back);

		// 본문 위에 성장 팝업이 겹친다
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RefineLayers"));
		Back->SetContent(Layers);

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RefineColumns"));
		if (UOverlaySlot* S = Layers->AddChildToOverlay(Columns))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildLeftColumn())) S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildCenterColumn())) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (UHorizontalBoxSlot* S = Columns->AddChildToHorizontalBox(BuildRightColumn())) S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

		ConfirmPopup = BuildConfirmPopup();
		ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Layers->AddChildToOverlay(ConfirmPopup))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		GrowthPopup = BuildGrowthPopup();
		GrowthPopup->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Layers->AddChildToOverlay(GrowthPopup))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		SuccessPopup = BuildSuccessPopup();
		SuccessPopup->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Layers->AddChildToOverlay(SuccessPopup))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

UWidget* URefineWidget::BuildLeftColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineLeftBox"));
	Box->SetWidthOverride(290.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefineLeftBack"));
	Back->SetBrushColor(Refine_ColumnColor);
	Back->SetPadding(FMargin(6.f));
	Box->SetContent(Back);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RefineLeft"));
	Back->SetContent(Column);

	// "☰ 전체" 필터 줄 (지금은 표시만)
	UBorder* Filter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefineFilter"));
	Filter->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.1f, 1.f));
	Filter->SetPadding(FMargin(8.f, 4.f));
	Filter->SetContent(Refine_MakeText(WidgetTree, TEXT("RefineFilterText"), 10, Refine_TextColor, NSLOCTEXT("Refine", "Filter", "☰  전체")));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Filter)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	PartRows.Reset();
	// 로아처럼 무기가 맨 위
	const EEquipSlot Order[] = { EEquipSlot::Weapon, EEquipSlot::Helmet, EEquipSlot::Chest, EEquipSlot::Pants, EEquipSlot::Gloves, EEquipSlot::Shoulder };
	for (const EEquipSlot PartSlot : Order)
	{
		URefinePartRowWidget* Row = WidgetTree->ConstructWidget<URefinePartRowWidget>(URefinePartRowWidget::StaticClass(),
			FName(*FString::Printf(TEXT("RefinePart_%d"), static_cast<int32>(PartSlot))));
		Row->Setup(this, PartSlot);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		PartRows.Add(Row);
	}
	return Box;
}

UWidget* URefineWidget::MakeMaterialCell(const FString& Name, FMaterialCell& Out, UButton** OutButton)
{
	UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*(Name + TEXT("Cell"))));

	USizeBox* CircleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name + TEXT("Box"))));
	CircleBox->SetWidthOverride(54.f);
	CircleBox->SetHeightOverride(54.f);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*(Name + TEXT("Layers"))));

	Out.Ring = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name + TEXT("Ring"))));
	Refine_MakeCircle(Out.Ring, FLinearColor(0.35f, 0.32f, 0.25f, 1.f));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(Out.Ring))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	UImage* Inner = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name + TEXT("Inner"))));
	Refine_MakeCircle(Inner, FLinearColor(0.05f, 0.06f, 0.09f, 1.f));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(Inner))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(2.f));
	}
	Out.Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name + TEXT("Icon"))));
	if (UOverlaySlot* S = Layers->AddChildToOverlay(Out.Icon))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(7.f));
	}

	if (OutButton)
	{
		// 누를 수 있는 칸(숨결) — 투명 버튼으로 감싼다
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*(Name + TEXT("Button"))));
		FButtonStyle Style = Button->GetStyle();
		const FLinearColor Clear(1.f, 1.f, 1.f, 0.f);
		Style.Normal.TintColor = FSlateColor(Clear);
		Style.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
		Style.Pressed.TintColor = FSlateColor(Clear);
		Style.Disabled.TintColor = FSlateColor(Clear);
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Button->SetStyle(Style);
		Button->SetContent(Layers);
		CircleBox->SetContent(Button);
		*OutButton = Button;
	}
	else
	{
		CircleBox->SetContent(Layers);
	}
	Refine_CenterSlot(Cell->AddChildToVerticalBox(CircleBox));

	Out.Count = Refine_MakeText(WidgetTree, FName(*(Name + TEXT("Count"))), 10, Refine_EnoughColor);
	Refine_CenterSlot(Cell->AddChildToVerticalBox(Out.Count), 3.f);
	Out.Root = Cell;
	return Cell;
}

UWidget* URefineWidget::MakeCostRow(const FString& Name, const FText& Label, UTextBlock*& OutValue)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*(Name + TEXT("Row"))));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(Name + TEXT("LabelBox"))));
	LabelBox->SetWidthOverride(90.f);
	LabelBox->SetContent(Refine_MakeText(WidgetTree, FName(*(Name + TEXT("Label"))), 10, Refine_DimTextColor, Label));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) S->SetVerticalAlignment(VAlign_Center);

	OutValue = Refine_MakeText(WidgetTree, FName(*(Name + TEXT("Value"))), 11, Refine_TextColor);
	OutValue->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutValue))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetHorizontalAlignment(HAlign_Right);
	}
	UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*(Name + TEXT("Coin"))));
	Refine_SetIcon(Coin, GoldIcon.LoadSynchronous(), FLinearColor(0.95f, 0.72f, 0.2f, 1.f));
	Coin->SetDesiredSizeOverride(FVector2D(16.f, 16.f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Coin))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
	}
	return Row;
}

UWidget* URefineWidget::BuildCenterColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RefineCenter"));

	// 제목 (끌기 손잡이)
	HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineHeader"));
	HeaderBox->SetHeightOverride(28.f);
	UOverlay* HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RefineHeaderOverlay"));
	HeaderBox->SetContent(HeaderOverlay);
	if (UOverlaySlot* S = HeaderOverlay->AddChildToOverlay(Refine_MakeText(WidgetTree, TEXT("RefineTitle"), 15, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Refine", "Title", "장비 재련"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Column->AddChildToVerticalBox(HeaderBox);

	// 장비 아이콘 (금테)
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineIconBox"));
	IconBox->SetWidthOverride(48.f);
	IconBox->SetHeightOverride(48.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefineIconFrame"));
	IconFrame->SetBrushColor(Refine_FrameSelected);
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	DetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RefineIcon"));
	IconFrame->SetContent(DetailIcon);
	Refine_CenterSlot(Column->AddChildToVerticalBox(IconBox), 4.f, 4.f);

	DetailName = Refine_MakeText(WidgetTree, TEXT("RefineName"), 14, Refine_NameColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(DetailName));
	LevelText = Refine_MakeText(WidgetTree, TEXT("RefineLevel"), 13, Refine_GoldColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(LevelText), 2.f);
	EnergyText = Refine_MakeText(WidgetTree, TEXT("RefineEnergy"), 9, Refine_TextColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(EnergyText), 1.f, 6.f);

	// 장비 성장 원 — 테두리 원 + 안쪽 원 + 가운데 % / 아래 [성장]
	USizeBox* CircleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineCircleBox"));
	CircleBox->SetWidthOverride(140.f);
	CircleBox->SetHeightOverride(140.f);
	UOverlay* Circle = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RefineCircle"));
	CircleBox->SetContent(Circle);
	GrowthRing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RefineGrowthRing"));
	Refine_MakeCircle(GrowthRing, FLinearColor(0.3f, 0.3f, 0.33f, 1.f));
	if (UOverlaySlot* S = Circle->AddChildToOverlay(GrowthRing))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	GrowthCircle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RefineGrowthCircle"));
	Refine_MakeCircle(GrowthCircle, FLinearColor(0.06f, 0.065f, 0.08f, 1.f));
	if (UOverlaySlot* S = Circle->AddChildToOverlay(GrowthCircle))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(5.f));
	}
	GrowthPercentText = Refine_MakeText(WidgetTree, TEXT("RefineGrowthPercent"), 20, FLinearColor::White);
	GrowthPercentText->SetShadowOffset(FVector2D(1.f, 1.f));
	GrowthPercentText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	if (UOverlaySlot* S = Circle->AddChildToOverlay(GrowthPercentText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Refine_CenterSlot(Column->AddChildToVerticalBox(CircleBox));

	UTextBlock* GrowthText = nullptr;
	UButton* Growth = nullptr;
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeButton(WidgetTree, TEXT("RefineGrowthButton"), NSLOCTEXT("Refine", "Grow", "성장"),
		110.f, 28.f, 11, FLinearColor(0.16f, 0.17f, 0.2f, 1.f), Growth, &GrowthText)), 8.f);
	GrowthButton = Growth;
	GrowthButtonText = GrowthText;
	GrowthButton->OnClicked.AddDynamic(this, &URefineWidget::HandleGrowthClicked);

	WarningText = Refine_MakeText(WidgetTree, TEXT("RefineWarning"), 10, Refine_ShortColor, NSLOCTEXT("Refine", "Warning", "▲ 장비 성장 완료 후, 재련할 수 있습니다"));
	Refine_CenterSlot(Column->AddChildToVerticalBox(WarningText), 4.f);
	ChanceText = Refine_MakeText(WidgetTree, TEXT("RefineChance"), 9, Refine_DimTextColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(ChanceText), 2.f, 6.f);

	// 재료 원 2개 — 돌 / 돌파석 (숨결은 재련 확인 팝업에서 개수를 고른다)
	UHorizontalBox* Materials = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RefineMaterials"));
	Refine_CenterSlot(Column->AddChildToVerticalBox(Materials), 0.f, 8.f);
	MaterialCells.SetNum(2);
	for (int32 i = 0; i < MaterialCells.Num(); ++i)
	{
		UWidget* Cell = MakeMaterialCell(FString::Printf(TEXT("RefineMat_%d"), i), MaterialCells[i]);
		if (UHorizontalBoxSlot* S = Materials->AddChildToHorizontalBox(Cell)) S->SetPadding(FMargin(12.f, 0.f));
	}

	// 재련 비용 / 소지 금액 (골드)
	USizeBox* CostBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineCostBox"));
	CostBox->SetWidthOverride(340.f);
	UVerticalBox* Costs = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RefineCosts"));
	CostBox->SetContent(Costs);
	UTextBlock* CostValue = nullptr;
	UTextBlock* HaveValue = nullptr;
	if (UVerticalBoxSlot* S = Costs->AddChildToVerticalBox(MakeCostRow(TEXT("RefineCost"), NSLOCTEXT("Refine", "Cost", "재련 비용"), CostValue))) S->SetPadding(FMargin(0.f, 1.f));
	if (UVerticalBoxSlot* S = Costs->AddChildToVerticalBox(MakeCostRow(TEXT("RefineHave"), NSLOCTEXT("Refine", "Have", "소지 금액"), HaveValue))) S->SetPadding(FMargin(0.f, 1.f));
	CostGoldText = CostValue;
	HaveGoldText = HaveValue;
	Refine_CenterSlot(Column->AddChildToVerticalBox(CostBox), 0.f, 6.f);

	// 연출 막대 + 결과
	USizeBox* AnimBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineAnimBox"));
	AnimBox->SetWidthOverride(440.f);
	AnimBox->SetHeightOverride(5.f);
	AnimBar = Refine_MakeBar(WidgetTree, TEXT("RefineAnimBar"), Refine_AnimColor);
	AnimBox->SetContent(AnimBar);
	Refine_CenterSlot(Column->AddChildToVerticalBox(AnimBox), 0.f, 3.f);
	ResultText = Refine_MakeText(WidgetTree, TEXT("RefineResult"), 12, Refine_GoldColor);
	ResultText->SetJustification(ETextJustify::Center);
	Refine_CenterSlot(Column->AddChildToVerticalBox(ResultText), 0.f, 4.f);

	// 남는 공간은 버튼 위에
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("RefineFill"))))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// [장비 재련] (금테) [장비 재련 10회]
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RefineButtons"));
	Refine_CenterSlot(Column->AddChildToVerticalBox(Buttons), 4.f, 6.f);
	{
		UBorder* Gilded = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefineButtonFrame"));
		Gilded->SetBrushColor(Refine_FrameSelected);
		Gilded->SetPadding(FMargin(2.f));
		UButton* Refine = nullptr;
		Gilded->SetContent(Refine_MakeButton(WidgetTree, TEXT("RefineButton"), NSLOCTEXT("Refine", "Button", "장비 재련"),
			210.f, 40.f, 14, FLinearColor(0.12f, 0.13f, 0.16f, 1.f), Refine));
		RefineButton = Refine;
		RefineButton->OnClicked.AddDynamic(this, &URefineWidget::HandleRefineClicked);
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(Gilded))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		UButton* Ten = nullptr;
		UTextBlock* TenText = nullptr;
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(Refine_MakeButton(WidgetTree, TEXT("RefineTenButton"),
			NSLOCTEXT("Refine", "TenButton", "장비 재련 10회"), 160.f, 36.f, 12, FLinearColor(0.12f, 0.13f, 0.16f, 1.f), Ten, &TenText)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		RefineTenButton = Ten;
		RefineTenText = TenText;
		RefineTenButton->OnClicked.AddDynamic(this, &URefineWidget::HandleRefineTenClicked);
	}
	return Column;
}

UWidget* URefineWidget::BuildRightColumn()
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RefineRightBox"));
	Box->SetWidthOverride(280.f);
	UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RefineRightBack"));
	Back->SetBrushColor(Refine_ColumnColor);
	Back->SetPadding(FMargin(6.f));
	Box->SetContent(Back);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RefineRight"));
	Back->SetContent(Column);
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeText(WidgetTree, TEXT("RefineStageTitle"), 12, FLinearColor(0.9f, 0.88f, 0.8f, 1.f),
		NSLOCTEXT("Refine", "StageTitle", "재련 단계"))), 2.f, 6.f);

	StageList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RefineStageList"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StageList)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Box;
}

UWidget* URefineWidget::BuildGrowthPopup()
{
	// 창 전체를 어둡게 덮고 가운데 카드
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GrowthDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	Dim->SetVisibility(ESlateVisibility::Visible);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GrowthFrame"));
	Frame->SetBrushColor(Refine_FrameSelected);
	Frame->SetPadding(FMargin(1.f));
	Dim->SetContent(Frame);
	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GrowthSize"));
	CardSize->SetWidthOverride(360.f);
	Frame->SetContent(CardSize);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GrowthCard"));
	Card->SetBrushColor(FLinearColor(0.03f, 0.036f, 0.052f, 1.f));
	Card->SetPadding(FMargin(14.f, 8.f, 14.f, 14.f));
	CardSize->SetContent(Card);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GrowthColumn"));
	Card->SetContent(Column);

	// 제목 + [X]
	UOverlay* Title = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GrowthTitle"));
	if (UOverlaySlot* S = Title->AddChildToOverlay(Refine_MakeText(WidgetTree, TEXT("GrowthTitleText"), 13, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Refine", "GrowthTitle", "장비 성장"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	UButton* Close = nullptr;
	if (UOverlaySlot* S = Title->AddChildToOverlay(Refine_MakeButton(WidgetTree, TEXT("GrowthClose"), FText::FromString(TEXT("X")), 24.f, 24.f, 10,
		FLinearColor(0.12f, 0.13f, 0.16f, 1.f), Close)))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Close->OnClicked.AddDynamic(this, &URefineWidget::HandleGrowthClose);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Title)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeText(WidgetTree, TEXT("GrowthExp"), 12, Refine_TextColor, NSLOCTEXT("Refine", "GrowthExp", "장비 경험치"))));
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeText(WidgetTree, TEXT("GrowthFragmentName"), 11, Refine_TextColor, NSLOCTEXT("Refine", "GrowthFragment", "운명의 파편"))), 8.f, 4.f);
	Refine_CenterSlot(Column->AddChildToVerticalBox(MakeMaterialCell(TEXT("GrowthFragment"), PopupFragmentCell)));
	PopupFragmentCell.Count->SetVisibility(ESlateVisibility::Collapsed);
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeText(WidgetTree, TEXT("GrowthNeedLabel"), 10, Refine_DimTextColor, NSLOCTEXT("Refine", "GrowthNeed", "필요 수량"))), 6.f);
	PopupNeedText = Refine_MakeText(WidgetTree, TEXT("GrowthNeed"), 12, Refine_EnoughColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(PopupNeedText), 0.f, 10.f);

	// 성장 비용 / 소지 금액 (실링)
	UTexture2D* SillingTexture = SillingIcon.LoadSynchronous();
	auto SillingRow = [this, Column, SillingTexture](const TCHAR* Key, const FText& Label, const FLinearColor& ValueColor) -> UTextBlock*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("Growth%sRow"), Key)));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(20.f, 1.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("Growth%sLabel"), Key)), 10, Refine_DimTextColor, Label)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* Value = Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("Growth%sValue"), Key)), 11, ValueColor);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Value)) S->SetVerticalAlignment(VAlign_Center);
		UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("Growth%sCoin"), Key)));
		Refine_SetIcon(Coin, SillingTexture, FLinearColor(0.75f, 0.77f, 0.82f, 1.f));
		Coin->SetDesiredSizeOverride(FVector2D(15.f, 15.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Coin))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
		}
		return Value;
	};
	PopupCostText = SillingRow(TEXT("Cost"), NSLOCTEXT("Refine", "GrowthCost", "성장 비용"), FLinearColor(0.6f, 0.75f, 1.f, 1.f));
	PopupHaveText = SillingRow(TEXT("Have"), NSLOCTEXT("Refine", "GrowthHave", "소지 금액"), Refine_EnoughColor);

	UButton* Grow = nullptr;
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeButton(WidgetTree, TEXT("GrowthConfirm"), NSLOCTEXT("Refine", "Grow", "성장"),
		150.f, 34.f, 12, FLinearColor(0.16f, 0.17f, 0.22f, 1.f), Grow)), 12.f);
	PopupGrowButton = Grow;
	PopupGrowButton->OnClicked.AddDynamic(this, &URefineWidget::HandleGrowthConfirm);
	return Dim;
}

UWidget* URefineWidget::BuildConfirmPopup()
{
	// 로아 [재련] 창 — 창 전체를 어둡게 덮고 가운데 카드. 특수 재료(책) 칸은 없음
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	Dim->SetVisibility(ESlateVisibility::Visible);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmFrame"));
	Frame->SetBrushColor(Refine_FrameSelected);
	Frame->SetPadding(FMargin(1.f));
	Dim->SetContent(Frame);
	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ConfirmSize"));
	CardSize->SetWidthOverride(420.f);
	Frame->SetContent(CardSize);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmCard"));
	Card->SetBrushColor(FLinearColor(0.03f, 0.036f, 0.052f, 1.f));
	Card->SetPadding(FMargin(16.f, 8.f, 16.f, 14.f));
	CardSize->SetContent(Card);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmColumn"));
	Card->SetContent(Column);

	// 제목 + [X]
	UOverlay* Title = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ConfirmTitle"));
	if (UOverlaySlot* S = Title->AddChildToOverlay(Refine_MakeText(WidgetTree, TEXT("ConfirmTitleText"), 13, FLinearColor(0.95f, 0.9f, 0.75f, 1.f), NSLOCTEXT("Refine", "ConfirmTitle", "재련"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	UButton* Close = nullptr;
	if (UOverlaySlot* S = Title->AddChildToOverlay(Refine_MakeButton(WidgetTree, TEXT("ConfirmClose"), FText::FromString(TEXT("X")), 24.f, 24.f, 10,
		FLinearColor(0.12f, 0.13f, 0.16f, 1.f), Close)))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Close->OnClicked.AddDynamic(this, &URefineWidget::HandleConfirmClose);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Title)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	ConfirmLevelText = Refine_MakeText(WidgetTree, TEXT("ConfirmLevel"), 13, Refine_GoldColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(ConfirmLevelText), 0.f, 10.f);

	// 재련 성공률 — 막대(지금 / 숨결 최대) + 가운데 글자
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeText(WidgetTree, TEXT("ConfirmChanceLabel"), 10, FLinearColor(0.55f, 0.75f, 0.95f, 1.f),
		NSLOCTEXT("Refine", "ConfirmChanceLabel", "재련 성공률"))), 0.f, 3.f);
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ConfirmBarBox"));
	BarBox->SetWidthOverride(360.f);
	BarBox->SetHeightOverride(24.f);
	UOverlay* BarLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ConfirmBarLayers"));
	BarBox->SetContent(BarLayers);
	ConfirmChanceBar = Refine_MakeBar(WidgetTree, TEXT("ConfirmChanceBar"), FLinearColor(0.35f, 0.62f, 0.78f, 1.f));
	if (UOverlaySlot* S = BarLayers->AddChildToOverlay(ConfirmChanceBar))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	ConfirmChanceText = Refine_MakeText(WidgetTree, TEXT("ConfirmChanceText"), 11, FLinearColor::White);
	ConfirmChanceText->SetShadowOffset(FVector2D(1.f, 1.f));
	ConfirmChanceText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	if (UOverlaySlot* S = BarLayers->AddChildToOverlay(ConfirmChanceText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	Refine_CenterSlot(Column->AddChildToVerticalBox(BarBox), 0.f, 8.f);

	// "라벨 ......... 값" 한 줄
	auto LabelRow = [this, Column](const TCHAR* Key, const FText& Label, const FLinearColor& LabelColor, const FLinearColor& ValueColor) -> UTextBlock*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("Confirm%sRow"), Key)));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 2.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("Confirm%sLabel"), Key)), 11, LabelColor, Label)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* Value = Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("Confirm%sValue"), Key)), 10, ValueColor);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Value)) S->SetVerticalAlignment(VAlign_Center);
		return Value;
	};
	ConfirmEnergyText = LabelRow(TEXT("Energy"), NSLOCTEXT("Refine", "ConfirmEnergy", "장인의 기운"), Refine_TextColor, Refine_GoldColor);
	ConfirmBreathMaxText = LabelRow(TEXT("Extra"), NSLOCTEXT("Refine", "ConfirmExtra", "추가 재료"), FLinearColor(0.55f, 0.75f, 0.95f, 1.f), Refine_TextColor);

	// 숨결 줄 — 아이콘(보유 수) | 이름 · 1개당 상승 / "N / 최대" [−] 슬라이더 [+] [최대]
	UBorder* BreathBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmBreathBack"));
	BreathBack->SetBrushColor(Refine_RowColor);
	BreathBack->SetPadding(FMargin(8.f, 6.f));
	ConfirmBreathRow = BreathBack;
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BreathBack)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UHorizontalBox* BreathLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmBreathLine"));
	BreathBack->SetContent(BreathLine);

	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ConfirmBreathIconBox"));
	IconBox->SetWidthOverride(52.f);
	IconBox->SetHeightOverride(52.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmBreathIconFrame"));
	IconFrame->SetBrushColor(FLinearColor(0.45f, 0.3f, 0.6f, 1.f));
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	UOverlay* IconLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ConfirmBreathIconLayers"));
	IconFrame->SetContent(IconLayers);
	ConfirmBreathIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ConfirmBreathIcon"));
	if (UOverlaySlot* S = IconLayers->AddChildToOverlay(ConfirmBreathIcon))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	ConfirmBreathOwnedText = Refine_MakeText(WidgetTree, TEXT("ConfirmBreathOwned"), 10, FLinearColor::White);
	ConfirmBreathOwnedText->SetShadowOffset(FVector2D(1.f, 1.f));
	ConfirmBreathOwnedText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
	if (UOverlaySlot* S = IconLayers->AddChildToOverlay(ConfirmBreathOwnedText))
	{
		S->SetHorizontalAlignment(HAlign_Right);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = BreathLine->AddChildToHorizontalBox(IconBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	UVerticalBox* BreathTexts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmBreathTexts"));
	if (UHorizontalBoxSlot* S = BreathLine->AddChildToHorizontalBox(BreathTexts))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	UHorizontalBox* NameLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmBreathNameLine"));
	if (UVerticalBoxSlot* S = BreathTexts->AddChildToVerticalBox(NameLine)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	ConfirmBreathName = Refine_MakeText(WidgetTree, TEXT("ConfirmBreathName"), 11, Refine_NameColor);
	if (UHorizontalBoxSlot* S = NameLine->AddChildToHorizontalBox(ConfirmBreathName))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	ConfirmBreathPerText = Refine_MakeText(WidgetTree, TEXT("ConfirmBreathPer"), 10, Refine_TextColor);
	if (UHorizontalBoxSlot* S = NameLine->AddChildToHorizontalBox(ConfirmBreathPerText)) S->SetVerticalAlignment(VAlign_Center);

	UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmBreathControls"));
	BreathTexts->AddChildToVerticalBox(Controls);
	USizeBox* CountBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ConfirmBreathCountBox"));
	CountBox->SetWidthOverride(64.f);
	UBorder* CountBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmBreathCountBack"));
	CountBack->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.035f, 1.f));
	CountBack->SetPadding(FMargin(4.f, 2.f));
	CountBack->SetHorizontalAlignment(HAlign_Center);
	CountBox->SetContent(CountBack);
	ConfirmBreathCountText = Refine_MakeText(WidgetTree, TEXT("ConfirmBreathCount"), 11, FLinearColor::White);
	CountBack->SetContent(ConfirmBreathCountText);
	if (UHorizontalBoxSlot* S = Controls->AddChildToHorizontalBox(CountBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	const FLinearColor SmallButton(0.14f, 0.15f, 0.19f, 1.f);
	UButton* Minus = nullptr;
	if (UHorizontalBoxSlot* S = Controls->AddChildToHorizontalBox(Refine_MakeButton(WidgetTree, TEXT("ConfirmBreathMinus"), FText::FromString(TEXT("-")), 22.f, 22.f, 11, SmallButton, Minus)))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	Minus->OnClicked.AddDynamic(this, &URefineWidget::HandleBreathMinus);

	ConfirmBreathSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ConfirmBreathSlider"));
	ConfirmBreathSlider->SetMinValue(0.f);
	ConfirmBreathSlider->SetMaxValue(1.f);
	ConfirmBreathSlider->SetStepSize(1.f);
	ConfirmBreathSlider->MouseUsesStep = true;  // 끌면 1개 단위로 딱딱 걸리게 (setter 없음 — RebuildWidget 전에 넣어야 반영)
	ConfirmBreathSlider->SetSliderBarColor(FLinearColor(0.3f, 0.3f, 0.34f, 1.f));
	ConfirmBreathSlider->SetSliderHandleColor(Refine_GoldColor);
	ConfirmBreathSlider->OnValueChanged.AddDynamic(this, &URefineWidget::HandleBreathSlider);
	if (UHorizontalBoxSlot* S = Controls->AddChildToHorizontalBox(ConfirmBreathSlider))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(6.f, 0.f));
	}

	UButton* Plus = nullptr;
	if (UHorizontalBoxSlot* S = Controls->AddChildToHorizontalBox(Refine_MakeButton(WidgetTree, TEXT("ConfirmBreathPlus"), FText::FromString(TEXT("+")), 22.f, 22.f, 11, SmallButton, Plus)))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
	}
	Plus->OnClicked.AddDynamic(this, &URefineWidget::HandleBreathPlus);
	UButton* MaxButton = nullptr;
	if (UHorizontalBoxSlot* S = Controls->AddChildToHorizontalBox(Refine_MakeButton(WidgetTree, TEXT("ConfirmBreathMax"), NSLOCTEXT("Refine", "BreathMax", "최대"), 40.f, 22.f, 9, SmallButton, MaxButton)))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	MaxButton->OnClicked.AddDynamic(this, &URefineWidget::HandleBreathMax);

	ConfirmNoBreathText = Refine_MakeText(WidgetTree, TEXT("ConfirmNoBreath"), 10, Refine_DimTextColor, NSLOCTEXT("Refine", "NoBreath", "이 단계에는 넣을 수 있는 추가 재료가 없습니다."));
	Refine_CenterSlot(Column->AddChildToVerticalBox(ConfirmNoBreathText), 8.f, 4.f);

	// [장비 재련] (금테) [취소]
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmButtons"));
	Refine_CenterSlot(Column->AddChildToVerticalBox(Buttons), 16.f);
	UBorder* Gilded = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmButtonFrame"));
	Gilded->SetBrushColor(Refine_FrameSelected);
	Gilded->SetPadding(FMargin(2.f));
	UButton* Go = nullptr;
	UTextBlock* GoText = nullptr;
	Gilded->SetContent(Refine_MakeButton(WidgetTree, TEXT("ConfirmGo"), NSLOCTEXT("Refine", "Button", "장비 재련"),
		190.f, 38.f, 13, FLinearColor(0.12f, 0.13f, 0.16f, 1.f), Go, &GoText));
	ConfirmButton = Go;
	ConfirmButtonText = GoText;
	ConfirmButton->OnClicked.AddDynamic(this, &URefineWidget::HandleConfirmRefine);
	if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(Gilded))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	UButton* Cancel = nullptr;
	if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(Refine_MakeButton(WidgetTree, TEXT("ConfirmCancel"), NSLOCTEXT("Refine", "Cancel", "취소"),
		100.f, 34.f, 12, FLinearColor(0.12f, 0.13f, 0.16f, 1.f), Cancel)))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	Cancel->OnClicked.AddDynamic(this, &URefineWidget::HandleConfirmClose);
	return Dim;
}

UWidget* URefineWidget::BuildSuccessPopup()
{
	// 로아 [재련 성공] 창 — 아이콘(금테) · 이름(주황) · "N 단계"(초록) · "재련 성공" · 스탯 변화 줄 · 시도 횟수 · [확인]
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SuccessDim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Dim->SetHorizontalAlignment(HAlign_Center);
	Dim->SetVerticalAlignment(VAlign_Center);
	Dim->SetVisibility(ESlateVisibility::Visible);

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SuccessSize"));
	CardSize->SetWidthOverride(560.f);
	Dim->SetContent(CardSize);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SuccessCard"));
	Card->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.045f, 0.98f));
	Card->SetPadding(FMargin(24.f, 20.f, 24.f, 18.f));
	CardSize->SetContent(Card);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SuccessColumn"));
	Card->SetContent(Column);

	// 아이콘 — 푸른 빛 테두리 위 금테
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SuccessIconBox"));
	IconBox->SetWidthOverride(72.f);
	IconBox->SetHeightOverride(72.f);
	UBorder* Glow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SuccessIconGlow"));
	Glow->SetBrushColor(FLinearColor(0.3f, 0.75f, 1.f, 1.f));
	Glow->SetPadding(FMargin(3.f));
	IconBox->SetContent(Glow);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SuccessIconFrame"));
	IconFrame->SetBrushColor(Refine_FrameSelected);
	IconFrame->SetPadding(FMargin(1.f));
	Glow->SetContent(IconFrame);
	SuccessIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SuccessIcon"));
	IconFrame->SetContent(SuccessIcon);
	Refine_CenterSlot(Column->AddChildToVerticalBox(IconBox), 0.f, 10.f);

	SuccessName = Refine_MakeText(WidgetTree, TEXT("SuccessName"), 13, Refine_NameColor);
	Refine_CenterSlot(Column->AddChildToVerticalBox(SuccessName));
	SuccessLevel = Refine_MakeText(WidgetTree, TEXT("SuccessLevel"), 14, FLinearColor(0.65f, 0.95f, 0.35f, 1.f));
	Refine_CenterSlot(Column->AddChildToVerticalBox(SuccessLevel), 4.f, 2.f);

	// "재련 성공" + 아래 빛줄기
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeText(WidgetTree, TEXT("SuccessTitle"), 17, FLinearColor(0.75f, 0.92f, 1.f, 1.f),
		NSLOCTEXT("Refine", "SuccessTitle", "재련 성공"))));
	USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SuccessLineBox"));
	LineBox->SetWidthOverride(420.f);
	LineBox->SetHeightOverride(2.f);
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SuccessLine"));
	Line->SetBrushColor(FLinearColor(0.3f, 0.7f, 1.f, 0.9f));
	LineBox->SetContent(Line);
	Refine_CenterSlot(Column->AddChildToVerticalBox(LineBox), 4.f, 14.f);

	SuccessRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SuccessRows"));
	Refine_CenterSlot(Column->AddChildToVerticalBox(SuccessRows), 0.f, 18.f);

	SuccessTriesText = Refine_MakeText(WidgetTree, TEXT("SuccessTries"), 11, Refine_TextColor);
	SuccessTriesText->SetJustification(ETextJustify::Center);
	Refine_CenterSlot(Column->AddChildToVerticalBox(SuccessTriesText), 0.f, 10.f);

	UButton* Ok = nullptr;
	Refine_CenterSlot(Column->AddChildToVerticalBox(Refine_MakeButton(WidgetTree, TEXT("SuccessOk"), NSLOCTEXT("Refine", "SuccessOk", "확인"),
		190.f, 36.f, 12, FLinearColor(0.12f, 0.2f, 0.28f, 1.f), Ok)));
	Ok->OnClicked.AddDynamic(this, &URefineWidget::HandleSuccessClose);
	return Dim;
}

void URefineWidget::ShowSuccessPopup(const FEquipmentInstance& Before, int32 Tries, int32 PlannedTries, bool bMulti)
{
	const UEquipmentComponent* Equip = Equipment.Get();
	const FEquipmentInstance* After = Equip ? Equip->GetEquipped(AnimSlot) : nullptr;
	const FItemDefinition* Def = After ? LoAItems::Find(After->ItemId) : nullptr;
	if (!SuccessPopup || !Def) return;

	Refine_SetIcon(SuccessIcon, Def->Icon.LoadSynchronous(), FLinearColor(0.3f, 0.3f, 0.35f, 1.f));
	SuccessName->SetText(Def->DisplayName);
	SuccessLevel->SetText(FText::Format(NSLOCTEXT("Refine", "SuccessLevel", "{0} 단계"), FText::AsNumber(After->RefineLevel)));

	// 스탯 줄 — 이름 | 이전 | ››› | 이후 | ▲
	SuccessRows->ClearChildren();
	int32 RowIndex = 0;
	auto AddRow = [this, &RowIndex](const FString& Label, const FString& From, const FString& To)
	{
		const int32 i = RowIndex++;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("SuccessRow_%d"), i)));
		auto Cell = [this, Row, i](const TCHAR* Key, float Width, const FString& Text, const FLinearColor& Color, ETextJustify::Type Justify)
		{
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("SuccessRow_%d_%sBox"), i, Key)));
			Box->SetWidthOverride(Width);
			UTextBlock* Block = Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("SuccessRow_%d_%s"), i, Key)), 11, Color, FText::FromString(Text));
			Block->SetJustification(Justify);
			Box->SetContent(Block);
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Box)) S->SetVerticalAlignment(VAlign_Center);
		};
		Cell(TEXT("Label"), 110.f, Label, Refine_DimTextColor, ETextJustify::Center);
		Cell(TEXT("From"), 100.f, From, Refine_TextColor, ETextJustify::Center);
		Cell(TEXT("Arrow"), 50.f, TEXT("›››"), Refine_AnimColor, ETextJustify::Center);
		Cell(TEXT("To"), 100.f, To, FLinearColor(0.75f, 0.95f, 0.45f, 1.f), ETextJustify::Center);
		Cell(TEXT("Up"), 30.f, TEXT("▲"), Refine_EnoughColor, ETextJustify::Center);
		if (UVerticalBoxSlot* S = SuccessRows->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 4.f));
	};
	auto Plus = [](double V) { return FString::Printf(TEXT("+%lld"), static_cast<int64>(V)); };

	AddRow(TEXT("아이템 레벨"), FString::FromInt(LoAItems::GetEquipmentItemLevel(Before)), FString::FromInt(LoAItems::GetEquipmentItemLevel(*After)));
	const FGearStats Old = UEquipmentComponent::GetEquipmentStats(Before);
	const FGearStats New = UEquipmentComponent::GetEquipmentStats(*After);
	if (Def->EquipSlot == EEquipSlot::Weapon)
	{
		AddRow(TEXT("무기 공격력"), Plus(Old.WeaponPower), Plus(New.WeaponPower));
	}
	else
	{
		AddRow(TEXT("물리 방어력"), Plus(Old.PhysDefense), Plus(New.PhysDefense));
		AddRow(TEXT("마법 방어력"), Plus(Old.MagicDefense), Plus(New.MagicDefense));
		AddRow(Refine_PrimaryStatName(Equip), Plus(Old.PrimaryStat), Plus(New.PrimaryStat));
		AddRow(TEXT("체력"), Plus(Old.Vitality), Plus(New.Vitality));
	}

	// [장비 재련 N회]일 때만 — 몇 번 만에 성공했는지, 남은 횟수의 재료는 쓰지 않음(반환)
	FString Tries_Text;
	if (bMulti)
	{
		Tries_Text = FString::Printf(TEXT("재련을 %d회 시도했습니다."), Tries);
		if (PlannedTries > Tries)
		{
			Tries_Text += FString::Printf(TEXT("\n남은 %d회분의 재료는 반환되었습니다."), PlannedTries - Tries);
		}
	}
	SuccessTriesText->SetText(FText::FromString(Tries_Text));
	SuccessTriesText->SetVisibility(bMulti ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SuccessPopup->SetVisibility(ESlateVisibility::Visible);
}

void URefineWidget::HandleSuccessClose()
{
	if (SuccessPopup) SuccessPopup->SetVisibility(ESlateVisibility::Collapsed);
}

// ═══ 재련 창 — 데이터 ════════════════════════════════════════════════════════

void URefineWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void URefineWidget::Bind(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory)
{
	Unbind();
	Equipment = InEquipment;
	Inventory = InInventory;
	if (InEquipment)
	{
		EquipmentChangedHandle = InEquipment->OnEquipmentChanged.AddUObject(this, &URefineWidget::HandleEquipmentChanged);
	}
	if (InInventory)
	{
		InventoryChangedHandle = InInventory->OnSlotChanged.AddUObject(this, &URefineWidget::HandleInventoryChanged);
		GoldChangedHandle = InInventory->OnGoldChanged.AddUObject(this, &URefineWidget::HandleCurrencyChanged);
		SillingChangedHandle = InInventory->OnSillingChanged.AddUObject(this, &URefineWidget::HandleCurrencyChanged);
	}
}

void URefineWidget::Unbind()
{
	if (UEquipmentComponent* Equip = Equipment.Get()) Equip->OnEquipmentChanged.Remove(EquipmentChangedHandle);
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->OnSlotChanged.Remove(InventoryChangedHandle);
		Inv->OnGoldChanged.Remove(GoldChangedHandle);
		Inv->OnSillingChanged.Remove(SillingChangedHandle);
	}
	EquipmentChangedHandle.Reset();
	InventoryChangedHandle.Reset();
	GoldChangedHandle.Reset();
	SillingChangedHandle.Reset();
	Equipment.Reset();
	Inventory.Reset();
}

void URefineWidget::OpenRefine(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory)
{
	if (!Panel) TakeWidget();
	Bind(InEquipment, InInventory);

	bAnimating = false;
	AnimElapsed = 0.f;
	if (AnimBar) AnimBar->SetPercent(0.f);
	SetResult(FText::GetEmpty(), Refine_GoldColor);
	if (GrowthPopup) GrowthPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (ConfirmPopup) ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (SuccessPopup) SuccessPopup->SetVisibility(ESlateVisibility::Collapsed);
	BreathUse = 0;
	StageListLevel = INDEX_NONE;

	// 처음엔 무기 — 없으면 착용 중인 첫 재련 장비
	SelectedSlot = EEquipSlot::Weapon;
	if (InEquipment && !InEquipment->GetEquipped(SelectedSlot))
	{
		for (int32 i = 0; i < LoAEquipRules::GearSlotCount; ++i)
		{
			if (InEquipment->GetEquipped(static_cast<EEquipSlot>(i)))
			{
				SelectedSlot = static_cast<EEquipSlot>(i);
				break;
			}
		}
	}
	Refresh();
}

void URefineWidget::CloseRefine()
{
	// 연출 중에 닫으면 굴리지 않는다 — 재료도 그대로
	bAnimating = false;
	WindowDrag.End();
	if (GrowthPopup) GrowthPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (ConfirmPopup) ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (SuccessPopup) SuccessPopup->SetVisibility(ESlateVisibility::Collapsed);
	Unbind();
}

bool URefineWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void URefineWidget::SelectPart(EEquipSlot PartSlot)
{
	if (bAnimating || PartSlot == SelectedSlot) return;
	SelectedSlot = PartSlot;
	BreathUse = 0;  // 부위마다 숨결 종류·최대 개수가 다르다
	if (ConfirmPopup) ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
	if (SuccessPopup) SuccessPopup->SetVisibility(ESlateVisibility::Collapsed);
	SetResult(FText::GetEmpty(), Refine_GoldColor);
	if (AnimBar) AnimBar->SetPercent(0.f);
	Refresh();
}

void URefineWidget::SetResult(const FText& Text, const FLinearColor& Color)
{
	if (!ResultText) return;
	ResultText->SetText(Text);
	ResultText->SetColorAndOpacity(FSlateColor(Color));
}

void URefineWidget::Refresh()
{
	if (!Panel) return;
	RefreshParts();
	RefreshDetail();
	RefreshGrowthPopup();
	RefreshConfirmPopup();
}

void URefineWidget::RefreshParts()
{
	const UEquipmentComponent* Equip = Equipment.Get();
	for (URefinePartRowWidget* Row : PartRows)
	{
		if (!Row) continue;
		const EEquipSlot PartSlot = Row->GetPartSlot();
		Row->SetPart(Equip ? Equip->GetEquipped(PartSlot) : nullptr, PartSlot == SelectedSlot);
	}
}

void URefineWidget::SetMaterialCell(FMaterialCell& Cell, UTexture2D* Texture, int64 Have, int64 Need, bool bVisible, bool bActive)
{
	if (!Cell.Root) return;
	Cell.Root->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible) return;
	Refine_SetIcon(Cell.Icon, Texture, FLinearColor(0.4f, 0.42f, 0.48f, 1.f));
	Cell.Icon->SetRenderOpacity(bActive ? 1.f : 0.35f);
	Cell.Ring->SetColorAndOpacity(bActive ? FLinearColor(0.75f, 0.6f, 0.32f, 1.f) : FLinearColor(0.22f, 0.22f, 0.25f, 1.f));
	Cell.Count->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"), *LoAItems::FormatWithCommas(Have), *LoAItems::FormatWithCommas(Need))));
	Cell.Count->SetColorAndOpacity(FSlateColor(!bActive ? Refine_DimTextColor : (Have >= Need ? Refine_EnoughColor : Refine_ShortColor)));
}

void URefineWidget::RefreshDetail()
{
	const UEquipmentComponent* Equip = Equipment.Get();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(SelectedSlot) : nullptr;
	const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
	RefreshStageList(Def ? Instance : nullptr);

	auto DisableAll = [this]()
	{
		for (FMaterialCell& Cell : MaterialCells) SetMaterialCell(Cell, nullptr, 0, 0, false);
		CostGoldText->SetText(FText::GetEmpty());
		HaveGoldText->SetText(FText::GetEmpty());
		ChanceText->SetText(FText::GetEmpty());
		WarningText->SetVisibility(ESlateVisibility::Collapsed);
		GrowthButton->SetIsEnabled(false);
		RefineButton->SetIsEnabled(false);
		RefineTenButton->SetIsEnabled(false);
	};

	if (!Def || !Inv)
	{
		Refine_SetIcon(DetailIcon, nullptr, FLinearColor(0.f, 0.f, 0.f, 0.f));
		DetailName->SetText(NSLOCTEXT("Refine", "NoEquip", "재련할 장비를 착용하세요."));
		LevelText->SetText(FText::GetEmpty());
		EnergyText->SetText(FText::GetEmpty());
		GrowthPercentText->SetText(FText::GetEmpty());
		DisableAll();
		return;
	}

	Refine_SetIcon(DetailIcon, Def->Icon.LoadSynchronous(), FLinearColor(0.3f, 0.3f, 0.35f, 1.f));
	DetailName->SetText(Def->DisplayName);

	const bool bWeapon = Def->EquipSlot == EEquipSlot::Weapon;
	const FRefineCost FullCost = LoARefine::GetCost(Instance->RefineLevel + 1, bWeapon);
	const FRefinePreview P = LoARefine::Preview(*Instance, 0);
	const bool bGrown = Instance->bGrowthDone;

	// 성장 원 — 완료면 주황으로 빛나고 100%
	GrowthPercentText->SetText(FText::FromString(bGrown ? TEXT("100%") : TEXT("0%")));
	GrowthRing->SetColorAndOpacity(bGrown ? FLinearColor(1.f, 0.72f, 0.3f, 1.f) : FLinearColor(0.3f, 0.3f, 0.33f, 1.f));
	GrowthCircle->SetColorAndOpacity(bGrown ? FLinearColor(0.85f, 0.4f, 0.08f, 1.f) : FLinearColor(0.06f, 0.065f, 0.08f, 1.f));

	if (!P.bValid)
	{
		LevelText->SetText(FText::Format(NSLOCTEXT("Refine", "LevelMax", "{0} 단계 (최대)"), FText::AsNumber(Instance->RefineLevel)));
		EnergyText->SetText(FText::GetEmpty());
		GrowthButtonText->SetText(NSLOCTEXT("Refine", "Grow", "성장"));
		DisableAll();
		return;
	}

	LevelText->SetText(FText::Format(NSLOCTEXT("Refine", "LevelStep", "{0} 단계  ›››  {1} 단계"), FText::AsNumber(P.CurrentLevel), FText::AsNumber(P.TargetLevel)));
	EnergyText->SetText(FText::Format(NSLOCTEXT("Refine", "Energy", "장인의 기운 {0}"), Refine_Percent(P.ArtisanEnergy)));

	GrowthButton->SetIsEnabled(!bGrown && !bAnimating);
	GrowthButtonText->SetText(bGrown ? NSLOCTEXT("Refine", "Grown", "성장 완료") : NSLOCTEXT("Refine", "Grow", "성장"));
	WarningText->SetVisibility(bGrown ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	ChanceText->SetText(P.bGuaranteed
		? NSLOCTEXT("Refine", "Guaranteed", "성공 확률 100% (장인의 기운)")
		: FText::Format(NSLOCTEXT("Refine", "ChanceNoBreath", "성공 확률 {0}  (기본 {1} + 실패 보정 {2}, 숨결은 재련할 때 선택)"),
			Refine_Percent(P.Chance), Refine_Percent(P.BaseChance), Refine_Percent(P.FailBonus)));

	// 재료 원 — 돌·돌파석
	const FItemDefinition* StoneDef = LoAItems::Find(FullCost.StoneId);
	const FItemDefinition* LeapDef = LoAItems::Find(LoAItems::LeapStone);
	SetMaterialCell(MaterialCells[0], StoneDef ? StoneDef->Icon.LoadSynchronous() : nullptr, Inv->CountItem(FullCost.StoneId), FullCost.StoneCount, true);
	SetMaterialCell(MaterialCells[1], LeapDef ? LeapDef->Icon.LoadSynchronous() : nullptr, Inv->CountItem(LoAItems::LeapStone), FullCost.LeapCount, FullCost.LeapCount > 0);

	CostGoldText->SetText(Refine_Number(P.Cost.Gold));
	HaveGoldText->SetText(Refine_Number(Inv->GetGold()));
	HaveGoldText->SetColorAndOpacity(FSlateColor(Inv->GetGold() >= P.Cost.Gold ? Refine_TextColor : Refine_ShortColor));

	const int32 Affordable = LoARefine::CountAffordableAttempts(*Inv, P.Cost);
	RefineButton->SetIsEnabled(bGrown && !bAnimating);
	// 로아처럼 재료 수에 따라 "장비 재련 N회" (2~10회) — 2번도 못 하면 꺼진 채 2회
	const int32 MultiCount = GetMultiCount(Affordable);
	RefineTenText->SetText(FText::Format(NSLOCTEXT("Refine", "MultiButton", "장비 재련 {0}회"), FText::AsNumber(MultiCount)));
	RefineTenButton->SetIsEnabled(bGrown && !bAnimating && Affordable >= 2);
	RefineTenButton->SetToolTipText(FText::Format(NSLOCTEXT("Refine", "TenTip", "성공하거나 재료가 떨어질 때까지 최대 {0}번 재련합니다.\n성공하면 남은 횟수의 재료는 쓰지 않습니다."),
		FText::AsNumber(MultiCount)));
}

void URefineWidget::RefreshStageList(const FEquipmentInstance* Instance)
{
	if (!StageList) return;
	const FGuid Id = Instance ? Instance->InstanceId : FGuid();
	const int32 Level = Instance ? Instance->RefineLevel : INDEX_NONE;
	if (Id == StageListInstance && Level == StageListLevel) return;
	StageListInstance = Id;
	StageListLevel = Level;

	StageList->ClearChildren();
	if (!Instance) return;

	const FItemDefinition* Def = LoAItems::Find(Instance->ItemId);
	const bool bWeapon = Def && Def->EquipSlot == EEquipSlot::Weapon;
	const FString PrimaryName = Refine_PrimaryStatName(Equipment.Get());
	UWidget* CurrentRow = nullptr;

	// 높은 단계가 위 — 현재 단계는 금빛 띠로 강조, 다음 단계는 살짝 밝게
	for (int32 L = LoAEquipRules::MaxRefineLevel; L >= 0; --L)
	{
		// 그 단계에 성공하면 늘어나는 양 (L-1 → L) — 누적값이 아니다. 로아 표라 부위·단계마다 다르다. 0단계는 늘어나는 게 없다
		FString Stat = TEXT("기본");
		if (L > 0)
		{
			FEquipmentInstance AtLevel = *Instance;
			AtLevel.RefineLevel = L;
			const FGearStats Now = UEquipmentComponent::GetEquipmentStats(AtLevel);
			AtLevel.RefineLevel = L - 1;
			const FGearStats Prev = UEquipmentComponent::GetEquipmentStats(AtLevel);
			auto Gain = [](double A, double B) { return LoAItems::FormatWithCommas(static_cast<int64>(A) - static_cast<int64>(B)); };
			Stat = bWeapon
				? FString::Printf(TEXT("무기 공격력 +%s"), *Gain(Now.WeaponPower, Prev.WeaponPower))
				: FString::Printf(TEXT("물리 방어력 +%s · 마법 방어력 +%s\n%s +%s · 체력 +%s"),
					*Gain(Now.PhysDefense, Prev.PhysDefense), *Gain(Now.MagicDefense, Prev.MagicDefense),
					*PrimaryName, *Gain(Now.PrimaryStat, Prev.PrimaryStat), *Gain(Now.Vitality, Prev.Vitality));
		}

		const bool bCurrent = L == Instance->RefineLevel;
		const bool bNext = L == Instance->RefineLevel + 1;

		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("Stage_%d"), L)));
		Row->SetBrushColor(bCurrent ? FLinearColor(0.85f, 0.65f, 0.3f, 1.f) : FLinearColor(0.f, 0.f, 0.f, 0.f));
		Row->SetPadding(FMargin(bCurrent ? 3.f : 0.f, 0.f, 0.f, 0.f));
		UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("StageInner_%d"), L)));
		Inner->SetBrushColor(bCurrent ? FLinearColor(0.12f, 0.1f, 0.07f, 1.f) : (bNext ? FLinearColor(0.07f, 0.08f, 0.1f, 1.f) : Refine_RowColor));
		Inner->SetPadding(FMargin(8.f, 9.f));
		Row->SetContent(Inner);

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("StageLine_%d"), L)));
		Inner->SetContent(Line);
		USizeBox* LevelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("StageLevelBox_%d"), L)));
		LevelBox->SetWidthOverride(44.f);
		UTextBlock* LevelLabel = Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("StageLevel_%d"), L)), 10,
			bCurrent ? Refine_GoldColor : Refine_DimTextColor, FText::FromString(FString::Printf(TEXT("%d\n단계"), L)));
		LevelLabel->SetJustification(ETextJustify::Center);
		LevelBox->SetContent(LevelLabel);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(LevelBox)) S->SetVerticalAlignment(VAlign_Center);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Refine_MakeText(WidgetTree, FName(*FString::Printf(TEXT("StageStat_%d"), L)), 10,
			bCurrent ? FLinearColor::White : Refine_TextColor, FText::FromString(Stat))))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}

		if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(StageList->AddChild(Row))) S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		if (bCurrent) CurrentRow = Row;
	}
	// 현재 단계가 보이게 (레이아웃 뒤에 처리됨)
	if (CurrentRow) StageList->ScrollWidgetIntoView(CurrentRow, false, EDescendantScrollDestination::Center);
}

void URefineWidget::RefreshGrowthPopup()
{
	if (!GrowthPopup || GrowthPopup->GetVisibility() == ESlateVisibility::Collapsed) return;
	const UEquipmentComponent* Equip = Equipment.Get();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(SelectedSlot) : nullptr;
	const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
	if (!Def || !Inv || Instance->bGrowthDone)
	{
		GrowthPopup->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FGrowthCost Cost = LoARefine::GetGrowthCost(Instance->RefineLevel + 1, Def->EquipSlot == EEquipSlot::Weapon);
	const FItemDefinition* FragmentDef = LoAItems::Find(LoAItems::FateFragment);
	const int64 HaveFragments = Inv->CountItem(LoAItems::FateFragment);
	SetMaterialCell(PopupFragmentCell, FragmentDef ? FragmentDef->Icon.LoadSynchronous() : nullptr, HaveFragments, Cost.Fragments, true);
	PopupFragmentCell.Count->SetVisibility(ESlateVisibility::Collapsed);

	PopupNeedText->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"), *LoAItems::FormatWithCommas(HaveFragments), *LoAItems::FormatWithCommas(Cost.Fragments))));
	PopupNeedText->SetColorAndOpacity(FSlateColor(HaveFragments >= Cost.Fragments ? Refine_EnoughColor : Refine_ShortColor));
	PopupCostText->SetText(Refine_Number(Cost.Silling));
	PopupHaveText->SetText(Refine_Number(Inv->GetSilling()));
	PopupHaveText->SetColorAndOpacity(FSlateColor(Inv->GetSilling() >= Cost.Silling ? Refine_EnoughColor : Refine_ShortColor));
	PopupGrowButton->SetIsEnabled(HaveFragments >= Cost.Fragments && Inv->GetSilling() >= Cost.Silling);
}

void URefineWidget::RefreshConfirmPopup()
{
	if (!ConfirmPopup || ConfirmPopup->GetVisibility() == ESlateVisibility::Collapsed) return;
	const UEquipmentComponent* Equip = Equipment.Get();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(SelectedSlot) : nullptr;
	if (!Instance || !Inv || !Instance->bGrowthDone)
	{
		ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 숨결 개수는 최대 개수·보유 수를 넘지 않게 (재련으로 숨결이 줄었을 수 있다)
	FRefinePreview P = LoARefine::Preview(*Instance, BreathUse);
	if (!P.bValid)
	{
		ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const int64 OwnedBreath = Inv->CountItem(P.Cost.BreathId);
	const int32 UseLimit = static_cast<int32>(FMath::Min<int64>(P.MaxBreath, OwnedBreath));
	if (BreathUse > UseLimit)
	{
		BreathUse = UseLimit;
		P = LoARefine::Preview(*Instance, BreathUse);
	}

	ConfirmLevelText->SetText(FText::Format(NSLOCTEXT("Refine", "LevelStep", "{0} 단계  ›››  {1} 단계"), FText::AsNumber(P.CurrentLevel), FText::AsNumber(P.TargetLevel)));
	ConfirmChanceBar->SetPercent(P.MaxChance > 0.f ? P.Chance / P.MaxChance : 0.f);
	ConfirmChanceText->SetText(FText::Format(NSLOCTEXT("Refine", "ConfirmChance", "{0} / {1}"), Refine_Percent(P.Chance), Refine_Percent(P.MaxChance)));
	ConfirmEnergyText->SetText(P.bGuaranteed
		? NSLOCTEXT("Refine", "ConfirmEnergyFull", "100.00%  (확정 성공)")
		: FText::Format(NSLOCTEXT("Refine", "ConfirmEnergyGain", "{0}  (실패 시 +{1})"), Refine_Percent(P.ArtisanEnergy), Refine_Percent(P.Chance * LoARefine::ArtisanEnergyRate)));

	const bool bHasBreath = P.MaxBreath > 0;
	ConfirmBreathRow->SetVisibility(bHasBreath ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ConfirmNoBreathText->SetVisibility(bHasBreath ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ConfirmBreathMaxText->SetText(bHasBreath
		? FText::Format(NSLOCTEXT("Refine", "ConfirmExtraMax", "(최대 {0} 상승)"), Refine_Percent(P.BreathBonusPerUnit * P.MaxBreath))
		: FText::GetEmpty());
	if (bHasBreath)
	{
		const FItemDefinition* BreathDef = LoAItems::Find(P.Cost.BreathId);
		Refine_SetIcon(ConfirmBreathIcon, BreathDef ? BreathDef->Icon.LoadSynchronous() : nullptr, FLinearColor(0.4f, 0.42f, 0.48f, 1.f));
		ConfirmBreathOwnedText->SetText(Refine_Number(OwnedBreath));
		ConfirmBreathName->SetText(BreathDef ? BreathDef->DisplayName : FText::FromName(P.Cost.BreathId));
		// 용암 = 주황, 빙하 = 하늘색
		ConfirmBreathName->SetColorAndOpacity(FSlateColor(P.Cost.BreathId == LoAItems::LavaBreath ? Refine_NameColor : FLinearColor(0.45f, 0.8f, 1.f, 1.f)));
		ConfirmBreathPerText->SetText(FText::Format(NSLOCTEXT("Refine", "BreathPer", "(1개당 {0} 상승)"), Refine_Percent(P.BreathBonusPerUnit)));
		ConfirmBreathCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), BreathUse, P.MaxBreath)));
		ConfirmBreathSlider->SetMaxValue(static_cast<float>(P.MaxBreath));
		ConfirmBreathSlider->SetValue(static_cast<float>(BreathUse));
		ConfirmBreathSlider->SetToolTipText(OwnedBreath < P.MaxBreath
			? FText::Format(NSLOCTEXT("Refine", "BreathOwnedTip", "보유 {0}개 — 그 이상은 넣을 수 없습니다."), FText::AsNumber(OwnedBreath))
			: FText::GetEmpty());
	}

	const int32 Affordable = LoARefine::CountAffordableAttempts(*Inv, P.Cost);
	// N회는 숨결 개수까지 넣은 재료 기준 — 숨결을 늘리면 N이 줄 수 있다
	ConfirmMultiCount = GetMultiCount(Affordable);
	ConfirmButton->SetIsEnabled(!bAnimating && Affordable >= (bConfirmMulti ? 2 : 1));
	ConfirmButtonText->SetText(bConfirmMulti
		? FText::Format(NSLOCTEXT("Refine", "MultiButton", "장비 재련 {0}회"), FText::AsNumber(ConfirmMultiCount))
		: NSLOCTEXT("Refine", "Button", "장비 재련"));
	ConfirmButton->SetToolTipText(bConfirmMulti
		? FText::Format(NSLOCTEXT("Refine", "ConfirmTenTip", "성공하거나 재료가 떨어질 때까지 최대 {0}번 — 성공하면 남은 횟수의 재료는 쓰지 않습니다"),
			FText::AsNumber(ConfirmMultiCount))
		: FText::GetEmpty());
}

// ═══ 재련 창 — 입력 ══════════════════════════════════════════════════════════

void URefineWidget::HandleGrowthClicked()
{
	if (bAnimating || !GrowthPopup) return;
	GrowthPopup->SetVisibility(ESlateVisibility::Visible);
	RefreshGrowthPopup();
}

void URefineWidget::HandleGrowthClose()
{
	if (GrowthPopup) GrowthPopup->SetVisibility(ESlateVisibility::Collapsed);
}

void URefineWidget::HandleGrowthConfirm()
{
	UEquipmentComponent* Equip = Equipment.Get();
	if (!Equip) return;
	FText Reason;
	if (LoARefine::TryGrow(*Equip, SelectedSlot, Reason))
	{
		GrowthPopup->SetVisibility(ESlateVisibility::Collapsed);
		SetResult(NSLOCTEXT("Refine", "Grown", "장비 성장 완료 — 이제 재련할 수 있습니다."), Refine_GoldColor);
	}
	else if (!Reason.IsEmpty())
	{
		Notify(NSLOCTEXT("Refine", "GrowthTitle", "장비 성장"), Reason);
	}
	Refresh();
}

void URefineWidget::HandleRefineClicked()
{
	OpenConfirm(false);
}

void URefineWidget::HandleRefineTenClicked()
{
	OpenConfirm(true);
}

void URefineWidget::OpenConfirm(bool bMulti)
{
	if (bAnimating || !ConfirmPopup) return;
	const UEquipmentComponent* Equip = Equipment.Get();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(SelectedSlot) : nullptr;
	if (!Instance || !Inv) return;
	const FRefinePreview P = LoARefine::Preview(*Instance, 0);
	if (!P.bValid) return;
	if (!Instance->bGrowthDone)
	{
		Notify(NSLOCTEXT("Refine", "NoticeTitle", "재련"), NSLOCTEXT("Refine", "NeedGrowth", "장비 성장 완료 후, 재련할 수 있습니다."));
		return;
	}
	// 숨결 없이도 모자라면 창을 띄울 필요가 없다
	FText Reason;
	if (!LoARefine::HasMaterials(*Inv, P.Cost, Reason))
	{
		Notify(NSLOCTEXT("Refine", "NoticeTitle", "재련"), Reason);
		return;
	}
	bConfirmMulti = bMulti;
	ConfirmPopup->SetVisibility(ESlateVisibility::Visible);
	RefreshConfirmPopup();
}

void URefineWidget::SetBreathUse(int32 NewUse)
{
	const UEquipmentComponent* Equip = Equipment.Get();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(SelectedSlot) : nullptr;
	if (!Instance || !Inv || bAnimating) return;
	const FRefinePreview P = LoARefine::Preview(*Instance, 0);
	const int32 Limit = static_cast<int32>(FMath::Min<int64>(P.MaxBreath, Inv->CountItem(P.Cost.BreathId)));
	BreathUse = FMath::Clamp(NewUse, 0, Limit);
	RefreshConfirmPopup();
}

void URefineWidget::HandleBreathMinus()
{
	SetBreathUse(BreathUse - 1);
}

void URefineWidget::HandleBreathPlus()
{
	SetBreathUse(BreathUse + 1);
}

void URefineWidget::HandleBreathMax()
{
	SetBreathUse(MAX_int32);
}

void URefineWidget::HandleBreathSlider(float Value)
{
	// 보유 수보다 높게 끌면 SetBreathUse가 다시 보유 수로 되돌린다
	SetBreathUse(FMath::RoundToInt(Value));
}

void URefineWidget::HandleConfirmClose()
{
	if (ConfirmPopup) ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
}

void URefineWidget::HandleConfirmRefine()
{
	if (ConfirmPopup) ConfirmPopup->SetVisibility(ESlateVisibility::Collapsed);
	StartRefine(bConfirmMulti);
}

void URefineWidget::StartRefine(bool bMulti)
{
	if (bAnimating) return;
	const UEquipmentComponent* Equip = Equipment.Get();
	const UInventoryComponent* Inv = Inventory.Get();
	const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(SelectedSlot) : nullptr;
	if (!Instance || !Inv) return;

	// 굴리기 전에 막을 것은 미리 — 연출이 다 돈 뒤에 "재료 부족"이 뜨면 번거롭다
	const FRefinePreview P = LoARefine::Preview(*Instance, BreathUse);
	if (!P.bValid) return;
	if (!Instance->bGrowthDone)
	{
		Notify(NSLOCTEXT("Refine", "NoticeTitle", "재련"), NSLOCTEXT("Refine", "NeedGrowth", "장비 성장 완료 후, 재련할 수 있습니다."));
		return;
	}
	FText Reason;
	if (!LoARefine::HasMaterials(*Inv, P.Cost, Reason))
	{
		Notify(NSLOCTEXT("Refine", "NoticeTitle", "재련"), Reason);
		return;
	}

	bAnimating = true;
	bAnimMulti = bMulti;
	AnimMultiCount = ConfirmMultiCount;
	AnimBefore = *Instance;
	AnimElapsed = 0.f;
	AnimSlot = SelectedSlot;
	AnimBreathUse = P.Cost.BreathCount;
	AnimBar->SetFillColorAndOpacity(Refine_AnimColor);
	AnimBar->SetPercent(0.f);
	SetResult(bMulti
		? FText::Format(NSLOCTEXT("Refine", "WorkingMulti", "재련 중... (최대 {0}회)"), FText::AsNumber(AnimMultiCount))
		: NSLOCTEXT("Refine", "Working", "재련 중..."), Refine_DimTextColor);
	RefineButton->SetIsEnabled(false);
	RefineTenButton->SetIsEnabled(false);
	GrowthButton->SetIsEnabled(false);
	if (RefineAnimDuration <= 0.f) FinishRefineAnim();
}

void URefineWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bAnimating) return;
	AnimElapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(AnimElapsed / RefineAnimDuration, 0.f, 1.f);
	AnimBar->SetPercent(Alpha);
	// 성장 원이 망치질하듯 깜빡임
	GrowthCircle->SetColorAndOpacity(FLinearColor::LerpUsingHSV(FLinearColor(0.85f, 0.4f, 0.08f, 1.f), FLinearColor(1.f, 0.85f, 0.4f, 1.f),
		0.5f + 0.5f * FMath::Sin(AnimElapsed * 18.f)));
	if (AnimElapsed >= RefineAnimDuration) FinishRefineAnim();
}

void URefineWidget::FinishRefineAnim()
{
	bAnimating = false;
	UEquipmentComponent* Equip = Equipment.Get();
	if (!Equip)
	{
		Refresh();
		return;
	}
	AnimBar->SetPercent(1.f);

	if (bAnimMulti)
	{
		// 한 번에 다 떼지 않고 시도마다 차감하므로, 성공하면 남은 횟수의 재료는 그대로 남는다(= 로아의 "반환")
		const FRefineManyOutcome Out = LoARefine::TryRefineMany(*Equip, AnimSlot, AnimBreathUse, AnimMultiCount);
		if (Out.bSucceeded)
		{
			AnimBar->SetFillColorAndOpacity(Refine_GoldColor);
			SetResult(FText::Format(NSLOCTEXT("Refine", "TenSuccess", "재련 성공!  {0} 단계  ({1}회째)"), FText::AsNumber(Out.NewLevel), FText::AsNumber(Out.Tries)), Refine_GoldColor);
			ShowSuccessPopup(AnimBefore, Out.Tries, AnimMultiCount, true);
		}
		else if (Out.Tries > 0)
		{
			AnimBar->SetFillColorAndOpacity(Refine_FailColor);
			SetResult(FText::Format(Out.bStoppedByMaterials
					? NSLOCTEXT("Refine", "TenStopped", "{0}회 실패 — 재료가 떨어져 멈춤  (장인의 기운 +{1})")
					: NSLOCTEXT("Refine", "TenFail", "{0}회 모두 실패  (장인의 기운 +{1})"),
				FText::AsNumber(Out.Tries), Refine_Percent(Out.EnergyGained)), Refine_FailColor);
		}
		else
		{
			AnimBar->SetPercent(0.f);
			SetResult(FText::GetEmpty(), Refine_GoldColor);
			if (!Out.FailReason.IsEmpty()) Notify(NSLOCTEXT("Refine", "NoticeTitle", "재련"), Out.FailReason);
		}
		if (Out.bSucceeded) BreathUse = 0;
		Refresh();
		return;
	}

	const FRefineOutcome Outcome = LoARefine::TryRefine(*Equip, AnimSlot, AnimBreathUse);
	switch (Outcome.Result)
	{
	case ERefineResult::Success:
		AnimBar->SetFillColorAndOpacity(Refine_GoldColor);
		SetResult(FText::Format(NSLOCTEXT("Refine", "Success", "재련 성공!  {0} 단계"), FText::AsNumber(Outcome.NewLevel)), Refine_GoldColor);
		ShowSuccessPopup(AnimBefore, 1, 1, false);
		break;
	case ERefineResult::Fail:
		AnimBar->SetFillColorAndOpacity(Refine_FailColor);
		SetResult(FText::Format(NSLOCTEXT("Refine", "Fail", "재련 실패 — 장인의 기운 +{0}"), Refine_Percent(Outcome.EnergyGained)), Refine_FailColor);
		break;
	default:
		AnimBar->SetPercent(0.f);
		SetResult(FText::GetEmpty(), Refine_GoldColor);
		if (!Outcome.FailReason.IsEmpty()) Notify(NSLOCTEXT("Refine", "NoticeTitle", "재련"), Outcome.FailReason);
		break;
	}
	// 성공 시 숨결 사용은 풀어 둔다 — 다음 단계에서 모르고 계속 쓰지 않게
	if (Outcome.Result == ERefineResult::Success) BreathUse = 0;
	Refresh();
}

void URefineWidget::Notify(const FText& Title, const FText& Message)
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->ShowTimedNotice(Title, Message, 1.5f);
	}
}

// ── 창 끌기 / 클릭 차단 ──

FReply URefineWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply URefineWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URefineWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply URefineWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void URefineWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
