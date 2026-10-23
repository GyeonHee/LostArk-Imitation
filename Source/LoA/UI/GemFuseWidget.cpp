#include "UI/GemFuseWidget.h"
#include "UI/BuildUIHelpers.h"
#include "UI/InventoryDragOperation.h"
#include "Item/GemComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemTypes.h"
#include "LoAPlayerController.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

using namespace LoABuildUI;

namespace
{
	constexpr float Fuse_Width = 380.f;
	constexpr float Fuse_Gap = 2.f;          // 소지품 창과의 틈
	constexpr float Fuse_InputCircle = 64.f;
	constexpr float Fuse_ResultCircle = 96.f;
	constexpr int32 Fuse_Columns = 4;
	constexpr int32 Fuse_MinTarget = 2;
	constexpr int32 Fuse_MaxTarget = 9;      // 로아 창처럼 2~9레벨

	// 재료 칸 — 가운데 고리(궤도) 위, 각도(도, 0 = 오른쪽, 시계 방향): 왼쪽 위 · 오른쪽 위 · 아래 (로아 배치)
	const float Fuse_InputAngles[LoAGems::FuseCount] = { -140.f, -40.f, 90.f };
	const FVector2D Fuse_ResultRatio(0.5f, 0.5f);

	// 버튼 색 — 보통 / 마우스 올림 / 누르는 중 (셋이 확실히 달라 보이게)
	const FLinearColor Fuse_ButtonHover(0.22f, 0.25f, 0.34f, 1.f);
	const FLinearColor Fuse_ButtonPressed(0.55f, 0.42f, 0.16f, 1.f);

	FLinearColor Fuse_LevelColor(int32 Level)
	{
		switch (Level)
		{
		case 2:  return FLinearColor(0.35f, 0.85f, 0.25f, 1.f);
		case 3:  return FLinearColor(0.25f, 0.65f, 1.f, 1.f);
		case 4:  return FLinearColor(0.30f, 0.80f, 1.f, 1.f);
		case 5:  return FLinearColor(0.75f, 0.40f, 1.f, 1.f);
		case 6:  return FLinearColor(0.80f, 0.45f, 1.f, 1.f);
		case 7:  return FLinearColor(1.f, 0.60f, 0.20f, 1.f);
		case 8:  return FLinearColor(1.f, 0.55f, 0.15f, 1.f);
		default: return FLinearColor(1.f, 0.50f, 0.10f, 1.f);
		}
	}
}

