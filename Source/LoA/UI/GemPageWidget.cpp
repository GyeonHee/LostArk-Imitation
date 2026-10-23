#include "UI/GemPageWidget.h"
#include "UI/BuildUIHelpers.h"
#include "Item/GemComponent.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemTypes.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"

using namespace LoABuildUI;

namespace
{
	constexpr float Gem_LeftWidth = 560.f;
	constexpr float Gem_BoardHeight = 400.f;
	constexpr float Gem_Circle = 60.f;

	// 보석 11칸 위치 (판 안 중심 좌표) — 로아 보석 탭: 위 2+2, 가운데 3, 아래 2+2
	const FVector2D Gem_Positions[LoAGems::SlotCount] = {
		{ 120.f, 95.f }, { 195.f, 95.f }, { 365.f, 95.f }, { 440.f, 95.f },
		{ 205.f, 185.f }, { 280.f, 185.f }, { 355.f, 185.f },
		{ 120.f, 275.f }, { 195.f, 275.f }, { 365.f, 275.f }, { 440.f, 275.f },
	};
}

TSharedRef<SWidget> UGemPageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GemPageRoot"));
		WidgetTree->RootWidget = Root;

		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UOverlaySlot* S = Root->AddChildToOverlay(Body))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		// ── 왼쪽: 프리셋 + 판 ──
		USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LeftSize->SetWidthOverride(Gem_LeftWidth);
		Body->AddChildToHorizontalBox(LeftSize);
		UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftSize->SetContent(Left);

		Left->AddChildToVerticalBox(MakePresetBar(WidgetTree, 10));
		UBorder* PresetLabel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PresetLabel->SetBrushColor(SectionColor);
		PresetLabel->SetPadding(FMargin(8.f, 3.f));
		PresetLabel->SetContent(MakeText(WidgetTree, TEXT("보석 프리셋"), 10, UiLabel));
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(PresetLabel)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

		UBorder* BoardBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BoardBack->SetBrushColor(BoardColor);
		BoardBack->SetPadding(FMargin(0.f));
		USizeBox* BoardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BoardSize->SetHeightOverride(Gem_BoardHeight);
		BoardSize->SetContent(BoardBack);
		Left->AddChildToVerticalBox(BoardSize);
		Board = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		BoardBack->SetContent(Board);

		StatusText = MakeText(WidgetTree, TEXT("소지품의 보석을 우클릭하면 장착할 스킬을 고를 수 있습니다. 장착한 보석 우클릭 = 빼기"), 9, UiDim);
		if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(StatusText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// ── 오른쪽: 장착 중인 보석 효과 ──
		UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Right))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}
		UTextBlock* Title = MakeText(WidgetTree, TEXT("장착 중인 보석 효과"), 13, FLinearColor(0.92f, 0.87f, 0.72f));
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(Title))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
		}
		UHorizontalBox* SortRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(SortRow)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		SortRow->AddChildToHorizontalBox(MakeButton(WidgetTree, TEXT("기본 정렬"), 10, SectionColor));
		AttackTotalText = MakeText(WidgetTree, TEXT(""), 11, UiGreen);
		if (UHorizontalBoxSlot* S = SortRow->AddChildToHorizontalBox(AttackTotalText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}
		EffectList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(EffectList)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// ── 스킬 선택 창 (평소 숨김) ──
		Picker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Picker->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Picker->SetHorizontalAlignment(HAlign_Center);
		Picker->SetVerticalAlignment(VAlign_Center);
		Picker->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Root->AddChildToOverlay(Picker))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		UBorder* PickerBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PickerBox->SetBrushColor(PanelColor);
		PickerBox->SetPadding(FMargin(14.f));
		Picker->SetContent(PickerBox);
		UVerticalBox* PickerColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PickerBox->SetContent(PickerColumn);
		PickerTitle = MakeText(WidgetTree, TEXT(""), 12, UiGold);
		if (UVerticalBoxSlot* S = PickerColumn->AddChildToVerticalBox(PickerTitle)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		PickerList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PickerColumn->AddChildToVerticalBox(PickerList);
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

void UGemPageWidget::SetSources(UGemComponent* InGems, UInventoryComponent* InInventory, USkillManagerComponent* InSkills)
{
	if (Gems.Get() == InGems && Inventory.Get() == InInventory && Skills.Get() == InSkills) return;
	Unbind();
	Gems = InGems;
	Inventory = InInventory;
	Skills = InSkills;
	if (InGems) GemsHandle = InGems->OnGemsChanged.AddUObject(this, &UGemPageWidget::HandleChanged);
	Refresh();
}

void UGemPageWidget::Unbind()
{
	if (UGemComponent* G = Gems.Get()) G->OnGemsChanged.Remove(GemsHandle);
	GemsHandle.Reset();
}

void UGemPageWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

TArray<FName> UGemPageWidget::GetSkillRows() const
{
	TArray<FName> Rows;
	if (const USkillManagerComponent* S = Skills.Get())
	{
		for (const FName& Row : S->GetAllSkillRowNames())
		{
			if (S->IsSkillTreeRow(Row)) Rows.Add(Row);
		}
	}
	return Rows;
}

FString UGemPageWidget::GetSkillName(FName Row) const
{
	const USkillManagerComponent* S = Skills.Get();
	const FSkillData* Data = (S && S->SkillDataTable) ? S->SkillDataTable->FindRow<FSkillData>(Row, TEXT("GemPage"), false) : nullptr;
	return (Data && !Data->SkillName.IsEmpty()) ? Data->SkillName.ToString() : Row.ToString();
}

UTexture2D* UGemPageWidget::GetSkillIcon(FName Row) const
{
	const USkillManagerComponent* S = Skills.Get();
	const FSkillData* Data = (S && S->SkillDataTable) ? S->SkillDataTable->FindRow<FSkillData>(Row, TEXT("GemPage"), false) : nullptr;
	return Data ? Data->Icon.LoadSynchronous() : nullptr;
}

void UGemPageWidget::Refresh()
{
	if (!Board || !EffectList) return;  // 트리가 아직 없음 — RebuildWidget 끝에서 다시 불린다
	Targets.Reset();
	Board->ClearChildren();
	EffectList->ClearChildren();

	const UGemComponent* G = Gems.Get();
	if (!G) return;

	float AttackTotal = 0.f;
	const TArray<FGemSlot>& Slots = G->GetSlots();
	for (int32 i = 0; i < Slots.Num() && i < LoAGems::SlotCount; ++i)
	{
		EGemType Type;
		int32 Level = 0;
		const bool bHas = LoAGems::ParseGemId(Slots[i].GemItemId, Type, Level);
		const FItemDefinition* Def = bHas ? LoAItems::Find(Slots[i].GemItemId) : nullptr;
		UTexture2D* Icon = Def ? Def->Icon.LoadSynchronous() : nullptr;

		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		USizeBox* Ring = MakeRingIcon(WidgetTree, Gem_Circle, bHas ? (Type == EGemType::Damage ? DamageGem : CooldownGem) : RingIdle, Icon);
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Ring)) S->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* LevelText = MakeText(WidgetTree, bHas ? FString::Printf(TEXT("Lv.%d"), Level) : TEXT(""), 10, UiLabel);
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(LevelText)) S->SetHorizontalAlignment(HAlign_Center);
		if (bHas)
		{
			Ring->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s\n%s\n\n%s\n\n우클릭: 빼기"),
				*LoAGems::GetGemName(Type, Level).ToString(), *GetSkillName(Slots[i].SkillRowName), *LoAGems::DescribeGem(Type, Level))));
		}
		PlaceAt(Board, Cell, Gem_Positions[i] + FVector2D(0.f, 10.f), FVector2D(Gem_Circle + 20.f, Gem_Circle + 20.f));
		Targets.Add({ Ring, EHit::GemSlot, i, NAME_None });

		if (!bHas) continue;

		// 오른쪽 효과 줄 — [아이콘] 스킬 이름 / "<스킬> 피해 24.00% 증가" / 추가 효과 기본 공격력
		const float Atk = LoAGems::GetAttackPowerPercent(Level);
		AttackTotal += Atk;
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetBrushColor(RowColor);
		Row->SetPadding(FMargin(6.f, 5.f));
		UVerticalBox* RowColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Row->SetContent(RowColumn);
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RowColumn->AddChildToVerticalBox(Top);
		USizeBox* SkillIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SkillIconBox->SetWidthOverride(40.f);
		SkillIconBox->SetHeightOverride(40.f);
		if (UTexture2D* SkillIcon = GetSkillIcon(Slots[i].SkillRowName))
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Image->SetBrushFromTexture(SkillIcon);
			SkillIconBox->SetContent(Image);
		}
		Top->AddChildToHorizontalBox(SkillIconBox);
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(Texts))
		{
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		const FString SkillName = GetSkillName(Slots[i].SkillRowName);
		Texts->AddChildToVerticalBox(MakeText(WidgetTree, SkillName, 11, FLinearColor(0.75f, 0.85f, 1.f)));
		UHorizontalBox* EffectLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Texts->AddChildToVerticalBox(EffectLine);
		EffectLine->AddChildToHorizontalBox(MakeText(WidgetTree, SkillName, 10, SkillYellow));
		EffectLine->AddChildToHorizontalBox(MakeText(WidgetTree, Type == EGemType::Damage
			? FString::Printf(TEXT(" 피해 %.2f%% 증가"), LoAGems::GetEffect(Type, Level) * 100.f)
			: FString::Printf(TEXT(" 재사용 대기시간 %.2f%% 감소"), LoAGems::GetEffect(Type, Level) * 100.f), 10, UiLabel));
		if (Atk > 0.f)
		{
			UHorizontalBox* Extra = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UVerticalBoxSlot* S = RowColumn->AddChildToVerticalBox(Extra)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			Extra->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("추가 효과  "), 10, UiLabel));
			Extra->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("기본 공격력 %.2f%% 증가"), Atk * 100.f), 10, UiGreen));
		}
		if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(EffectList->AddChild(Row))) S->SetPadding(FMargin(0.f, 0.f, 6.f, 4.f));
	}
	if (AttackTotalText) AttackTotalText->SetText(FText::FromString(FString::Printf(TEXT("기본 공격력 총합 : %.2f%%"), AttackTotal * 100.f)));
	RefreshPicker();
}

