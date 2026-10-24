#include "UI/ArkPassiveWidget.h"
#include "UI/BuildUIHelpers.h"
#include "Stat/ArkPassiveComponent.h"
#include "LoAPlayerController.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

using namespace LoABuildUI;

namespace
{
	constexpr float Ark_LeftWidth = 270.f;
	constexpr float Ark_TierWidth = 92.f;
	constexpr float Ark_CellWidth = 96.f;
	constexpr float Ark_RowHeight = 104.f;
	constexpr float Ark_NodeDiameter = 58.f;
	constexpr int32 Ark_MaxColumns = 6;
	const FLinearColor Ark_Locked(0.16f, 0.17f, 0.20f, 1.f);

	FLinearColor Ark_Dim(const FLinearColor& C, float Scale)
	{
		return FLinearColor(C.R * Scale, C.G * Scale, C.B * Scale, C.A);
	}

	void Ark_AddFill(UOverlay* Layers, UWidget* W, float Pad)
	{
		if (UOverlaySlot* S = Layers->AddChildToOverlay(W))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(Pad));
		}
	}

	void Ark_AddCentered(UOverlay* Layers, UWidget* W, EVerticalAlignment V = VAlign_Center, FMargin Pad = FMargin(0.f))
	{
		if (UOverlaySlot* S = Layers->AddChildToOverlay(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(V);
			S->SetPadding(Pad);
		}
	}
}

