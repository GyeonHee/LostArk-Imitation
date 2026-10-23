#include "UI/SkillTooltipWidget.h"
#include "UI/BuildUIHelpers.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Stat/StatComponent.h"
#include "Stat/LoADamageLibrary.h"
#include "Item/GemComponent.h"
#include "Item/ItemTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"

using namespace LoABuildUI;

namespace
{
	constexpr float Tip_Width = 300.f;
	const FLinearColor Tip_Title(0.95f, 0.92f, 0.82f, 1.f);
	const FLinearColor Tip_Number(0.55f, 0.95f, 0.35f, 1.f);    // 피해 수치 (로아 초록)
	const FLinearColor Tip_Blue(0.55f, 0.75f, 1.f, 1.f);

	// 설명 문구 — {D} = 피해 수치. 로아 소서리스 스킬 설명 말투
	const TCHAR* Tip_Description(FName Row)
	{
		const FString R = Row.ToString();
		if (R == TEXT("블레이즈"))     return TEXT("전방으로 화염을 내뿜어 지나간 자리에 불길을 남긴다. 불길 위의 적은 총 {D}의 피해를 받는다.");
		if (R == TEXT("돌풍"))        return TEXT("커서 방향으로 몸을 돌려 전방에 회오리바람을 일으켜 {D}의 피해를 준다.");
		if (R == TEXT("인페르노"))     return TEXT("지정한 위치의 땅에서 화염이 솟구쳐 올라 {D}의 피해를 준다.");
		if (R == TEXT("아이스에로우"))  return TEXT("지정한 위치에 얼음 화살을 4번 쏟아부어 총 {D}의 피해를 준다.");
		if (R == TEXT("혹한의부름"))    return TEXT("키를 누르고 있는 동안 지정한 위치에 눈보라와 고드름을 불러 최대 {D}의 피해를 준다.");
		if (R == TEXT("익스플로전"))    return TEXT("마력을 모아 전방으로 운석을 날려, 떨어진 자리에 폭발을 일으켜 {D}의 피해를 준다.");
		if (R == TEXT("천벌"))        return TEXT("마력을 모아 지정한 위치에 거대한 낙뢰를 떨어뜨려 {D}의 피해를 준다.");
		if (R == TEXT("종말의날"))     return TEXT("마력을 모아 지정한 위치에 거대한 운석을 떨어뜨려 {D}의 피해를 준다.");
		if (R == TEXT("BasicAttack")) return TEXT("마력 구체를 날려 {D}의 피해를 준다.");
		if (R == TEXT("Dash"))        return TEXT("커서 방향으로 빠르게 이동한다.");
		if (R == TEXT("InstantGetUp")) return TEXT("넉다운 상태에서 즉시 일어난다.");
		return TEXT("");
	}

	// DT 이름이 영문인 행 — 툴팁에선 한국어로
	FString Tip_DisplayName(FName Row, const FSkillData* Data)
	{
		const FString R = Row.ToString();
		if (R == TEXT("Dash")) return TEXT("이동기");
		if (R == TEXT("InstantGetUp")) return TEXT("기상기");
		return (Data && !Data->SkillName.IsEmpty()) ? Data->SkillName.ToString() : R;
	}

	FString Tip_Seconds(float S)
	{
		return FMath::IsNearlyEqual(S, FMath::RoundToFloat(S), 0.01f) ? FString::Printf(TEXT("%.0f초"), S) : FString::Printf(TEXT("%.1f초"), S);
	}
}

TSharedRef<SWidget> USkillTooltipWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SkillTooltipRoot"));
		WidgetTree->RootWidget = Root;

		// 금테 1px + 거의 검은 남색 바탕 (로아 툴팁)
		UBorder* TipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SkillTooltipFrame"));
		TipFrame->SetBrushColor(LoABuildUI::UiFrame);
		TipFrame->SetPadding(FMargin(1.f));
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(TipFrame))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(0.f, 0.f));
		}
		Panel = TipFrame;
		UBorder* Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Back->SetBrushColor(FLinearColor(0.025f, 0.03f, 0.042f, 0.97f));
		Back->SetPadding(FMargin(12.f, 10.f));
		TipFrame->SetContent(Back);
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetWidthOverride(Tip_Width);
		Back->SetContent(Width);
		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Width->SetContent(Content);
	}
	SetVisibility(ESlateVisibility::Collapsed);
	return Super::RebuildWidget();
}

