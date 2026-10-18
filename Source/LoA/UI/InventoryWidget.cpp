#include "UI/InventoryWidget.h"
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryDragOperation.h"
#include "Item/InventoryComponent.h"
#include "LoAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// 배경이 투명하고 올리면 살짝 밝아지는 헤더 버튼
	void MakeFlatButtonStyle(UButton* Button)
	{
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.3f));
		Style.NormalPadding = FMargin(4.f);
		Style.PressedPadding = FMargin(4.f);
		Button->SetStyle(Style);
	}
}

TSharedRef<SWidget> UInventoryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryRoot"));
		WidgetTree->RootWidget = Root;

		// 창 — 화면 오른쪽 가운데
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryPanel"));
		Panel->SetBrushColor(FLinearColor(0.035f, 0.04f, 0.055f, 0.94f));
		Panel->SetPadding(FMargin(8.f, 4.f, 8.f, 8.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
		}
		ResetPanelPlacement();

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryColumn"));
		Panel->SetContent(Column);

		// ── 헤더: 가운데 제목, 오른쪽 [정렬][닫기] ──
		HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HeaderBox"));
		HeaderBox->SetHeightOverride(30.f);
		Column->AddChildToVerticalBox(HeaderBox);

		UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Header"));
		HeaderBox->SetContent(Header);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetText(NSLOCTEXT("Inventory", "Title", "소지품"));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 13;
		Title->SetFont(TitleFont);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.87f, 0.72f)));
		if (UOverlaySlot* S = Header->AddChildToOverlay(Title))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderButtons"));
		if (UOverlaySlot* S = Header->AddChildToOverlay(Buttons))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 정렬 버튼 — 네모 4개 아이콘 (2x2 작은 사각형, 에셋 없이)
		UButton* SortButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SortButton"));
		MakeFlatButtonStyle(SortButton);
		SortButton->SetToolTipText(NSLOCTEXT("Inventory", "SortTooltip", "정렬"));
		SortButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleSortClicked);
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SortIconBox"));
			IconBox->SetWidthOverride(16.f);
			IconBox->SetHeightOverride(16.f);
			SortButton->SetContent(IconBox);

			UUniformGridPanel* IconGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SortIconGrid"));
			IconGrid->SetSlotPadding(FMargin(1.f));
			IconBox->SetContent(IconGrid);
			for (int32 i = 0; i < 4; ++i)
			{
				UBorder* Square = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("SortIconSquare_%d"), i)));
				Square->SetBrushColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
				Square->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UUniformGridSlot* GS = IconGrid->AddChildToUniformGrid(Square, i / 2, i % 2))
				{
					GS->SetHorizontalAlignment(HAlign_Fill);
					GS->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(SortButton))
		{
			S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		MakeFlatButtonStyle(CloseButton);
		CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleCloseClicked);
		{
			UTextBlock* X = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseText"));
			X->SetText(FText::FromString(TEXT("X")));
			FSlateFontInfo XFont = X->GetFont();
			XFont.Size = 12;
			X->SetFont(XFont);
			X->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
			CloseButton->SetContent(X);
		}
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(CloseButton))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		// 구분선
		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Divider"));
		Divider->SetBrushColor(FLinearColor(0.35f, 0.32f, 0.25f, 0.8f));
		Divider->SetPadding(FMargin(0.f, 0.5f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Divider))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		// ── 10x10 칸 ──
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("SlotGrid"));
		Grid->SetSlotPadding(FMargin(SlotGap * 0.5f));
		Column->AddChildToVerticalBox(Grid);

		SlotWidgets.Reset();
		for (int32 i = 0; i < UInventoryComponent::Capacity; ++i)
		{
			UInventorySlotWidget* SlotWidget = WidgetTree->ConstructWidget<UInventorySlotWidget>(UInventorySlotWidget::StaticClass(), FName(*FString::Printf(TEXT("InvSlot_%d"), i)));
			SlotWidget->Setup(this, i, SlotSize);
			Grid->AddChildToUniformGrid(SlotWidget, i / UInventoryComponent::Columns, i % UInventoryComponent::Columns);
			SlotWidgets.Add(SlotWidget);
		}

		// ── 하단 화폐 줄: [은화] 100,000,000 (오른쪽 정렬, 어두운 띠) ──
		UBorder* CurrencyBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CurrencyBar"));
		CurrencyBar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		CurrencyBar->SetPadding(FMargin(8.f, 3.f));
		CurrencyBar->SetHorizontalAlignment(HAlign_Right);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(CurrencyBar))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		UHorizontalBox* CurrencyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CurrencyRow"));
		CurrencyBar->SetContent(CurrencyRow);

		USizeBox* CoinBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SillingIconBox"));
		CoinBox->SetWidthOverride(18.f);
		CoinBox->SetHeightOverride(18.f);
		UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SillingIcon"));
		if (UTexture2D* CoinTexture = SillingIcon.LoadSynchronous())
		{
			Coin->SetBrushFromTexture(CoinTexture);
		}
		else
		{
			Coin->SetColorAndOpacity(FLinearColor(0.75f, 0.77f, 0.82f, 1.f));
		}
		CoinBox->SetContent(Coin);
		if (UHorizontalBoxSlot* S = CurrencyRow->AddChildToHorizontalBox(CoinBox))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
		}

		SillingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SillingText"));
		FSlateFontInfo SillingFont = SillingText->GetFont();
		SillingFont.Size = 11;
		SillingText->SetFont(SillingFont);
		SillingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.89f, 0.92f)));
		SillingText->SetToolTipText(NSLOCTEXT("Inventory", "SillingTooltip", "실링"));
		if (UHorizontalBoxSlot* S = CurrencyRow->AddChildToHorizontalBox(SillingText))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 루트는 통과 — 창(Panel) 밖은 게임 클릭으로
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	RefreshAll();
	HandleSillingChanged(Inventory.IsValid() ? Inventory->GetSilling() : 0);
	return Result;
}