TSharedRef<SWidget> UArkPassiveWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ArkPassiveRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ArkPassivePanel"));
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

		// ── 헤더 — 가운데 "아크 패시브", 오른쪽 X (끌면 창 이동) ──
		HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HeaderBox->SetHeightOverride(34.f);
		Column->AddChildToVerticalBox(HeaderBox);
		UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		HeaderBox->SetContent(Header);
		Ark_AddCentered(Header, MakeText(WidgetTree, TEXT("아크 패시브"), 15, FLinearColor(0.92f, 0.87f, 0.72f)));
		CloseButton = MakeButton(WidgetTree, TEXT("X"), 12, FLinearColor(0.f, 0.f, 0.f, 0.f));
		if (UOverlaySlot* S = Header->AddChildToOverlay(CloseButton))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}

		// ── 탭 + 추천 세팅 ──
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TabRow)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		TabBorders.Reset();
		TabTexts.Reset();
		for (int32 i = 0; i < static_cast<int32>(EArkPassiveTree::MAX); ++i)
		{
			UBorder* Tab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Tab->SetPadding(FMargin(0.f, 6.f));
			Tab->SetHorizontalAlignment(HAlign_Center);
			UTextBlock* Label = MakeText(WidgetTree, LoAArkPassive::GetTreeName(static_cast<EArkPassiveTree>(i)).ToString(), 12, UiLabel);
			Tab->SetContent(Label);
			USizeBox* TabSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			TabSize->SetWidthOverride(150.f);
			TabSize->SetContent(Tab);
			if (UHorizontalBoxSlot* S = TabRow->AddChildToHorizontalBox(TabSize)) S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			TabBorders.Add(Tab);
			TabTexts.Add(Label);
		}
		if (UHorizontalBoxSlot* S = TabRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		RecommendButton = MakeButton(WidgetTree, TEXT("✦ 추천 세팅"), 11, FLinearColor(0.10f, 0.42f, 0.40f, 1.f), FLinearColor::White);
		RecommendButton->SetToolTipText(NSLOCTEXT("ArkPassive", "RecommendTip", "환류 소서리스 추천 배치를 불러옵니다 (세 탭 전부). [저장]을 눌러야 적용됩니다."));
		TabRow->AddChildToHorizontalBox(RecommendButton);

		// ── 본문 ──
		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Body);

		// 왼쪽: 포인트 출처 · 카르마 · 종합 효과
		USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LeftSize->SetWidthOverride(Ark_LeftWidth);
		Body->AddChildToHorizontalBox(LeftSize);
		UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftSize->SetContent(Left);

		UBorder* SourceBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SourceBox->SetBrushColor(SectionColor);
		SourceBox->SetPadding(FMargin(10.f, 8.f));
		Left->AddChildToVerticalBox(SourceBox);
		UVerticalBox* SourceLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		SourceBox->SetContent(SourceLines);
		SourceTitle = MakeText(WidgetTree, TEXT(""), 13, UiGold);
		if (UVerticalBoxSlot* S = SourceLines->AddChildToVerticalBox(SourceTitle)) S->SetHorizontalAlignment(HAlign_Center);
		SourceText = MakeText(WidgetTree, TEXT(""), 10, UiLabel);
		SourceText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = SourceLines->AddChildToVerticalBox(SourceText)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		UBorder* KarmaBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		KarmaBox->SetBrushColor(RowColor);
		KarmaBox->SetPadding(FMargin(10.f, 6.f));
		KarmaBox->SetContent(MakeText(WidgetTree, TEXT("카르마 — 아직 없음 (포인트는 최대치로 고정)"), 9, UiDim));
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(KarmaBox)) S->SetPadding(FMargin(0.f, 6.f));

		SummaryTitle = MakeText(WidgetTree, TEXT(""), 11, UiGold);
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(SummaryTitle))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));
		}
		UBorder* SummaryBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SummaryBack->SetBrushColor(BoardColor);
		SummaryBack->SetPadding(FMargin(4.f));
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(SummaryBack)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UScrollBox* SummaryScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		SummaryBack->SetContent(SummaryScroll);
		SummaryList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		SummaryScroll->AddChild(SummaryList);

		// 오른쪽: 안내 + 포인트 / 티어 줄
		UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Right)) S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		UHorizontalBox* PointsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(PointsRow)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		GuideText = MakeText(WidgetTree, TEXT(""), 10, UiLabel);
		if (UHorizontalBoxSlot* S = PointsRow->AddChildToHorizontalBox(GuideText)) S->SetVerticalAlignment(VAlign_Center);
		if (UHorizontalBoxSlot* S = PointsRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		PointsText = MakeText(WidgetTree, TEXT(""), 13, FLinearColor::White);
		PointsRow->AddChildToHorizontalBox(PointsText);

		UBorder* TreeBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		TreeBack->SetBrushColor(BoardColor);
		TreeBack->SetPadding(FMargin(8.f));
		USizeBox* TreeSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TreeSize->SetWidthOverride(Ark_TierWidth + Ark_CellWidth * Ark_MaxColumns + 16.f);
		TreeSize->SetHeightOverride(Ark_RowHeight * 5 + 16.f);
		TreeSize->SetContent(TreeBack);
		Right->AddChildToVerticalBox(TreeSize);
		TreeRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TreeBack->SetContent(TreeRows);

		// ── 아래: 프리셋 + 되돌리기 · 초기화 · 저장 ──
		UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Bottom)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("아크 패시브 프리셋  "), 10, UiLabel))) S->SetVerticalAlignment(VAlign_Center);
		Bottom->AddChildToHorizontalBox(MakePresetBar(WidgetTree, 10, 34.f));
		if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		auto AddBottomButton = [this, Bottom](const TCHAR* Label, const FLinearColor& Fill, const FLinearColor& TextColor) -> UBorder*
		{
			UBorder* Button = MakeButton(WidgetTree, Label, 11, Fill, TextColor);
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(110.f);
			Box->SetHeightOverride(30.f);
			Box->SetContent(Button);
			if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(Box)) S->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
			return Button;
		};
		RevertButton = AddBottomButton(TEXT("되돌리기"), ButtonColor, UiLabel);
		RevertButton->SetToolTipText(NSLOCTEXT("ArkPassive", "RevertTip", "마지막으로 저장한 배치로 되돌립니다."));
		ResetButton = AddBottomButton(TEXT("초기화"), ButtonColor, UiLabel);
		ResetButton->SetToolTipText(NSLOCTEXT("ArkPassive", "ResetTip", "지금 탭의 포인트를 전부 돌려받습니다. [저장]을 눌러야 적용됩니다."));
		SaveButton = AddBottomButton(TEXT("저장"), FLinearColor(0.45f, 0.33f, 0.12f, 1.f), FLinearColor::White);

		StatusText = MakeText(WidgetTree, TEXT("좌클릭 +1 · 우클릭 -1 · Shift = 10레벨씩.  바꾼 뒤 [저장]을 눌러야 적용됩니다."), 9, UiDim);
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