void UGemPageWidget::BeginEquip(FName GemItemId)
{
	EGemType Type;
	int32 Level;
	if (!LoAGems::ParseGemId(GemItemId, Type, Level)) return;
	PendingGem = GemItemId;
	RefreshPicker();
}

void UGemPageWidget::RefreshPicker()
{
	if (!Picker || !PickerList) return;
	PickerTargets.Reset();
	PickerList->ClearChildren();
	EGemType Type;
	int32 Level;
	const UGemComponent* G = Gems.Get();
	if (PendingGem.IsNone() || !G || !LoAGems::ParseGemId(PendingGem, Type, Level))
	{
		Picker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Picker->SetVisibility(ESlateVisibility::Visible);
	PickerTitle->SetText(FText::FromString(FString::Printf(TEXT("%s — 장착할 스킬 선택"), *LoAGems::GetGemName(Type, Level).ToString())));
	for (const FName& Row : GetSkillRows())
	{
		const int32 Current = G->GetGemLevelOnSkill(Row, Type);
		const FString Text = Current > 0
			? FString::Printf(TEXT("%s   (지금 %s Lv.%d → 바꿔 끼움)"), *GetSkillName(Row), Type == EGemType::Damage ? TEXT("겁화") : TEXT("작열"), Current)
			: GetSkillName(Row);
		UBorder* Btn = MakeButton(WidgetTree, Text, 11, LoABuildUI::ButtonColor);
		if (UVerticalBoxSlot* S = PickerList->AddChildToVerticalBox(Btn))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
		}
		PickerTargets.Add({ Btn, EHit::PickSkill, 0, Row });
	}
	UBorder* Cancel = MakeButton(WidgetTree, TEXT("취소"), 11, SectionColor);
	if (UVerticalBoxSlot* S = PickerList->AddChildToVerticalBox(Cancel))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	PickerTargets.Add({ Cancel, EHit::PickCancel, 0, NAME_None });
}

