#include "UI/EngravingSetupWidget.h"
#include "UI/BuildUIHelpers.h"
#include "Stat/EngravingComponent.h"
#include "Item/InventoryComponent.h"
#include "LoAPlayerController.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

using namespace LoABuildUI;

namespace
{
	constexpr int32 Setup_Columns = 5;
	constexpr float Setup_ListWidth = 540.f;
	constexpr float Setup_Board = 380.f;
	constexpr float Setup_BodyHeight = 480.f;
}

TSharedRef<SWidget> UEngravingSetupWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EngravingSetupRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EngravingSetupPanel"));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(14.f, 4.f, 14.f, 12.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		// 헤더 — 제목 + X (끌면 창 이동)
		HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HeaderBox->SetHeightOverride(34.f);
		Column->AddChildToVerticalBox(HeaderBox);
		UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		HeaderBox->SetContent(Header);
		if (UOverlaySlot* S = Header->AddChildToOverlay(MakeText(WidgetTree, TEXT("각인 효과"), 15, FLinearColor(0.92f, 0.87f, 0.72f))))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}
		CloseButton = MakeButton(WidgetTree, TEXT("X"), 12, FLinearColor(0.f, 0.f, 0.f, 0.f));
		if (UOverlaySlot* S = Header->AddChildToOverlay(CloseButton))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UHorizontalBox* Guide = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Guide))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("각인 슬롯에 "), 10, Label));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("우클릭 장착"), 10, SkillYellow));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("하여 최대 "), 10, Label));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("5개"), 10, SkillYellow));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("까지 각인을 활성화 할 수 있습니다."), 10, Label));

		USizeBox* BodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BodySize->SetHeightOverride(Setup_BodyHeight);
		Column->AddChildToVerticalBox(BodySize);
		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		BodySize->SetContent(Body);

		// ── 왼쪽: 각인 카드 ──
		USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ListSize->SetWidthOverride(Setup_ListWidth);
		Body->AddChildToHorizontalBox(ListSize);
		UBorder* ListBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ListBack->SetBrushColor(BoardColor);
		ListBack->SetPadding(FMargin(6.f));
		ListSize->SetContent(ListBack);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		ListBack->SetContent(Scroll);
		CardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		CardGrid->SetSlotPadding(FMargin(3.f));
		Scroll->AddChild(CardGrid);

		// ── 오른쪽: 프리셋 + 판 + 저장 ──
		UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Right)) S->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
		Right->AddChildToVerticalBox(MakePresetBar(WidgetTree, 10, 36.f));
		UBorder* PresetLabel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PresetLabel->SetBrushColor(SectionColor);
		PresetLabel->SetPadding(FMargin(8.f, 3.f));
		PresetLabel->SetContent(MakeText(WidgetTree, TEXT("각인 프리셋"), 10, Label));
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(PresetLabel)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		BoardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BoardBox->SetWidthOverride(Setup_Board);
		BoardBox->SetHeightOverride(Setup_Board);
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(BoardBox)) S->SetHorizontalAlignment(HAlign_Center);
		SaveButton = MakeButton(WidgetTree, TEXT("저장"), 12, FLinearColor(0.45f, 0.33f, 0.12f, 1.f), FLinearColor::White);
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(SaveButton))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		StatusText = MakeText(WidgetTree, TEXT("각인서는 소지품에서 우클릭해 읽습니다 — 1권 = 배움(유물 0단계), 이후 5권마다 +1단계"), 9, Dim);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StatusText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

void UEngravingSetupWidget::Open(UEngravingComponent* InEngravings, UInventoryComponent* InInventory)
{
	Engravings = InEngravings;
	Inventory = InInventory;
	Pending = InEngravings ? InEngravings->GetEquipped() : TArray<FName>();
	Pending.SetNum(LoAEngravings::SlotCount);
	Refresh();
}