void UArkPassiveWidget::Open(UArkPassiveComponent* InArkPassive)
{
	if (UArkPassiveComponent* Old = ArkPassive.Get()) Old->OnArkPassiveChanged.Remove(ChangedHandle);
	ArkPassive = InArkPassive;
	Pending = InArkPassive ? InArkPassive->GetLevels() : TMap<FName, int32>();
	Notify(NSLOCTEXT("ArkPassive", "Hint", "좌클릭 +1 · 우클릭 -1 · Shift = 10레벨씩.  바꾼 뒤 [저장]을 눌러야 적용됩니다."), false);
	if (StatusText) StatusText->SetColorAndOpacity(FSlateColor(UiDim));
	Refresh();
}

// ─── 화면 구성 ─────────────────────────────────────────────────────────────

UWidget* UArkPassiveWidget::MakeTierCell(int32 Tier)
{
	const EArkPassiveTree Tree = GetTree();
	const FLinearColor TreeColor = LoAArkPassive::GetTreeColor(Tree);
	const bool bUnlocked = LoAArkPassive::IsTierUnlocked(Pending, Tree, Tier);
	const int32 Spent = LoAArkPassive::GetSpent(Pending, Tree, Tier);
	const int32 Cap = LoAArkPassive::GetTierCap(Tree, Tier);

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Ark_TierWidth);
	Box->SetHeightOverride(Ark_RowHeight);
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Box->SetContent(Lines);

	// 마름모 대신 둥근 칸 — 안에 티어 숫자
	USizeBox* Badge = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Badge->SetWidthOverride(52.f);
	Badge->SetHeightOverride(52.f);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Badge->SetContent(Layers);
	const bool bFull = Cap > 0 ? Spent >= Cap : Spent > 0;
	Ark_AddFill(Layers, MakeCircleImage(WidgetTree, bUnlocked ? (bFull ? TreeColor : Ark_Dim(TreeColor, 0.55f)) : Ark_Locked), 0.f);
	Ark_AddFill(Layers, MakeCircleImage(WidgetTree, FLinearColor(0.05f, 0.05f, 0.07f, 1.f)), 3.f);
	Ark_AddCentered(Layers, MakeText(WidgetTree, FString::FromInt(Tier + 1), 18, bUnlocked ? FLinearColor::White : UiDim));
	if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(Badge))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 2.f));
	}
	const FString Count = Cap > 0 ? FString::Printf(TEXT("%d/%d"), Spent, Cap) : FString::FromInt(Spent);
	if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(MakeText(WidgetTree, bUnlocked ? Count : FString::Printf(TEXT("잠김  %s"), *Count), 10, bUnlocked ? UiLabel : UiDim)))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
	return Box;
}

