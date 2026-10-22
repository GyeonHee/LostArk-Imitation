#include "UI/InventorySlotWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/InventoryDragOperation.h"
#include "UI/SlotDragVisualWidget.h"
#include "Item/ItemTypes.h"
#include "Item/InventoryComponent.h"
#include "Item/EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	const FLinearColor InvSlotFrameNormal(0.20f, 0.20f, 0.24f, 1.f);
	const FLinearColor InvSlotFrameHover(0.80f, 0.68f, 0.35f, 1.f);
	const FLinearColor InvSlotFrameLocked(0.15f, 0.50f, 1.00f, 1.f);
	const FLinearColor InvSlotBackground(0.06f, 0.06f, 0.08f, 0.95f);
}

void UInventorySlotWidget::Setup(UInventoryWidget* InOwner, int32 InIndex, float InSize)
{
	Owner = InOwner;
	SlotIndex = InIndex;
	SlotSize = InSize;
}

TSharedRef<SWidget> UInventorySlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// SizeBox → Frame(테두리 색) → Back(어두운 바탕) → Overlay[아이콘, 개수]
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotBox"));
		Box->SetWidthOverride(SlotSize);
		Box->SetHeightOverride(SlotSize);
		WidgetTree->RootWidget = Box;

		Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Box->SetContent(Frame);

		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Back"));
		Back->SetBrushColor(InvSlotBackground);
		Back->SetPadding(FMargin(0.f));
		Frame->SetContent(Back);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SlotOverlay"));
		Back->SetContent(Overlay);

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(IconImage))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(2.f));
		}

		CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Count"));
		FSlateFontInfo Font = CountText->GetFont();
		Font.Size = 9;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.95f);
		CountText->SetFont(Font);
		CountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CountText->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Overlay->AddChildToOverlay(CountText))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		}

		ApplyFrame();
	}

	// UserWidget 기본값(SelfHitTestInvisible)이면 칸 자체가 마우스·드랍을 못 받는다
	SetVisibility(ESlateVisibility::Visible);

	return Super::RebuildWidget();
}

void UInventorySlotWidget::Refresh(const FInventorySlot& InSlot)
{
	if (!IconImage) TakeWidget();
	if (!IconImage || !CountText) return;

	const FItemDefinition* Def = InSlot.IsEmpty() ? nullptr : LoAItems::Find(InSlot.ItemId);
	bHasItem = Def != nullptr;
	bLocked = bHasItem && InSlot.bLocked;

	IconImage->SetRenderOpacity(1.f);
	if (bHasItem)
	{
		if (UTexture2D* Texture = Def->Icon.LoadSynchronous())
		{
			if (IconImage->GetBrush().GetResourceObject() != Texture)
			{
				IconImage->SetBrushResourceObject(Texture);
			}
		}
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 장비 — 개수 대신 재련 단계(+10), 툴팁은 아이템 레벨·스탯과 착용 중 장비 대비 차이
		const UInventoryComponent* Inventory = Owner.IsValid() ? Owner->GetInventory() : nullptr;
		const FEquipmentInstance* Equipment = (InSlot.IsEquipment() && Inventory) ? Inventory->FindEquipment(InSlot.EquipmentId) : nullptr;

		FText Tooltip = Def->DisplayName;
		if (Equipment)
		{
			CountText->SetText(FText::FromString(LoAItems::GetEquipmentBadge(*Equipment)));
			CountText->SetVisibility(ESlateVisibility::HitTestInvisible);

			const APawn* Pawn = GetOwningPlayerPawn();
			const UEquipmentComponent* Worn = Pawn ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;
			Tooltip = UEquipmentComponent::DescribeEquipment(*Equipment, Worn ? Worn->GetEquipped(Def->EquipSlot) : nullptr);
		}
		else
		{
			// 1개짜리는 개수 표시 안 함 (로아와 같이)
			CountText->SetText(FText::AsNumber(InSlot.Count));
			CountText->SetVisibility(InSlot.Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}

		if (bLocked)
		{
			Tooltip = FText::Format(NSLOCTEXT("Inventory", "LockedTooltip", "{0} (잠금)"), Tooltip);
		}
		SetToolTipText(Tooltip);
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Hidden);
		CountText->SetVisibility(ESlateVisibility::Collapsed);
		SetToolTipText(FText::GetEmpty());
	}

	ApplyFrame();
}

void UInventorySlotWidget::ApplyFrame()
{
	if (!Frame) return;
	// 잠금 파랑이 가장 우선 — 호버 중이어도 잠긴 칸은 파랗게
	const FLinearColor Color = bLocked ? InvSlotFrameLocked : (bHovered ? InvSlotFrameHover : InvSlotFrameNormal);
	Frame->SetBrushColor(Color);
	Frame->SetPadding(FMargin(bLocked ? 2.f : 1.f));
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();

	if (Button == EKeys::RightMouseButton && InMouseEvent.IsControlDown())
	{
		if (Owner.IsValid()) Owner->HandleSlotCtrlRightClick(SlotIndex);
		return FReply::Handled();
	}

	// 우클릭 — 상점이 열려 있으면 판매 목록에 올림 (Shift면 묶음 판매 수량 선택)
	if (Button == EKeys::RightMouseButton && bHasItem)
	{
		if (Owner.IsValid()) Owner->HandleSlotRightClick(SlotIndex, InMouseEvent.IsShiftDown());
		return FReply::Handled();
	}

	if (Button == EKeys::LeftMouseButton && bHasItem)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	// 빈 칸·다른 버튼도 먹는다 — 창 안 클릭이 캐릭터 이동으로 새지 않게
	return FReply::Handled();
}

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빠르게 두 번 누르면 두 번째는 MouseButtonDown이 아니라 이 이벤트로 온다 — 안 먹으면 게임(클릭 이동)으로 샌다.
	// 첫 클릭과 똑같이 처리해 두 번째 클릭으로도 잠금·드래그가 된다
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!bHasItem || !IconImage) return;

	UInventoryDragOperation* Operation = NewObject<UInventoryDragOperation>(this);
	Operation->SourceIndex = SlotIndex;
	Operation->SourceInventory = Owner;
	Operation->Pivot = EDragPivot::CenterCenter;

	if (USlotDragVisualWidget* Visual = CreateWidget<USlotDragVisualWidget>(GetOwningPlayer(), USlotDragVisualWidget::StaticClass()))
	{
		Visual->IconSize = SlotSize;
		Visual->SetIconBrush(IconImage->GetBrush());
		Operation->DefaultDragVisual = Visual;
	}

	// 끌고 있는 동안 원래 칸은 흐리게 — 놓거나 취소하면 Refresh가 되돌린다
	IconImage->SetRenderOpacity(0.35f);
	OutOperation = Operation;
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bHovered = false;
	ApplyFrame();

	if (const UInventoryDragOperation* Operation = Cast<UInventoryDragOperation>(InOperation))
	{
		if (Owner.IsValid()) Owner->HandleSlotDrop(Operation->SourceIndex, SlotIndex);
		return true;
	}
	return false;
}

void UInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bHovered = true;
	ApplyFrame();
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	bHovered = false;
	ApplyFrame();
}

void UInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyFrame();
}

void UInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyFrame();
}
