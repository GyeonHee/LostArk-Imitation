#include "UI/ResourceBarWidget.h"
#include "LoACharacter.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	UTextBlock* MakeResourceText(UWidgetTree* Tree, const TCHAR* Name, FLinearColor Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = 11;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	// 아이콘(텍스처가 없으면 대체 색 네모) + 숫자 한 묶음. 앞 묶음과는 넓게 띄운다
	UTextBlock* AddResourceEntry(UWidgetTree* Tree, UHorizontalBox* Row, const TCHAR* Name, const TSoftObjectPtr<UTexture2D>& Icon,
		FLinearColor FallbackColor, FLinearColor TextColor)
	{
		USizeBox* IconBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(FString(Name) + TEXT("IconBox")));
		IconBox->SetWidthOverride(18.f);
		IconBox->SetHeightOverride(18.f);
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), *(FString(Name) + TEXT("Icon")));
		if (UTexture2D* Texture = Icon.LoadSynchronous())
		{
			Image->SetBrushFromTexture(Texture);
		}
		else
		{
			Image->SetColorAndOpacity(FallbackColor);
		}
		IconBox->SetContent(Image);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconBox))
		{
			S->SetPadding(FMargin(18.f, 0.f, 4.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* Text = MakeResourceText(Tree, *(FString(Name) + TEXT("Text")), TextColor);
		Text->SetText(FText::FromString(TEXT("0")));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Text))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		return Text;
	}
}

TSharedRef<SWidget> UResourceBarWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResourceRoot"));
		WidgetTree->RootWidget = Root;

		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResourceBackground"));
		Background->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.72f));
		Background->SetPadding(FMargin(10.f, 4.f, 12.f, 4.f));
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Background))
		{
			S->SetAnchors(FAnchors(0.f, 0.f));
			S->SetPosition(ScreenOffset);
			S->SetAutoSize(true);
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResourceRow"));
		Background->SetContent(Row);

		ClockText = MakeResourceText(WidgetTree, TEXT("ClockText"), FLinearColor(0.9f, 0.9f, 0.9f, 1.f));
		ClockText->SetToolTipText(NSLOCTEXT("ResourceBar", "Clock", "현재 시간"));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ClockText))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		SillingText = AddResourceEntry(WidgetTree, Row, TEXT("Silling"), SillingIcon, FLinearColor(0.75f, 0.78f, 0.82f, 1.f), FLinearColor(0.88f, 0.89f, 0.92f));
		GoldText = AddResourceEntry(WidgetTree, Row, TEXT("Gold"), GoldIcon, FLinearColor(0.95f, 0.72f, 0.2f, 1.f), FLinearColor(1.f, 0.85f, 0.45f));
		FragmentText = AddResourceEntry(WidgetTree, Row, TEXT("Fragment"), FragmentIcon, FLinearColor(0.95f, 0.5f, 0.15f, 1.f), FLinearColor(1.f, 0.72f, 0.4f));
	}

	// 클릭 이동 게임이라 마우스 입력을 먹으면 안 된다 (HUD 위젯 규칙)
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UResourceBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 빙의가 늦거나 폰이 바뀌면 지금 폰의 인벤토리로 다시 묶는다
	const ALoACharacter* Char = Cast<ALoACharacter>(GetOwningPlayerPawn());
	UInventoryComponent* Current = Char ? Char->Inventory.Get() : nullptr;
	if (Current != Inventory.Get())
	{
		BindInventory(Current);
	}

	RefreshClock();
}

void UResourceBarWidget::NativeDestruct()
{
	BindInventory(nullptr);
	Super::NativeDestruct();
}

void UResourceBarWidget::BindInventory(UInventoryComponent* InInventory)
{
	if (UInventoryComponent* Old = Inventory.Get())
	{
		Old->OnSillingChanged.Remove(SillingHandle);
		Old->OnGoldChanged.Remove(GoldHandle);
		Old->OnSlotChanged.Remove(SlotHandle);
	}
	SillingHandle.Reset();
	GoldHandle.Reset();
	SlotHandle.Reset();
	Inventory = InInventory;

	if (InInventory)
	{
		SillingHandle = InInventory->OnSillingChanged.AddUObject(this, &UResourceBarWidget::HandleSillingChanged);
		GoldHandle = InInventory->OnGoldChanged.AddUObject(this, &UResourceBarWidget::HandleGoldChanged);
		SlotHandle = InInventory->OnSlotChanged.AddUObject(this, &UResourceBarWidget::HandleSlotChanged);
	}
	HandleSillingChanged(InInventory ? InInventory->GetSilling() : 0);
	HandleGoldChanged(InInventory ? InInventory->GetGold() : 0);
	HandleSlotChanged(INDEX_NONE);
}

void UResourceBarWidget::HandleSillingChanged(int64 NewAmount)
{
	if (SillingText)
	{
		SillingText->SetText(FText::FromString(LoAItems::FormatWithCommas(NewAmount)));
	}
}

void UResourceBarWidget::HandleGoldChanged(int64 NewAmount)
{
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(LoAItems::FormatWithCommas(NewAmount)));
	}
}

void UResourceBarWidget::HandleSlotChanged(int32 /*Index*/)
{
	// 파편은 칸에 들어 있는 아이템이라 칸이 바뀔 때마다 다시 센다 (100칸 훑기 — 칸이 바뀔 때만이라 충분히 싸다)
	if (FragmentText)
	{
		const UInventoryComponent* Inv = Inventory.Get();
		FragmentText->SetText(FText::FromString(LoAItems::FormatWithCommas(Inv ? Inv->CountItem(LoAItems::FateFragment) : 0)));
	}
}

void UResourceBarWidget::RefreshClock()
{
	const FDateTime Now = FDateTime::Now();
	const int32 MinuteOfDay = Now.GetHour() * 60 + Now.GetMinute();
	if (MinuteOfDay == ShownMinuteOfDay || !ClockText) return;

	ShownMinuteOfDay = MinuteOfDay;
	ClockText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Now.GetHour(), Now.GetMinute())));
}