TSharedRef<SWidget> UGemFuseWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GemFuseRoot"));
		WidgetTree->RootWidget = Root;

		// 크기·위치는 NativeTick이 소지품 창에 맞춘다 (왼쪽 위 기준)
		PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GemFuseSize"));
		PanelSize->SetWidthOverride(Fuse_Width);
		PanelSize->SetHeightOverride(600.f);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(PanelSize))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(0.f, 0.f));
			S->SetAlignment(FVector2D(0.f, 0.f));
		}
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GemFusePanel"));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(12.f, 4.f, 12.f, 12.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		PanelSize->SetContent(Panel);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		USizeBox* HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HeaderBox->SetHeightOverride(30.f);
		Column->AddChildToVerticalBox(HeaderBox);
		UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		HeaderBox->SetContent(Header);
		if (UOverlaySlot* S = Header->AddChildToOverlay(MakeText(WidgetTree, TEXT("보석 합성"), 15, FLinearColor(0.92f, 0.87f, 0.72f))))
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
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("동일 레벨 보석 3개"), 10, SkillYellow));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("를 합성하여 "), 10, Label));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("상위 레벨 보석 1개"), 10, SkillYellow));
		Guide->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT(" 획득"), 10, Label));

		// 판 — 남는 높이를 전부 차지 (재료 칸 위치는 판 크기 비율로)
		UBorder* BoardBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BoardBack->SetBrushColor(BoardColor);
		BoardBack->SetPadding(FMargin(0.f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BoardBack)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Board = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		BoardBack->SetContent(Board);

		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("합성 목표 레벨 선택"), 13, FLinearColor(0.92f, 0.87f, 0.72f))))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));
		}
		UUniformGridPanel* TargetGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		TargetGrid->SetSlotPadding(FMargin(3.f));
		Column->AddChildToVerticalBox(TargetGrid);
		for (int32 Level = Fuse_MinTarget; Level <= Fuse_MaxTarget; ++Level)
		{
			UBorder* LevelButton = MakeButton(WidgetTree, FString::Printf(TEXT("%d레벨"), Level), 11, SectionColor, Fuse_LevelColor(Level));
			const int32 Index = Level - Fuse_MinTarget;
			if (UUniformGridSlot* S = TargetGrid->AddChildToUniformGrid(LevelButton, Index / Fuse_Columns, Index % Fuse_Columns))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
			}
			Targets.Add({ LevelButton, EHit::Target, Level });
		}

		FuseButton = MakeButton(WidgetTree, TEXT("합성하기"), 12, ButtonColor);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(FuseButton))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetPadding(FMargin(3.f, 10.f, 3.f, 0.f));
		}
		Targets.Add({ CloseButton, EHit::Close, 0 });
		Targets.Add({ FuseButton, EHit::Fuse, 0 });

		StatusText = MakeText(WidgetTree, TEXT(""), 9, Dim);
		StatusText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StatusText)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	RefreshBoard();
	return Result;
}

void UGemFuseWidget::Open(UGemComponent* InGems, UWidget* AttachTo)
{
	if (UGemComponent* Old = Gems.Get()) Old->OnFuseInputsChanged.Remove(InputsHandle);
	Gems = InGems;
	AttachPanel = AttachTo;
	if (InGems) InputsHandle = InGems->OnFuseInputsChanged.AddUObject(this, &UGemFuseWidget::HandleInputsChanged);
	LastResult = NAME_None;
	RefreshBoard();
	Notify(NSLOCTEXT("GemFuse", "Guide", "소지품의 보석을 우클릭하거나 끌어다 놓아 올립니다. 재료 칸 우클릭 = 되돌리기"), false);
	if (StatusText) StatusText->SetColorAndOpacity(FSlateColor(Dim));
}

void UGemFuseWidget::Close()
{
	if (UGemComponent* G = Gems.Get())
	{
		G->ReturnAllFuseInputs();
		G->OnFuseInputsChanged.Remove(InputsHandle);
	}
	InputsHandle.Reset();
	ReleasePressedButton();
}

void UGemFuseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 소지품 창 왼쪽에 같은 높이로 붙는다 — 소지품 창을 끌어 옮겨도 따라감
	const UWidget* Attach = AttachPanel.Get();
	UCanvasPanelSlot* PanelSlot = PanelSize ? Cast<UCanvasPanelSlot>(PanelSize->Slot) : nullptr;
	if (!Attach || !PanelSlot) return;
	const FGeometry& Inv = Attach->GetCachedGeometry();
	const FVector2D TopLeft = MyGeometry.AbsoluteToLocal(Inv.GetAbsolutePosition());
	const FVector2D BottomRight = MyGeometry.AbsoluteToLocal(Inv.GetAbsolutePosition() + Inv.GetAbsoluteSize());
	const float Height = BottomRight.Y - TopLeft.Y;
	if (Height <= 1.f) return;  // 소지품 창이 아직 안 그려짐
	PanelSize->SetHeightOverride(Height);
	PanelSlot->SetPosition(FVector2D(TopLeft.X - Fuse_Width - Fuse_Gap, TopLeft.Y));

	// 버튼 색 — 누르는 중 > 마우스 올림 > 보통
	const FVector2D CursorPos = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector;
	for (const FHitTarget& T : Targets)
	{
		UBorder* Button = Cast<UBorder>(T.Widget.Get());
		if (!Button || (T.Kind != EHit::Target && T.Kind != EHit::Fuse)) continue;
		const bool bHovered = Button->GetCachedGeometry().IsUnderLocation(CursorPos);
		const FLinearColor Normal = T.Kind == EHit::Fuse ? ButtonColor : SectionColor;
		Button->SetBrushColor(PressedButton.Get() == Button ? Fuse_ButtonPressed : (bHovered ? Fuse_ButtonHover : Normal));
	}

	if (Board)
	{
		const FVector2D BoardSize = Board->GetCachedGeometry().GetLocalSize();
		if (!BoardSize.Equals(LastBoardSize, 1.f))
		{
			LastBoardSize = BoardSize;
			RefreshBoard();
		}
	}
}