void UInventoryWidget::NativeDestruct()
{
	SetInventory(nullptr);
	Super::NativeDestruct();
}

void UInventoryWidget::SetInventory(UInventoryComponent* InInventory)
{
	if (Inventory.Get() == InInventory) return;

	if (UInventoryComponent* Old = Inventory.Get())
	{
		Old->OnSlotChanged.Remove(InventoryChangedHandle);
		Old->OnSillingChanged.Remove(SillingChangedHandle);
	}
	InventoryChangedHandle.Reset();
	SillingChangedHandle.Reset();
	Inventory = InInventory;

	if (InInventory)
	{
		InventoryChangedHandle = InInventory->OnSlotChanged.AddUObject(this, &UInventoryWidget::HandleInventoryChanged);
		SillingChangedHandle = InInventory->OnSillingChanged.AddUObject(this, &UInventoryWidget::HandleSillingChanged);
	}
	RefreshAll();
	HandleSillingChanged(InInventory ? InInventory->GetSilling() : 0);
}

void UInventoryWidget::HandleSillingChanged(int64 NewAmount)
{
	if (SillingText)
	{
		SillingText->SetText(FText::FromString(LoAItems::FormatWithCommas(NewAmount)));
	}
}

void UInventoryWidget::HandleInventoryChanged(int32 Index)
{
	if (Index == INDEX_NONE)
	{
		RefreshAll();
	}
	else
	{
		RefreshSlot(Index);
	}
}

void UInventoryWidget::RefreshSlot(int32 Index)
{
	if (!SlotWidgets.IsValidIndex(Index) || !SlotWidgets[Index]) return;
	const UInventoryComponent* Inv = Inventory.Get();
	SlotWidgets[Index]->Refresh(Inv ? Inv->GetSlot(Index) : FInventorySlot());
}

void UInventoryWidget::RefreshAll()
{
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		RefreshSlot(i);
	}
}

bool UInventoryWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UInventoryWidget::HandleSlotDrop(int32 From, int32 To)
{
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->MoveOrSwap(From, To);
	}
	// 같은 칸에 놓은 경우 등 변화가 없어도 흐려진 원래 칸은 되돌린다
	RefreshSlot(From);
}

void UInventoryWidget::HandleSlotRightClick(int32 Index, bool bShift)
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->HandleInventorySlotRightClick(Index, bShift);
	}
}

void UInventoryWidget::HandleSlotCtrlRightClick(int32 Index)
{
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->ToggleLock(Index);
	}
}

void UInventoryWidget::HandleDragCancelled(int32 SourceIndex)
{
	RefreshSlot(SourceIndex);

	// 창 밖에 놓음 — HUD 배틀아이템 칸 위면 등록
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->HandleInventoryDropOutside(SourceIndex);
	}
}

void UInventoryWidget::HandleSortClicked()
{
	if (UInventoryComponent* Inv = Inventory.Get())
	{
		Inv->SortItems();
	}
}

void UInventoryWidget::HandleCloseClicked()
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->SetInventoryOpen(false);
	}
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 헤더를 좌클릭 — 창 끌기 시작
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// 창 안 빈 곳 클릭도 먹는다 (루트가 SelfHitTestInvisible이라 창 밖 클릭은 여기까지 안 옴)
	return FReply::Handled();
}

FReply UInventoryWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 두 번째 클릭은 DoubleClick으로 온다 — 안 먹으면 창 뒤의 맵으로 클릭 이동이 샌다
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	// 창 위에서 뗀 버튼도 게임으로 넘기지 않는다
	return FReply::Handled();
}

void UInventoryWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UInventoryWidget::SetPanelPlacement(FVector2D Anchor, FVector2D Alignment, FVector2D Position)
{
	if (!Panel) TakeWidget();
	if (UCanvasPanelSlot* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr)
	{
		PanelSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		PanelSlot->SetAlignment(Alignment);
		PanelSlot->SetPosition(Position);
	}
}

void UInventoryWidget::ResetPanelPlacement()
{
	SetPanelPlacement(FVector2D(1.f, 0.5f), FVector2D(1.f, 0.5f), FVector2D(-30.f, -20.f));
}

bool UInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// 칸 사이 틈·헤더에 놓음 — 아무것도 안 하고 원래 칸만 되돌린다
	if (const UInventoryDragOperation* Operation = Cast<UInventoryDragOperation>(InOperation))
	{
		RefreshSlot(Operation->SourceIndex);
		return true;
	}
	return false;
}