UWidget* UArkPassiveWidget::MakeNodeCell(const FArkPassiveNode& Node)
{
	const FLinearColor TreeColor = LoAArkPassive::GetTreeColor(Node.Tree);
	const int32 Level = LoAArkPassive::GetLevel(Pending, Node.Id);
	const bool bUnlocked = LoAArkPassive::IsTierUnlocked(Pending, Node.Tree, Node.Tier)
		&& (Node.Requires.IsNone() || LoAArkPassive::GetLevel(Pending, Node.Requires) >= Node.RequiresLevel);
	const bool bMax = Level >= Node.MaxLevel;

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Ark_CellWidth);
	Box->SetHeightOverride(Ark_RowHeight);
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Box->SetContent(Lines);

	// 원 — 배움: 트리 색 / 최대: 금색 / 못 배움: 어두운 테두리. 가운데 글자 + 아래 nP
	USizeBox* Ring = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Ring->SetWidthOverride(Ark_NodeDiameter);
	Ring->SetHeightOverride(Ark_NodeDiameter);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Ring->SetContent(Layers);
	const FLinearColor RingColor = Level > 0 ? (bMax ? UiGold : TreeColor) : (bUnlocked ? FLinearColor(0.30f, 0.31f, 0.36f, 1.f) : Ark_Locked);
	if (Level > 0)
	{
		// 배운 노드는 바깥에 은은한 빛 (로아의 발광 테두리 흉내)
		UImage* Glow = MakeCircleImage(WidgetTree, FLinearColor(RingColor.R, RingColor.G, RingColor.B, 0.25f));
		Ark_AddFill(Layers, Glow, -3.f);
	}
	Ark_AddFill(Layers, MakeCircleImage(WidgetTree, RingColor), 0.f);
	Ark_AddFill(Layers, MakeCircleImage(WidgetTree, Level > 0 ? Ark_Dim(TreeColor, 0.22f) : FLinearColor(0.07f, 0.07f, 0.09f, 1.f)), Level > 0 ? 3.f : 2.f);
	FLinearColor GlyphColor = Level > 0 ? FLinearColor::White : (bUnlocked ? FLinearColor(0.7f, 0.7f, 0.74f, 1.f) : UiDim);
	if (!Node.bHasEffect) GlyphColor = Ark_Dim(GlyphColor, 0.7f);
	FString Glyph;
	Glyph.AppendChar(Node.Glyph);
	Ark_AddCentered(Layers, MakeText(WidgetTree, Glyph, 18, GlyphColor), VAlign_Center, FMargin(0.f, 0.f, 0.f, 8.f));
	Ark_AddCentered(Layers, MakeText(WidgetTree, FString::Printf(TEXT("%dP"), Node.Points), 8, Level > 0 ? FLinearColor::White : UiDim), VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 6.f));
	Ring->SetRenderOpacity(bUnlocked || Level > 0 ? 1.f : 0.45f);
	Ring->SetToolTipText(FText::FromString(MakeNodeTooltip(Node)));
	if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(Ring))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	}

	// 이름 + Lv.x/y
	UTextBlock* Name = MakeText(WidgetTree, Node.Name.ToString(), 9, Level > 0 ? (Node.bHasEffect ? UiOrange : FLinearColor(0.6f, 0.45f, 0.3f, 1.f)) : UiDim);
	if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(Name)) S->SetHorizontalAlignment(HAlign_Center);
	UBorder* LevelBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LevelBox->SetBrushColor(Level > 0 ? FLinearColor(0.10f, 0.09f, 0.06f, 1.f) : RowColor);
	LevelBox->SetPadding(FMargin(6.f, 0.f));
	LevelBox->SetContent(MakeText(WidgetTree, FString::Printf(TEXT("Lv.%d/%d"), Level, Node.MaxLevel), 9, bMax ? UiGold : (Level > 0 ? FLinearColor::White : UiDim)));
	if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(LevelBox)) S->SetHorizontalAlignment(HAlign_Center);

	Targets.Add({ Ring, EHit::Node, INDEX_NONE, Node.Id });
	return Box;
}