void UGemFuseWidget::AddInput(FName GemItemId)
{
	FText Fail;
	UGemComponent* G = Gems.Get();
	if (!G) return;
	if (G->AddFuseInput(GemItemId, Fail))
	{
		LastResult = NAME_None;
		RefreshBoard();
	}
	else if (!Fail.IsEmpty())
	{
		Notify(Fail);
	}
}

void UGemFuseWidget::RefreshBoard()
{
	if (!Board) return;
	Board->ClearChildren();
	InputTargets.Reset();

	// 판 크기 — 아직 안 그려졌으면 기본값
	FVector2D BoardSize = Board->GetCachedGeometry().GetLocalSize();
	if (BoardSize.X < 10.f || BoardSize.Y < 10.f) BoardSize = FVector2D(Fuse_Width - 24.f, 320.f);
	auto At = [&BoardSize](const FVector2D& Ratio) { return FVector2D(BoardSize.X * Ratio.X, BoardSize.Y * Ratio.Y); };

	// 고리 — 재료 칸(지름 Fuse_InputCircle)이 판 밖으로 안 나가게 그만큼 안쪽으로
	const FVector2D Center = At(Fuse_ResultRatio);
	const float Ring = FMath::Min(BoardSize.X, BoardSize.Y) - Fuse_InputCircle - 16.f;
	const float OrbitRadius = Ring * 0.5f - 2.5f;  // 고리 두께(5) 가운데
	PlaceAt(Board, MakeCircleImage(WidgetTree, FLinearColor(0.10f, 0.11f, 0.15f, 1.f)), Center, FVector2D(Ring));
	PlaceAt(Board, MakeCircleImage(WidgetTree, BoardColor), Center, FVector2D(Ring - 10.f));

	auto IconOf = [](FName Id) -> UTexture2D*
	{
		const FItemDefinition* Def = LoAItems::Find(Id);
		return Def ? Def->Icon.LoadSynchronous() : nullptr;
	};
	const TArray<FName> Inputs = Gems.IsValid() ? Gems->GetFuseInputs() : TArray<FName>();
	for (int32 i = 0; i < LoAGems::FuseCount; ++i)
	{
		const bool bHas = Inputs.IsValidIndex(i);
		USizeBox* Circle = MakeRingIcon(WidgetTree, Fuse_InputCircle, bHas ? Orange : RingIdle, bHas ? IconOf(Inputs[i]) : nullptr);
		if (bHas)
		{
			EGemType Type;
			int32 Level;
			LoAGems::ParseGemId(Inputs[i], Type, Level);
			Circle->SetToolTipText(FText::FromString(LoAGems::GetGemName(Type, Level).ToString() + TEXT("\n\n우클릭: 소지품으로")));
		}
		const float Angle = FMath::DegreesToRadians(Fuse_InputAngles[i]);
		PlaceAt(Board, Circle, Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * OrbitRadius, FVector2D(Fuse_InputCircle));
		InputTargets.Add({ Circle, EHit::Input, i });
	}

	USizeBox* Result = MakeRingIcon(WidgetTree, Fuse_ResultCircle, LastResult.IsNone() ? RingIdle : Gold,
		LastResult.IsNone() ? nullptr : IconOf(LastResult), 5.f);
	if (!LastResult.IsNone())
	{
		EGemType Type;
		int32 Level;
		LoAGems::ParseGemId(LastResult, Type, Level);
		Result->SetToolTipText(FText::FromString(LoAGems::GetGemName(Type, Level).ToString() + TEXT("\n") + LoAGems::DescribeGem(Type, Level)));
	}
	PlaceAt(Board, Result, Center, FVector2D(Fuse_ResultCircle));
}