void UGemPageWidget::Notify(const FText& Message) const
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(UiGold));
	}
}

FReply UGemPageWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

	UGemComponent* G = Gems.Get();
	FText Fail;

	// 스킬 선택 창이 떠 있으면 그것만 받는다
	if (!PendingGem.IsNone())
	{
		const FHitTarget* Hit = Find(PickerTargets);
		if (bLeft && Hit)
		{
			const FHitTarget Target = *Hit;
			const FName Gem = PendingGem;
			PendingGem = NAME_None;
			if (Target.Kind == EHit::PickSkill && G)
			{
				if (G->EquipGem(Gem, Target.Id, Fail)) Notify(FText::FromString(FString::Printf(TEXT("%s에 장착했습니다."), *GetSkillName(Target.Id))));
				else if (!Fail.IsEmpty()) Notify(Fail);
			}
			RefreshPicker();
		}
		return FReply::Handled();
	}

	const FHitTarget* Hit = Find(Targets);
	if (!Hit || !G) return FReply::Handled();
	const FHitTarget Target = *Hit;
	if (bRight && Target.Kind == EHit::GemSlot)
	{
		if (G->UnequipGem(Target.Index, Fail)) Notify(NSLOCTEXT("Gem", "Unequipped", "보석을 소지품으로 뺐습니다."));
		else if (!Fail.IsEmpty()) Notify(Fail);
	}
	return FReply::Handled();
}

FReply UGemPageWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 두 번째 클릭은 DoubleClick으로 온다 — 안 먹으면 맵 클릭 이동이 샌다. 빼기를 두 번 하지 않게 처리는 안 함
	return FReply::Handled();
}