FString UArkPassiveWidget::MakeNodeTooltip(const FArkPassiveNode& Node) const
{
	const int32 Level = LoAArkPassive::GetLevel(Pending, Node.Id);
	FString Tip = FString::Printf(TEXT("%s\n%s · %d티어   아크 패시브 레벨 %d / %d\n%s 포인트 %d 필요 (레벨당)\n\n"),
		*Node.Name.ToString(), *LoAArkPassive::GetTreeName(Node.Tree).ToString(), Node.Tier + 1, Level, Node.MaxLevel,
		*LoAArkPassive::GetTreeName(Node.Tree).ToString(), Node.Points);
	Tip += LoAArkPassive::Describe(Node.Id, FMath::Max(1, Level));
	if (Level > 0 && Level < Node.MaxLevel)
	{
		Tip += FString::Printf(TEXT("\n\n[다음 레벨] %s"), *LoAArkPassive::Describe(Node.Id, Level + 1));
	}

	// 해방 조건
	TArray<FString> Conditions;
	if (Node.Tree == EArkPassiveTree::Evolution && Node.Tier > 0)
	{
		Conditions.Add(FString::Printf(TEXT("• %d티어까지 진화 포인트를 상한까지 사용"), Node.Tier));
	}
	if (Node.Tree == EArkPassiveTree::Leap && Node.Tier > 0)
	{
		Conditions.Add(FString::Printf(TEXT("• 1티어 도약 포인트 40 사용 (%d/40)"), LoAArkPassive::GetSpent(Pending, Node.Tree, 0)));
	}
	if (const FArkPassiveNode* Req = LoAArkPassive::Find(Node.Requires))
	{
		Conditions.Add(FString::Printf(TEXT("• [%s] %d레벨 (%d/%d)"), *Req->Name.ToString(), Node.RequiresLevel, LoAArkPassive::GetLevel(Pending, Req->Id), Node.RequiresLevel));
	}
	if (!Node.ExclusiveGroup.IsNone())
	{
		TArray<FString> Others;
		for (const FArkPassiveNode& Other : LoAArkPassive::GetAll())
		{
			if (Other.ExclusiveGroup == Node.ExclusiveGroup && Other.Id != Node.Id) Others.Add(Other.Name.ToString());
		}
		Conditions.Add(FString::Printf(TEXT("• 이것을 배우면 [%s]은(는) 배울 수 없음"), *FString::Join(Others, TEXT("], ["))));
	}
	if (Conditions.Num() > 0)
	{
		Tip += TEXT("\n\n아크 패시브 해방 조건\n") + FString::Join(Conditions, TEXT("\n"));
	}
	if (!Node.Note.IsEmpty())
	{
		Tip += FString::Printf(TEXT("\n\n※ %s"), *Node.Note);
	}
	Tip += TEXT("\n\n좌클릭 +1 · 우클릭 -1 · Shift = 10레벨씩");
	return Tip;
}