void UGemFuseWidget::Notify(const FText& Message, bool bError) const
{
	if (!StatusText) return;
	StatusText->SetText(Message);
	StatusText->SetColorAndOpacity(FSlateColor(bError ? FLinearColor(1.f, 0.45f, 0.35f, 1.f) : Gold));
}

bool UGemFuseWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

bool UGemFuseWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UInventoryDragOperation* Operation = Cast<UInventoryDragOperation>(InOperation);
	const UGemComponent* G = Gems.Get();
	const UInventoryComponent* Inv = G && G->GetOwner() ? G->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Operation || !Inv || !Inv->IsValidIndex(Operation->SourceIndex)) return false;
	AddInput(Inv->GetSlot(Operation->SourceIndex).ItemId);
	return true;
}

void UGemFuseWidget::ReleasePressedButton()
{
	PressedButton.Reset();  // 색은 NativeTick이 다시 칠한다
}

FReply UGemFuseWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Pos = InMouseEvent.GetScreenSpacePosition();
	const bool bLeft = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bRight = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

	auto Find = [&Pos](const TArray<FHitTarget>& List) -> const FHitTarget*
	{
		for (const FHitTarget& T : List)
		{
			if (T.Widget.IsValid() && T.Widget->GetCachedGeometry().IsUnderLocation(Pos)) return &T;
		}
		return nullptr;
	};
	const FHitTarget* Hit = Find(InputTargets);
	if (!Hit) Hit = Find(Targets);
	if (!Hit) return FReply::Handled();
	const FHitTarget Target = *Hit;
	UGemComponent* G = Gems.Get();
	FText Fail;

	if (bRight && Target.Kind == EHit::Input && G)
	{
		if (!G->RemoveFuseInput(Target.Index, Fail) && !Fail.IsEmpty()) Notify(Fail);
	}
	else if (bLeft && Target.Kind == EHit::Close)
	{
		if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>()) PC->SetGemFuseOpen(false);
	}
	else if (bLeft && Target.Kind == EHit::Target && G)
	{
		// 누르는 동안만 밝게 — 떼면(NativeOnMouseButtonUp) 원래 색. 적용은 지금 한 번
		PressedButton = Cast<UBorder>(Target.Widget.Get());
		LastResult = NAME_None;
		if (!G->FillFuseInputsForTarget(Target.Index, Fail) && !Fail.IsEmpty()) Notify(Fail);
		RefreshBoard();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	else if (bLeft && Target.Kind == EHit::Fuse && G)
	{
		PressedButton = Cast<UBorder>(Target.Widget.Get());
		FName Result;
		if (G->FuseInputs3(Result, Fail))
		{
			EGemType Type;
			int32 Level;
			LoAGems::ParseGemId(Result, Type, Level);
			LastResult = Result;
			Notify(FText::FromString(FString::Printf(TEXT("%s 획득!"), *LoAGems::GetGemName(Type, Level).ToString())), false);
			RefreshBoard();
		}
		else if (!Fail.IsEmpty())
		{
			Notify(Fail);
		}
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UGemFuseWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PressedButton.IsValid())
	{
		ReleasePressedButton();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

FReply UGemFuseWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빠른 연타의 두 번째 클릭은 먹기만 — 맵 클릭 이동이 새지 않게, [합성하기]가 두 번 되지 않게
	return FReply::Handled();
}