void UEngravingSetupWidget::Refresh()
{
	if (!CardGrid || !BoardBox) return;
	Targets.Reset();
	Targets.Add({ CloseButton, EHit::Close, 0, NAME_None });
	Targets.Add({ SaveButton, EHit::Save, 0, NAME_None });
	CardGrid->ClearChildren();
	const UEngravingComponent* E = Engravings.Get();

	int32 Index = 0;
	for (const FEngravingDefinition& Def : LoAEngravings::GetAll())
	{
		const int32 Level = E ? E->GetLevel(Def.Id) : -1;
		const bool bLearned = Level >= 0;
		const bool bPending = Pending.Contains(Def.Id);

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetBrushColor(bPending ? FLinearColor(0.14f, 0.12f, 0.08f, 1.f) : SectionColor);
		Card->SetPadding(FMargin(4.f, 6.f));
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Content);
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(MakeText(WidgetTree, Def.Name.ToString(), 10, bLearned ? Orange : Dim)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		USizeBox* Ring = MakeRingIcon(WidgetTree, 58.f, bPending ? RingActive : RingIdle, LoAEngravings::LoadIcon(Def.Id));
		Ring->SetRenderOpacity(bLearned ? 1.f : 0.35f);
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(Ring))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f));
		}
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(MakeLevelDiamonds(WidgetTree, bLearned ? Level : 0, 9)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		const int32 Books = E ? E->GetBooksRead(Def.Id) : 0;
		const int32 Owned = Inventory.IsValid() ? Inventory->CountItem(Def.BookItemId) : 0;
		Card->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s  %s\n%s\n\n읽은 각인서 %d / %d권 · 소지 %d권%s"),
			*Def.Name.ToString(), bLearned ? *FString::Printf(TEXT("유물 %d단계"), Level) : TEXT("(안 배움)"),
			*LoAEngravings::Describe(Def.Id, FMath::Max(0, Level)), Books, LoAEngravings::MaxBooks, Owned,
			bLearned ? TEXT("\n\n우클릭: 장착") : TEXT("\n\n각인서 1권을 읽으면 배웁니다"))));
		if (UUniformGridSlot* S = CardGrid->AddChildToUniformGrid(Card, Index / Setup_Columns, Index % Setup_Columns))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
		Targets.Add({ Card, EHit::Card, Index, Def.Id });
		++Index;
	}

	TArray<UWidget*> SlotWidgets;
	BoardBox->SetContent(MakeEngravingBoard(WidgetTree, Setup_Board, Pending, E, SlotWidgets, 60.f));
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		Targets.Add({ SlotWidgets[i], EHit::Slot, i, NAME_None });
	}
}

void UEngravingSetupWidget::Notify(const FText& Message, bool bError) const
{
	if (!StatusText) return;
	StatusText->SetText(Message);
	StatusText->SetColorAndOpacity(FSlateColor(bError ? FLinearColor(1.f, 0.45f, 0.35f, 1.f) : Gold));
}

bool UEngravingSetupWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

FReply UEngravingSetupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Pos = InMouseEvent.GetScreenSpacePosition();
	const bool bLeft = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bRight = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

	const FHitTarget* Hit = nullptr;
	for (const FHitTarget& T : Targets)
	{
		if (T.Widget.IsValid() && T.Widget->GetCachedGeometry().IsUnderLocation(Pos))
		{
			Hit = &T;
			break;
		}
	}

	if (!Hit)
	{
		if (bLeft && HeaderBox && HeaderBox->GetCachedGeometry().IsUnderLocation(Pos) && WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
		{
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
		return FReply::Handled();
	}
	const FHitTarget Target = *Hit;  // 아래 Refresh가 목록을 다시 만든다
	UEngravingComponent* E = Engravings.Get();

	if (bLeft && Target.Kind == EHit::Close)
	{
		if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>()) PC->SetEngravingSetupOpen(false);
	}
	else if (bLeft && Target.Kind == EHit::Save && E)
	{
		FText Fail;
		if (E->SetAllEquipped(Pending, Fail)) Notify(NSLOCTEXT("Engraving", "Saved", "각인을 저장했습니다."), false);
		else if (!Fail.IsEmpty()) Notify(Fail);
		Pending = E->GetEquipped();
		Refresh();
	}
	else if (bRight && Target.Kind == EHit::Card && E)
	{
		if (E->GetLevel(Target.Id) < 0) Notify(NSLOCTEXT("Engraving", "NotLearnedSetup", "아직 배우지 않은 각인입니다 — 소지품에서 각인서를 우클릭해 읽어 주세요."));
		else if (Pending.Contains(Target.Id)) Notify(NSLOCTEXT("Engraving", "AlreadyIn", "이미 장착한 각인입니다."));
		else
		{
			const int32 Empty = Pending.IndexOfByKey(NAME_None);
			if (Empty == INDEX_NONE) Notify(NSLOCTEXT("Engraving", "BoardFull", "5칸이 모두 찼습니다 — 판의 각인을 우클릭해 빼 주세요."));
			else
			{
				Pending[Empty] = Target.Id;
				Notify(NSLOCTEXT("Engraving", "PendingHint", "[저장]을 눌러야 적용됩니다."), false);
				Refresh();
			}
		}
	}
	else if (bRight && Target.Kind == EHit::Slot && Pending.IsValidIndex(Target.Index) && !Pending[Target.Index].IsNone())
	{
		Pending[Target.Index] = NAME_None;
		Notify(NSLOCTEXT("Engraving", "PendingHint", "[저장]을 눌러야 적용됩니다."), false);
		Refresh();
	}
	return FReply::Handled();
}

FReply UEngravingSetupWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 두 번째 클릭은 DoubleClick으로 온다 — 안 먹으면 맵 클릭 이동이 샌다
	return FReply::Handled();
}

FReply UEngravingSetupWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UEngravingSetupWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UEngravingSetupWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