void UArkPassiveWidget::Refresh()
{
	if (!TreeRows || !SummaryList) return;
	const EArkPassiveTree Tree = GetTree();
	const FLinearColor TreeColor = LoAArkPassive::GetTreeColor(Tree);
	const FString TreeName = LoAArkPassive::GetTreeName(Tree).ToString();

	Targets.Reset();
	Targets.Add({ CloseButton, EHit::Close, 0, NAME_None });
	Targets.Add({ RecommendButton, EHit::Recommend, 0, NAME_None });
	Targets.Add({ RevertButton, EHit::Revert, 0, NAME_None });
	Targets.Add({ ResetButton, EHit::Reset, 0, NAME_None });
	Targets.Add({ SaveButton, EHit::Save, 0, NAME_None });

	// 탭
	for (int32 i = 0; i < TabBorders.Num(); ++i)
	{
		const bool bOn = i == CurrentTab;
		TabBorders[i]->SetBrushColor(bOn ? FLinearColor(0.20f, 0.17f, 0.10f, 1.f) : SectionColor);
		TabTexts[i]->SetColorAndOpacity(FSlateColor(bOn ? UiGold : UiDim));
		Targets.Add({ TabBorders[i], EHit::Tab, i, NAME_None });
	}

	// 왼쪽 — 포인트 출처 (장착 장비에서 계산한 내역)
	const FArkPassiveBudget Budget = ArkPassive.IsValid() ? ArkPassive->GetBudget() : FArkPassiveBudget();
	const TCHAR* SourceNames[] = { TEXT("진화 장비"), TEXT("깨달음 장신구"), TEXT("도약 팔찌") };
	const TCHAR* TreeNotes[] = {
		TEXT("모든 클래스 공통 — 1티어에서 전투 특성(치명·특화·신속 등)을 올린다."),
		TEXT("소서리스 전용 — 점화와 환류 중 하나만. 연계 노드는 선행 노드가 최고 레벨이어야 한다."),
		TEXT("각성기·초각성기 강화 (이 게임엔 아직 각성기가 없음). 1티어에 40포인트를 써야 2티어가 열린다."),
	};
	const int32 TreeIndex = static_cast<int32>(Tree);
	SourceTitle->SetText(FText::FromString(SourceNames[TreeIndex]));
	SourceText->SetText(FText::FromString(FString::Join(Budget.Lines[TreeIndex], TEXT("\n"))
		+ FString::Printf(TEXT("\n= %d (로아 최대 %d)\n\n%s"), Budget.Get(Tree), LoAArkPassive::GetTotalPoints(Tree), TreeNotes[TreeIndex])));
	SourceTitle->SetColorAndOpacity(FSlateColor(TreeColor));

	// 왼쪽 — 종합 효과
	SummaryTitle->SetText(FText::FromString(FString::Printf(TEXT("%s 아크 패시브 종합 효과"), *TreeName)));
	SummaryList->ClearChildren();
	TArray<const FArkPassiveNode*> Learned;
	for (const FArkPassiveNode& Node : LoAArkPassive::GetAll())
	{
		if (Node.Tree == Tree && LoAArkPassive::GetLevel(Pending, Node.Id) > 0) Learned.Add(&Node);
	}
	for (const FArkPassiveNode* Node : Learned)
	{
		const int32 Level = LoAArkPassive::GetLevel(Pending, Node->Id);
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetBrushColor(RowColor);
		Row->SetPadding(FMargin(8.f, 5.f));
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(Line);
		Line->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("%d티어  "), Node->Tier + 1), 10, UiLabel));
		Line->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("%s Lv.%d"), *Node->Name.ToString(), Level), 10, Node->bHasEffect ? TreeColor : UiDim));
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (Level >= Node->MaxLevel) Line->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("최대"), 10, UiOrange));
		Row->SetToolTipText(FText::FromString(LoAArkPassive::Describe(Node->Id, Level) + (Node->Note.IsEmpty() ? FString() : FString::Printf(TEXT("\n\n※ %s"), *Node->Note))));
		if (UVerticalBoxSlot* S = SummaryList->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	}
	if (Learned.Num() == 0)
	{
		SummaryList->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("  배운 아크 패시브가 없습니다."), 10, UiDim));
	}

	// 오른쪽 — 안내 · 포인트
	GuideText->SetText(FText::FromString(FString::Printf(TEXT("%s 포인트를 사용해서 아크 패시브를 활성화하세요."), *TreeName)));
	const int32 Spent = LoAArkPassive::GetSpent(Pending, Tree);
	const bool bOver = Spent > Budget.Get(Tree);
	PointsText->SetText(FText::FromString(FString::Printf(TEXT("%s%s 포인트   %d / %d"), bOver ? TEXT("포인트 부족 — 일부만 적용   ") : TEXT(""), *TreeName, Spent, Budget.Get(Tree))));
	PointsText->SetColorAndOpacity(FSlateColor(bOver ? FLinearColor(1.f, 0.4f, 0.3f, 1.f) : FLinearColor::White));

	// 오른쪽 — 티어 줄
	TreeRows->ClearChildren();
	for (int32 Tier = 0; Tier < LoAArkPassive::GetTierCount(Tree); ++Tier)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		TreeRows->AddChildToVerticalBox(Row);
		Row->AddChildToHorizontalBox(MakeTierCell(Tier));

		TArray<const FArkPassiveNode*> ByColumn;
		ByColumn.Init(nullptr, Ark_MaxColumns);
		for (const FArkPassiveNode& Node : LoAArkPassive::GetAll())
		{
			if (Node.Tree == Tree && Node.Tier == Tier && ByColumn.IsValidIndex(Node.Column)) ByColumn[Node.Column] = &Node;
		}
		for (const FArkPassiveNode* Node : ByColumn)
		{
			if (Node)
			{
				Row->AddChildToHorizontalBox(MakeNodeCell(*Node));
			}
			else
			{
				USizeBox* Empty = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Empty->SetWidthOverride(Ark_CellWidth);
				Empty->SetHeightOverride(Ark_RowHeight);
				Row->AddChildToHorizontalBox(Empty);
			}
		}
	}

	// 저장 안 한 변경이 있으면 [저장]을 밝게
	const bool bDirty = !ArkPassive.IsValid() || !Pending.OrderIndependentCompareEqual(ArkPassive->GetLevels());
	SaveButton->SetBrushColor(bDirty ? FLinearColor(0.62f, 0.45f, 0.14f, 1.f) : FLinearColor(0.25f, 0.20f, 0.10f, 1.f));
}