void USkillTooltipWidget::ShowSkill(FName RowName, USkillManagerComponent* Skills)
{
	if (!Content) TakeWidget();
	// 다른 스킬로 옮겼을 때만 새로 — 같은 스킬 위에 있는 동안은 아무것도 안 바꾼다
	if (RowName != ShownRow || Skills != ShownSkills.Get())
	{
		ShownRow = RowName;
		ShownSkills = Skills;
		Build(RowName, Skills);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USkillTooltipWidget::HideTooltip()
{
	ShownRow = NAME_None;
	SetVisibility(ESlateVisibility::Collapsed);
}

void USkillTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UCanvasPanelSlot* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
	if (!PanelSlot || !FSlateApplication::IsInitialized()) return;

	// 커서 오른쪽 아래 — 화면 밖으로 나가면 왼쪽/위로 뒤집는다
	const FVector2D CursorPos = MyGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	const FVector2D Screen = MyGeometry.GetLocalSize();
	// 내용이 방금 바뀌었어도 이번 프레임의 크기로 자리를 잡게
	Panel->ForceLayoutPrepass();
	const FVector2D Size = Panel->GetDesiredSize();
	FVector2D Pos = CursorPos + FVector2D(18.f, 18.f);
	if (Pos.X + Size.X > Screen.X) Pos.X = CursorPos.X - Size.X - 12.f;
	if (Pos.Y + Size.Y > Screen.Y) Pos.Y = FMath::Max(0.f, Screen.Y - Size.Y);
	PanelSlot->SetPosition(Pos);
}

void USkillTooltipWidget::Build(FName RowName, USkillManagerComponent* Skills)
{
	Content->ClearChildren();
	const FSkillData* Data = (Skills && Skills->SkillDataTable) ? Skills->SkillDataTable->FindRow<FSkillData>(RowName, TEXT("SkillTooltip"), false) : nullptr;
	if (!Data) return;
	AActor* Owner = Skills->GetOwner();
	const UGemComponent* Gems = Owner ? Owner->FindComponentByClass<UGemComponent>() : nullptr;

	auto AddRow = [this](UWidget* W, float Top = 0.f)
	{
		if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(W)) S->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	};
	auto Divider = [this, &AddRow]()
	{
		UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Line->SetBrushColor(FLinearColor(0.35f, 0.32f, 0.25f, 0.6f));
		Line->SetPadding(FMargin(0.f, 0.5f));
		AddRow(Line, 8.f);
	};

	// ── 이름 ──
	UTextBlock* Title = MakeText(WidgetTree, Tip_DisplayName(RowName, Data), 15, Tip_Title);
	if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(Title)) S->SetHorizontalAlignment(HAlign_Center);
	Divider();

	// ── [아이콘] 종류 / 재사용 대기시간 ──
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddRow(Head, 8.f);
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(60.f);
	IconBox->SetHeightOverride(60.f);
	UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconFrame->SetBrushColor(LoABuildUI::UiFrame);
	IconFrame->SetPadding(FMargin(1.f));
	IconBox->SetContent(IconFrame);
	if (UTexture2D* Icon = Data->Icon.LoadSynchronous())
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrushFromTexture(Icon);
		IconFrame->SetContent(Image);
	}
	Head->AddChildToHorizontalBox(IconBox);
	UVerticalBox* HeadText = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(HeadText))
	{
		S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	const FString TypeLabel = Data->SkillTypeLabel.IsEmpty() ? TEXT("일반") : Data->SkillTypeLabel.ToString();
	HeadText->AddChildToVerticalBox(MakeText(WidgetTree, TypeLabel, 12, UiGold));
	HeadText->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(TEXT("[%s 스킬]"), *TypeLabel), 10, UiLabel));
	if (Data->Cooldown > 0.f)
	{
		HeadText->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(TEXT("재사용 대기시간 %s"), *Tip_Seconds(Data->Cooldown)), 10, UiLabel));
	}

	// ── 스킬 레벨 · 마나 ──
	const bool bTreeSkill = Skills->IsSkillTreeRow(RowName);
	if (bTreeSkill) AddRow(MakeText(WidgetTree, FString::Printf(TEXT("스킬 레벨 %d"), FMath::Max(1, Skills->GetSkillLevel(RowName))), 11, Tip_Title), 10.f);
	if (Data->ManaCost > 0.f) AddRow(MakeText(WidgetTree, FString::Printf(TEXT("마나 %s 소모"), *LoAItems::FormatWithCommas(FMath::RoundToInt64(Data->ManaCost))), 11, Tip_Blue), 2.f);

	// ── 설명 (피해 수치는 툴팁을 띄운 순간의 스펙 기준, 치명 전) ──
	const FString Template = Tip_Description(RowName);
	if (!Template.IsEmpty())
	{
		const FString Damage = LoAItems::FormatWithCommas(FMath::RoundToInt64(ULoADamageLibrary::CalcOutgoingDamage(Owner, Data->DamageCoefficient, RowName)));
		UTextBlock* DescText = MakeText(WidgetTree, Template.Replace(TEXT("{D}"), *Damage), 11, UiLabel);
		DescText->SetAutoWrapText(true);
		AddRow(DescText, 10.f);
		if (Template.Contains(TEXT("{D}")))
		{
			UHorizontalBox* DamageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			DamageRow->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("피해량  "), 10, UiDim));
			DamageRow->AddChildToHorizontalBox(MakeText(WidgetTree, Damage, 11, Tip_Number));
			AddRow(DamageRow, 4.f);
		}
	}

	// ── 부가 정보 (기본값) ──
	if (Data->InputType == ESkillInputType::Cast && Data->CastTime > 0.f)
	{
		AddRow(MakeText(WidgetTree, FString::Printf(TEXT("캐스팅 시간 %s"), *Tip_Seconds(Data->CastTime)), 10, UiLabel), 8.f);
	}
	else if (Data->InputType == ESkillInputType::Charge && Data->ChargeMaxTime > 0.f)
	{
		AddRow(MakeText(WidgetTree, FString::Printf(TEXT("차지 시간 %s"), *Tip_Seconds(Data->ChargeMaxTime)), 10, UiLabel), 8.f);
	}
	else if (Data->InputType == ESkillInputType::Hold && Data->HoldMaxTime > 0.f)
	{
		AddRow(MakeText(WidgetTree, FString::Printf(TEXT("홀딩 최대 %s"), *Tip_Seconds(Data->HoldMaxTime)), 10, UiLabel), 8.f);
	}
	if (Data->bCanCounter)
	{
		UHorizontalBox* Counter = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Counter->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("카운터 : "), 10, UiLabel));
		Counter->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("가능"), 10, SkillYellow));
		AddRow(Counter, 4.f);
	}

	// ── 장착 보석 ──
	if (Gems && bTreeSkill)
	{
		bool bAnyGem = false;
		for (const EGemType Type : { EGemType::Damage, EGemType::Cooldown })
		{
			const int32 Level = Gems->GetGemLevelOnSkill(RowName, Type);
			if (Level <= 0) continue;
			if (!bAnyGem) Divider();
			bAnyGem = true;
			UHorizontalBox* GemRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			const FItemDefinition* GemDef = LoAItems::Find(LoAGems::MakeGemId(Type, Level));
			GemRow->AddChildToHorizontalBox(MakeRingIcon(WidgetTree, 30.f, Type == EGemType::Damage ? DamageGem : CooldownGem, GemDef ? GemDef->Icon.LoadSynchronous() : nullptr, 2.f));
			UVerticalBox* GemText = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			if (UHorizontalBoxSlot* S = GemRow->AddChildToHorizontalBox(GemText))
			{
				S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
			GemText->AddChildToVerticalBox(MakeText(WidgetTree, LoAGems::GetGemName(Type, Level).ToString(), 10, UiOrange));
			GemText->AddChildToVerticalBox(MakeText(WidgetTree, Type == EGemType::Damage
				? FString::Printf(TEXT("피해 %.2f%% 증가"), LoAGems::GetEffect(Type, Level) * 100.f)
				: FString::Printf(TEXT("재사용 대기시간 %.2f%% 감소"), LoAGems::GetEffect(Type, Level) * 100.f), 10, UiLabel));
			AddRow(GemRow, 6.f);
		}
	}
}