void UArkPassiveWidget::Notify(const FText& Message, bool bError) const
{
	if (!StatusText) return;
	StatusText->SetText(Message);
	StatusText->SetColorAndOpacity(FSlateColor(bError ? FLinearColor(1.f, 0.45f, 0.35f, 1.f) : UiGold));
}

bool UArkPassiveWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

// ─── 입력 ─────────────────────────────────────────────────────────────────

void UArkPassiveWidget::HandleClick(const FHitTarget& Target, bool bLeft, bool bShift)
{
	UArkPassiveComponent* Comp = ArkPassive.Get();
	switch (Target.Kind)
	{
	case EHit::Close:
		if (bLeft)
		{
			if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>()) PC->SetArkPassiveOpen(false);
		}
		return;
	case EHit::Tab:
		if (bLeft) CurrentTab = Target.Index;
		break;
	case EHit::Recommend:
		if (bLeft)
		{
			Pending = LoAArkPassive::GetRecommended(Comp ? Comp->GetBudget() : FArkPassiveBudget());
			Notify(NSLOCTEXT("ArkPassive", "Recommended", "추천 세팅을 불러왔습니다. [저장]을 눌러야 적용됩니다."), false);
		}
		break;
	case EHit::Revert:
		if (bLeft && Comp)
		{
			Pending = Comp->GetLevels();
			Notify(NSLOCTEXT("ArkPassive", "Reverted", "마지막으로 저장한 배치로 되돌렸습니다."), false);
		}
		break;
	case EHit::Reset:
		if (bLeft)
		{
			const EArkPassiveTree Tree = GetTree();
			for (auto It = Pending.CreateIterator(); It; ++It)
			{
				const FArkPassiveNode* Node = LoAArkPassive::Find(It.Key());
				if (Node && Node->Tree == Tree) It.RemoveCurrent();
			}
			Notify(FText::Format(NSLOCTEXT("ArkPassive", "ResetDone", "{0} 포인트를 모두 돌려받았습니다. [저장]을 눌러야 적용됩니다."), LoAArkPassive::GetTreeName(Tree)), false);
		}
		break;
	case EHit::Save:
		if (bLeft && Comp)
		{
			FText Fail;
			if (Comp->SetAllLevels(Pending, Fail)) Notify(NSLOCTEXT("ArkPassive", "Saved", "아크 패시브를 저장했습니다."), false);
			else Notify(Fail);
			Pending = Comp->GetLevels();
		}
		break;
	case EHit::Node:
	{
		FText Fail;
		const int32 Delta = (bLeft ? 1 : -1) * (bShift ? 10 : 1);
		if (LoAArkPassive::TryChange(Pending, Comp ? Comp->GetBudget() : FArkPassiveBudget(), Target.Id, Delta, Fail))
		{
			Notify(NSLOCTEXT("ArkPassive", "PendingHint", "[저장]을 눌러야 적용됩니다."), false);
		}
		else if (!Fail.IsEmpty())
		{
			Notify(Fail);
		}
		break;
	}
	}
	Refresh();
}

FReply UArkPassiveWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Pos = InMouseEvent.GetScreenSpacePosition();
	const bool bLeft = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bRight = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	if (!bLeft && !bRight) return FReply::Handled();

	for (const FHitTarget& T : Targets)
	{
		if (T.Widget.IsValid() && T.Widget->GetCachedGeometry().IsUnderLocation(Pos))
		{
			const FHitTarget Target = T;  // HandleClick의 Refresh가 목록을 다시 만든다
			HandleClick(Target, bLeft, InMouseEvent.IsShiftDown());
			return FReply::Handled();
		}
	}
	if (bLeft && HeaderBox && HeaderBox->GetCachedGeometry().IsUnderLocation(Pos) && WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UArkPassiveWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빠른 연타의 두 번째 클릭은 DoubleClick으로 온다 — 노드 연타가 씹히지 않게 같은 처리 (맵 클릭 이동으로도 새지 않음)
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UArkPassiveWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UArkPassiveWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UArkPassiveWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
