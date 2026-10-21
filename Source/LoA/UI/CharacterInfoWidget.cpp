#include "UI/CharacterInfoWidget.h"
#include "Stat/StatComponent.h"
#include "Item/EquipmentComponent.h"
#include "Item/ItemTypes.h"
#include "Engine/Texture2D.h"
#include "UI/CharacterPreviewActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LoAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// 로아 캐릭터 정보 창 느낌 — 거의 검은 남색 바탕 + 금색 글자/테두리
	const FLinearColor CharInfo_PanelColor(0.028f, 0.032f, 0.045f, 0.97f);
	const FLinearColor CharInfo_SectionColor(0.055f, 0.06f, 0.08f, 1.f);
	const FLinearColor CharInfo_Gold(0.93f, 0.80f, 0.52f, 1.f);
	const FLinearColor CharInfo_FrameColor(0.42f, 0.36f, 0.22f, 1.f);
	const FLinearColor CharInfo_LabelColor(0.80f, 0.80f, 0.82f, 1.f);
	const FLinearColor CharInfo_ValueColor(0.96f, 0.96f, 0.96f, 1.f);
	const FLinearColor CharInfo_DimColor(0.45f, 0.45f, 0.48f, 1.f);

	constexpr float CharInfo_SlotSize = 50.f;
	constexpr float CharInfo_PanelWidth = 1000.f;
	constexpr float CharInfo_BodyHeight = 540.f;
	constexpr float CharInfo_PreviewHeight = 516.f; // 본문 높이 - 장비 영역 안쪽 여백(12 × 2)
	constexpr float CharInfo_PreviewRotateSpeed = 0.6f; // 1px 끌 때 몇 도

	void CharInfo_FlatButtonStyle(UButton* Button, float NormalAlpha)
	{
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, NormalAlpha));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, NormalAlpha + 0.12f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, NormalAlpha + 0.25f));
		Style.NormalPadding = FMargin(4.f);
		Style.PressedPadding = FMargin(4.f);
		Button->SetStyle(Style);
	}

	FString CharInfo_Percent(float Ratio)
	{
		return FString::Printf(TEXT("%.2f%%"), Ratio * 100.f);
	}
}

// ─── 트리 조립 헬퍼 ────────────────────────────────────────────────────────

UTextBlock* UCharacterInfoWidget::MakeText(const FString& Text, int32 Size, const FLinearColor& Color, FName Name)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Block->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

UWidget* UCharacterInfoWidget::MakeEquipSlot(const FText& PartName, FName Name, FGearSlotWidgets* OutWidgets)
{
	// 금색 테두리(1px) 안에 어두운 칸, 비어 있으면 부위 이름을 흐리게 — 장비(3단계)가 생기면 여기에 아이콘이 들어간다
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
	Box->SetWidthOverride(CharInfo_SlotSize);
	Box->SetHeightOverride(CharInfo_SlotSize);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(CharInfo_FrameColor);
	Frame->SetPadding(FMargin(1.f));
	Frame->SetToolTipText(FText::Format(NSLOCTEXT("CharInfo", "EmptySlotTip", "{0}\n미착용"), PartName));
	Box->SetContent(Frame);

	UBorder* Inner = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Inner->SetBrushColor(FLinearColor(0.07f, 0.07f, 0.09f, 1.f));
	Inner->SetHorizontalAlignment(HAlign_Center);
	Inner->SetVerticalAlignment(VAlign_Center);
	Frame->SetContent(Inner);

	// 부위 이름(빈 칸일 때) / 아이콘 / 왼쪽 위 +재련 — 겹쳐 두고 Refresh가 보이고 숨긴다
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Inner->SetHorizontalAlignment(HAlign_Fill);
	Inner->SetVerticalAlignment(VAlign_Fill);
	Inner->SetContent(Layers);

	UTextBlock* PartText = MakeText(PartName.ToString(), 8, CharInfo_DimColor);
	if (UOverlaySlot* S = Layers->AddChildToOverlay(PartText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Icon->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Layers->AddChildToOverlay(Icon))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UTextBlock* RefineText = MakeText(TEXT(""), 9, CharInfo_Gold);
	RefineText->SetShadowOffset(FVector2D(1.f, 1.f));
	RefineText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
	RefineText->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Layers->AddChildToOverlay(RefineText))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(2.f, 0.f));
	}

	if (OutWidgets)
	{
		OutWidgets->Box = Box;
		OutWidgets->Frame = Frame;
		OutWidgets->Icon = Icon;
		OutWidgets->PartText = PartText;
		OutWidgets->RefineText = RefineText;
		OutWidgets->PartName = PartName;
	}
	return Box;
}

UWidget* UCharacterInfoWidget::MakeSectionTitle(const FText& Title)
{
	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bar->SetBrushColor(FLinearColor(0.08f, 0.085f, 0.11f, 1.f));
	Bar->SetPadding(FMargin(6.f, 4.f));
	Bar->SetHorizontalAlignment(HAlign_Center);
	Bar->SetContent(MakeText(Title.ToString(), 11, FLinearColor(0.85f, 0.85f, 0.88f)));
	return Bar;
}

UWidget* UCharacterInfoWidget::MakeStatRow(const FText& Label, UTextBlock*& OutValue, FName Name)
{
	// 줄 전체가 툴팁을 받도록 Border(히트 테스트 됨)로 감싼다 — TextBlock은 기본이 히트 테스트 안 됨
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Row->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	Row->SetPadding(FMargin(10.f, 4.f));

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(Line);

	if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(MakeText(Label.ToString(), 11, CharInfo_Gold)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}

	OutValue = MakeText(TEXT("0"), 11, CharInfo_ValueColor);
	if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(OutValue))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	return Row;
}

UWidget* UCharacterInfoWidget::BuildProfileColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ProfileColumn"));

	// ── 프로필: 직업 문장(원) + 직업 이름 + 주 스탯 ──
	UBorder* Profile = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfileBox"));
	Profile->SetBrushColor(CharInfo_SectionColor);
	Profile->SetPadding(FMargin(10.f, 18.f));
	Profile->SetHorizontalAlignment(HAlign_Center);
	Column->AddChildToVerticalBox(Profile);

	UVerticalBox* ProfileLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Profile->SetContent(ProfileLines);

	USizeBox* EmblemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EmblemBox->SetWidthOverride(84.f);
	EmblemBox->SetHeightOverride(84.f);
	UBorder* Emblem = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ClassEmblem"));
	Emblem->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.20f, 0.12f, 0.30f, 1.f), 42.f, CharInfo_Gold, 2.f));
	Emblem->SetHorizontalAlignment(HAlign_Center);
	Emblem->SetVerticalAlignment(VAlign_Center);
	Emblem->SetContent(MakeText(TEXT("마법사"), 11, CharInfo_Gold));
	EmblemBox->SetContent(Emblem);
	if (UVerticalBoxSlot* S = ProfileLines->AddChildToVerticalBox(EmblemBox))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	if (UVerticalBoxSlot* S = ProfileLines->AddChildToVerticalBox(MakeText(TEXT("소서리스"), 16, CharInfo_ValueColor, TEXT("ClassName"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
	if (UVerticalBoxSlot* S = ProfileLines->AddChildToVerticalBox(MakeText(TEXT("주 스탯 · 지능"), 10, CharInfo_Gold, TEXT("PrimaryStatLabel"))))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	// 남는 공간
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// ── 특수장비 (나침반·부적·문장) — 로아 배치 그대로 자리만 ──
	UBorder* Special = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SpecialEquipBox"));
	Special->SetBrushColor(CharInfo_SectionColor);
	Special->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	Column->AddChildToVerticalBox(Special);

	UVerticalBox* SpecialLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Special->SetContent(SpecialLines);
	if (UVerticalBoxSlot* S = SpecialLines->AddChildToVerticalBox(MakeSectionTitle(NSLOCTEXT("CharInfo", "Special", "특수장비"))))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	UHorizontalBox* SpecialRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = SpecialLines->AddChildToVerticalBox(SpecialRow))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}
	const FText SpecialNames[] = {
		NSLOCTEXT("CharInfo", "Compass", "나침반"),
		NSLOCTEXT("CharInfo", "Charm", "부적"),
		NSLOCTEXT("CharInfo", "Crest", "문장"),
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(SpecialNames); ++i)
	{
		if (UHorizontalBoxSlot* S = SpecialRow->AddChildToHorizontalBox(MakeEquipSlot(SpecialNames[i], FName(*FString::Printf(TEXT("SpecialSlot_%d"), i)))))
		{
			S->SetPadding(FMargin(6.f, 0.f));
		}
	}
	return Column;
}

UWidget* UCharacterInfoWidget::BuildEquipmentArea()
{
	UBorder* Area = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EquipmentArea"));
	Area->SetBrushColor(FLinearColor(0.018f, 0.02f, 0.03f, 1.f));
	Area->SetPadding(FMargin(12.f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Area->SetContent(Row);

	// 세로 칸 줄 하나 — 금색 테두리 띠 안에 칸들을 쌓는다
	auto MakeSlotColumn = [this](const TArray<FText>& Parts, const TCHAR* Prefix, bool bGear) -> UWidget*
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(FLinearColor(0.10f, 0.09f, 0.065f, 1.f));
		Frame->SetPadding(FMargin(4.f));

		UVerticalBox* Slots = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Slots);
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			FGearSlotWidgets* Out = bGear ? &GearSlots.AddDefaulted_GetRef() : nullptr;
			if (UVerticalBoxSlot* S = Slots->AddChildToVerticalBox(MakeEquipSlot(Parts[i], FName(*FString::Printf(TEXT("%s_%d"), Prefix, i)), Out)))
			{
				S->SetPadding(FMargin(0.f, i == 0 ? 0.f : 6.f, 0.f, 0.f));
			}
		}
		return Frame;
	};

	// 왼쪽 줄: 방어구 5부위 + 무기 (장착 아이템 레벨 = 이 6부위 평균)
	const TArray<FText> GearParts = {
		NSLOCTEXT("CharInfo", "Helmet", "투구"),
		NSLOCTEXT("CharInfo", "Shoulder", "어깨"),
		NSLOCTEXT("CharInfo", "Chest", "상의"),
		NSLOCTEXT("CharInfo", "Pants", "하의"),
		NSLOCTEXT("CharInfo", "Gloves", "장갑"),
		NSLOCTEXT("CharInfo", "Weapon", "무기"),
	};
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeSlotColumn(GearParts, TEXT("GearSlot"), true)))
	{
		S->SetVerticalAlignment(VAlign_Top);
	}

	// 가운데: 3D 캐릭터 미리보기 (렌더 타깃 2:3, 끌어서 회전) — 배경색은 미리보기 액터의 배경판과 맞춤.
	// 툴팁은 안 단다 — 캐릭터 얼굴 위에 떠서 가렸음
	PreviewArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CharacterPreview"));
	PreviewArea->SetBrushColor(FLinearColor::Black);
	PreviewArea->SetPadding(FMargin(0.f));
	PreviewArea->SetHorizontalAlignment(HAlign_Center);
	PreviewArea->SetVerticalAlignment(VAlign_Center);

	PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CharacterPreviewImage"));
	PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	PreviewImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f)); // 렌더 타깃이 붙기 전엔 숨김
	FSlateBrush PreviewBrush;
	PreviewBrush.ImageSize = FVector2D(CharInfo_PreviewHeight * 2.f / 3.f, CharInfo_PreviewHeight);
	PreviewImage->SetBrush(PreviewBrush);
	PreviewArea->SetContent(PreviewImage);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PreviewArea))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetPadding(FMargin(12.f, 0.f));
	}

	// 오른쪽 줄: 장신구 5 + 팔찌 + 어빌리티 스톤
	const TArray<FText> AccessoryParts = {
		NSLOCTEXT("CharInfo", "Necklace", "목걸이"),
		NSLOCTEXT("CharInfo", "Earring1", "귀걸이"),
		NSLOCTEXT("CharInfo", "Earring2", "귀걸이"),
		NSLOCTEXT("CharInfo", "Ring1", "반지"),
		NSLOCTEXT("CharInfo", "Ring2", "반지"),
		NSLOCTEXT("CharInfo", "Bracelet", "팔찌"),
		NSLOCTEXT("CharInfo", "Stone", "스톤"),
	};
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeSlotColumn(AccessoryParts, TEXT("AccessorySlot"), false)))
	{
		S->SetVerticalAlignment(VAlign_Top);
	}
	return Area;
}

UWidget* UCharacterInfoWidget::BuildStatColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatColumn"));

	// ── 장착 아이템 레벨 — 금색 테두리 + 남색 바탕, 큰 숫자 ──
	UBorder* LevelFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ItemLevelFrame"));
	LevelFrame->SetBrushColor(CharInfo_Gold);
	LevelFrame->SetPadding(FMargin(1.5f));
	Column->AddChildToVerticalBox(LevelFrame);

	UBorder* LevelBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ItemLevelBox"));
	LevelBox->SetBrushColor(FLinearColor(0.04f, 0.07f, 0.17f, 1.f));
	LevelBox->SetPadding(FMargin(8.f, 8.f, 8.f, 10.f));
	LevelBox->SetHorizontalAlignment(HAlign_Center);
	LevelFrame->SetContent(LevelBox);

	UVerticalBox* LevelLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LevelBox->SetContent(LevelLines);
	if (UVerticalBoxSlot* S = LevelLines->AddChildToVerticalBox(MakeText(TEXT("장착 아이템 레벨"), 11, CharInfo_Gold)))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	UHorizontalBox* Number = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* S = LevelLines->AddChildToVerticalBox(Number))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));
	}
	ItemLevelText = MakeText(TEXT("0"), 30, FLinearColor(0.92f, 0.95f, 1.f), TEXT("ItemLevelText"));
	if (UHorizontalBoxSlot* S = Number->AddChildToHorizontalBox(ItemLevelText))
	{
		S->SetVerticalAlignment(VAlign_Bottom);
	}
	ItemLevelFractionText = MakeText(TEXT(".00"), 18, FLinearColor(0.92f, 0.95f, 1.f), TEXT("ItemLevelFraction"));
	if (UHorizontalBoxSlot* S = Number->AddChildToHorizontalBox(ItemLevelFractionText))
	{
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	ItemLevelNoteText = MakeText(TEXT(""), 9, FLinearColor(0.60f, 0.65f, 0.78f), TEXT("ItemLevelNote"));
	if (UVerticalBoxSlot* S = LevelLines->AddChildToVerticalBox(ItemLevelNoteText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// ── 기본 특성 ──
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MakeSectionTitle(NSLOCTEXT("CharInfo", "BaseStats", "기본 특성"))))
	{
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 2.f));
	}
	UTextBlock* Value = nullptr;
	AttackPowerRow = MakeStatRow(NSLOCTEXT("CharInfo", "AttackPower", "공격력"), Value, TEXT("AttackPowerRow"));
	AttackPowerText = Value;
	Column->AddChildToVerticalBox(AttackPowerRow);
	Column->AddChildToVerticalBox(MakeStatRow(NSLOCTEXT("CharInfo", "MaxHP", "최대 생명력"), Value, TEXT("MaxHPRow")));
	MaxHPText = Value;

	// ── 전투 특성 — 로아 창 순서(치명·특화·제압·신속·인내·숙련), 값은 ELoACombatStat 순서로 보관 ──
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MakeSectionTitle(NSLOCTEXT("CharInfo", "CombatStats", "전투 특성"))))
	{
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 2.f));
	}

	const int32 StatCount = static_cast<int32>(ELoACombatStat::MAX);
	CombatStatTexts.SetNum(StatCount);
	CombatStatRows.SetNum(StatCount);

	const UEnum* StatEnum = StaticEnum<ELoACombatStat>();
	const ELoACombatStat DisplayOrder[] = {
		ELoACombatStat::Crit, ELoACombatStat::Specialization, ELoACombatStat::Domination,
		ELoACombatStat::Swiftness, ELoACombatStat::Endurance, ELoACombatStat::Expertise,
	};
	for (ELoACombatStat Stat : DisplayOrder)
	{
		const int32 Index = static_cast<int32>(Stat);
		UWidget* StatRow = MakeStatRow(StatEnum->GetDisplayNameTextByValue(Index), Value, FName(*FString::Printf(TEXT("CombatStatRow_%d"), Index)));
		CombatStatTexts[Index] = Value;
		CombatStatRows[Index] = StatRow;
		Column->AddChildToVerticalBox(StatRow);
	}

	// ── [상세정보] 펼침 칸 ──
	DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailBox"));
	DetailBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(DetailBox))
	{
		S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}
	DetailBox->AddChildToVerticalBox(MakeSectionTitle(NSLOCTEXT("CharInfo", "Detail", "상세 효과")));
	DetailText = MakeText(TEXT(""), 10, CharInfo_LabelColor, TEXT("DetailText"));
	if (UVerticalBoxSlot* S = DetailBox->AddChildToVerticalBox(DetailText))
	{
		S->SetPadding(FMargin(10.f, 6.f));
	}

	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass())))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UButton* DetailButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DetailButton"));
	CharInfo_FlatButtonStyle(DetailButton, 0.08f);
	DetailButton->OnClicked.AddDynamic(this, &UCharacterInfoWidget::HandleDetailClicked);
	DetailButtonText = MakeText(TEXT("상세정보"), 11, CharInfo_LabelColor, TEXT("DetailButtonText"));
	DetailButton->SetContent(DetailButtonText);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(DetailButton))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}
	return Column;
}

// ─── 위젯 ──────────────────────────────────────────────────────────────────

TSharedRef<SWidget> UCharacterInfoWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CharacterInfoRoot"));
		WidgetTree->RootWidget = Root;

		// 창 — 화면 가운데
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CharacterInfoPanel"));
		Panel->SetBrushColor(CharInfo_PanelColor);
		Panel->SetPadding(FMargin(10.f, 4.f, 10.f, 10.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetPosition(FVector2D::ZeroVector);
		}

		USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
		PanelSize->SetWidthOverride(CharInfo_PanelWidth);
		Panel->SetContent(PanelSize);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CharacterInfoColumn"));
		PanelSize->SetContent(Column);

		// ── 헤더: 가운데 제목, 오른쪽 닫기 ──
		HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HeaderBox"));
		HeaderBox->SetHeightOverride(32.f);
		Column->AddChildToVerticalBox(HeaderBox);

		UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Header"));
		HeaderBox->SetContent(Header);
		if (UOverlaySlot* S = Header->AddChildToOverlay(MakeText(TEXT("캐릭터 정보"), 14, FLinearColor(0.92f, 0.87f, 0.72f), TEXT("Title"))))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		CharInfo_FlatButtonStyle(CloseButton, 0.f);
		CloseButton->OnClicked.AddDynamic(this, &UCharacterInfoWidget::HandleCloseClicked);
		CloseButton->SetContent(MakeText(TEXT("X"), 13, FLinearColor(0.8f, 0.8f, 0.8f)));
		if (UOverlaySlot* S = Header->AddChildToOverlay(CloseButton))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeaderDivider"));
		Divider->SetBrushColor(CharInfo_FrameColor);
		Divider->SetPadding(FMargin(0.f, 0.5f));
		Column->AddChildToVerticalBox(Divider);

		// ── 탭: 장비(선택) / 각인 / 보석 — 각인·보석은 7단계에서 ──
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Tabs))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));
		}
		const FText TabNames[] = {
			NSLOCTEXT("CharInfo", "TabEquip", "장비"),
			NSLOCTEXT("CharInfo", "TabEngraving", "각인"),
			NSLOCTEXT("CharInfo", "TabGem", "보석"),
		};
		for (int32 i = 0; i < UE_ARRAY_COUNT(TabNames); ++i)
		{
			const bool bActive = i == 0;
			UBorder* Tab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*FString::Printf(TEXT("Tab_%d"), i)));
			Tab->SetBrushColor(bActive ? FLinearColor(0.20f, 0.15f, 0.08f, 1.f) : FLinearColor(0.05f, 0.055f, 0.07f, 1.f));
			Tab->SetPadding(FMargin(0.f, 6.f));
			Tab->SetHorizontalAlignment(HAlign_Center);
			Tab->SetContent(MakeText(TabNames[i].ToString(), 12, bActive ? CharInfo_Gold : CharInfo_DimColor));
			if (!bActive)
			{
				Tab->SetToolTipText(NSLOCTEXT("CharInfo", "TabSoon", "준비 중"));
			}
			if (UHorizontalBoxSlot* S = Tabs->AddChildToHorizontalBox(Tab))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetPadding(FMargin(i == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f));
			}
		}

		// ── 본문: 프로필 | 장비 칸 + 미리보기 | 아이템 레벨·특성 ──
		USizeBox* BodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BodySize"));
		BodySize->SetHeightOverride(CharInfo_BodyHeight);
		Column->AddChildToVerticalBox(BodySize);

		UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
		BodySize->SetContent(Body);

		USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LeftSize"));
		LeftSize->SetWidthOverride(220.f);
		LeftSize->SetContent(BuildProfileColumn());
		Body->AddChildToHorizontalBox(LeftSize);

		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(BuildEquipmentArea()))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(10.f, 0.f));
		}

		USizeBox* RightSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RightSize"));
		RightSize->SetWidthOverride(240.f);
		RightSize->SetContent(BuildStatColumn());
		Body->AddChildToHorizontalBox(RightSize);
	}

	// 루트는 통과 — 창(Panel) 밖은 게임 클릭으로
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

void UCharacterInfoWidget::NativeDestruct()
{
	SetStatComponent(nullptr);
	Super::NativeDestruct();
}

void UCharacterInfoWidget::SetStatComponent(UStatComponent* InStats)
{
	if (StatComponent.Get() == InStats) return;

	if (UStatComponent* Old = StatComponent.Get())
	{
		Old->OnStatsChanged.Remove(StatsChangedHandle);
	}
	StatsChangedHandle.Reset();
	StatComponent = InStats;

	if (InStats)
	{
		StatsChangedHandle = InStats->OnStatsChanged.AddUObject(this, &UCharacterInfoWidget::HandleStatsChanged);
	}
	Refresh();
}

void UCharacterInfoWidget::SetPreviewActor(ACharacterPreviewActor* InPreview)
{
	PreviewActor = InPreview;
	if (!PreviewImage) TakeWidget();
	if (!PreviewImage) return;

	UTextureRenderTarget2D* Target = InPreview ? InPreview->GetRenderTarget() : nullptr;
	if (Target)
	{
		FSlateBrush Brush = PreviewImage->GetBrush();
		Brush.SetResourceObject(Target);
		PreviewImage->SetBrush(Brush);
		PreviewImage->SetColorAndOpacity(FLinearColor::White);
	}
	else
	{
		PreviewImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	}
}

void UCharacterInfoWidget::HandleStatsChanged(const FCombatStats& NewStats)
{
	Refresh();
}

void UCharacterInfoWidget::Refresh()
{
	if (!ItemLevelText) return; // 트리가 아직 없음 — RebuildWidget 끝에서 다시 불린다

	const UStatComponent* Comp = StatComponent.Get();
	const FCombatStats Stats = Comp ? Comp->GetStats() : FCombatStats();

	// 장착 아이템 레벨 — 장비(3단계) 전까지는 착용 장비가 없어 0
	// 장착 아이템 레벨 — 스탯 컴포넌트와 같은 캐릭터의 장비 컴포넌트에서 (6부위 평균)
	const AActor* OwnerActor = Comp ? Comp->GetOwner() : nullptr;
	const UEquipmentComponent* Equip = OwnerActor ? OwnerActor->FindComponentByClass<UEquipmentComponent>() : nullptr;
	const float ItemLevel = Equip ? Equip->GetItemLevel() : 0.f;
	const int32 Whole = FMath::FloorToInt(ItemLevel);
	const int32 Fraction = FMath::RoundToInt((ItemLevel - Whole) * 100.f);
	ItemLevelText->SetText(FText::FromString(FString::FromInt(Whole)));
	ItemLevelFractionText->SetText(FText::FromString(FString::Printf(TEXT(".%02d"), FMath::Clamp(Fraction, 0, 99))));
	ItemLevelNoteText->SetText(ItemLevel > 0.f ? FText::GetEmpty() : NSLOCTEXT("CharInfo", "NoGear", "착용한 장비 없음"));
	RefreshGearSlots();

	// 공격력은 로아 감각의 표시값(K 곱하기 전), 실제 계산값은 툴팁
	AttackPowerText->SetText(FText::FromString(FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(Stats.DisplayAttackPower)))));
	if (AttackPowerRow)
	{
		AttackPowerRow->SetToolTipText(FText::FromString(FString::Printf(TEXT("주 스탯과 무기 공격력으로 정해지는 공격력\n실제 피해 계산 공격력: %lld"),
			static_cast<int64>(Stats.AttackPower))));
	}
	MaxHPText->SetText(FText::FromString(FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(Stats.MaxHP)))));

	const FCombatStatPoints& P = Stats.CombatPoints;
	const float SpecBonus = Stats.SpecializationMultiplier - 1.f;
	const float SpeedBonus = Stats.MoveSpeedMultiplier - 1.f;
	const float DominationBonus = Stats.DominationMultiplier - 1.f;

	for (int32 i = 0; i < CombatStatTexts.Num(); ++i)
	{
		if (!CombatStatTexts[i]) continue;
		const ELoACombatStat Stat = static_cast<ELoACombatStat>(i);
		CombatStatTexts[i]->SetText(FText::FromString(FString::FromInt(P.Get(Stat))));

		FString Tip;
		switch (Stat)
		{
		case ELoACombatStat::Crit:
			Tip = FString::Printf(TEXT("치명타 적중률 +%s"), *CharInfo_Percent(P.Crit * LoAStatRules::CritRatePerPoint));
			break;
		case ELoACombatStat::Specialization:
			Tip = FString::Printf(TEXT("캐스팅·차지 스킬(종말의 날·천벌·익스플로전) 피해 +%s"), *CharInfo_Percent(SpecBonus));
			break;
		case ELoACombatStat::Domination:
			Tip = FString::Printf(TEXT("무력화(그로기) 상태의 적에게 주는 피해 +%s"), *CharInfo_Percent(DominationBonus));
			break;
		case ELoACombatStat::Swiftness:
			Tip = FString::Printf(TEXT("스킬 재사용 대기시간 감소 %s (최대 %s)\n이동·공격 속도 +%s"),
				*CharInfo_Percent(Stats.CooldownReduction), *CharInfo_Percent(LoAStatRules::MaxCooldownReduction), *CharInfo_Percent(SpeedBonus));
			break;
		default:
			Tip = TEXT("아직 효과가 없는 특성입니다");
			break;
		}
		if (CombatStatRows.IsValidIndex(i) && CombatStatRows[i])
		{
			CombatStatRows[i]->SetToolTipText(FText::FromString(Tip));
		}
	}

	if (DetailText)
	{
		DetailText->SetText(FText::FromString(FString::Printf(
			TEXT("치명타 적중률  %s\n치명타 피해  %s\n특화 (캐스팅 스킬 피해)  +%s\n재사용 대기시간 감소  %s\n이동 속도  +%s\n공격·캐스팅 속도  +%s\n제압 (그로기 대상 피해)  +%s\n피해 증가 (합)  +%s\n피해 증가 (곱)  x%.4f"),
			*CharInfo_Percent(Stats.CritRate), *CharInfo_Percent(Stats.CritDamage), *CharInfo_Percent(SpecBonus),
			*CharInfo_Percent(Stats.CooldownReduction), *CharInfo_Percent(SpeedBonus), *CharInfo_Percent(Stats.AttackSpeedMultiplier - 1.f),
			*CharInfo_Percent(DominationBonus), *CharInfo_Percent(Stats.AdditiveDamageBonus), Stats.MultiplicativeDamage)));
	}
}

void UCharacterInfoWidget::RefreshGearSlots()
{
	const UStatComponent* Comp = StatComponent.Get();
	const AActor* OwnerActor = Comp ? Comp->GetOwner() : nullptr;
	const UEquipmentComponent* Equip = OwnerActor ? OwnerActor->FindComponentByClass<UEquipmentComponent>() : nullptr;

	for (int32 i = 0; i < GearSlots.Num(); ++i)
	{
		FGearSlotWidgets& W = GearSlots[i];
		const FEquipmentInstance* Instance = Equip ? Equip->GetEquipped(static_cast<EEquipSlot>(i)) : nullptr;
		const FItemDefinition* Def = Instance ? LoAItems::Find(Instance->ItemId) : nullptr;
		if (!Def)
		{
			W.Icon->SetVisibility(ESlateVisibility::Collapsed);
			W.RefineText->SetVisibility(ESlateVisibility::Collapsed);
			W.PartText->SetColorAndOpacity(FSlateColor(CharInfo_DimColor));
			W.PartText->SetVisibility(ESlateVisibility::HitTestInvisible);
			W.Frame->SetBrushColor(CharInfo_FrameColor);
			W.Frame->SetToolTipText(FText::Format(NSLOCTEXT("CharInfo", "EmptySlotTip", "{0}\n미착용"), W.PartName));
			continue;
		}

		// 아이콘이 있으면 아이콘, 없으면(임시 아이콘 전) 부위 이름을 밝게 — 어느 쪽이든 착용 중임이 보이게
		UTexture2D* Texture = Def->Icon.LoadSynchronous();
		if (Texture)
		{
			W.Icon->SetBrushFromTexture(Texture);
			W.Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			W.PartText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			W.Icon->SetVisibility(ESlateVisibility::Collapsed);
			W.PartText->SetColorAndOpacity(FSlateColor(CharInfo_ValueColor));
			W.PartText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		W.RefineText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Instance->RefineLevel)));
		W.RefineText->SetVisibility(ESlateVisibility::HitTestInvisible);
		W.Frame->SetBrushColor(CharInfo_Gold);
		W.Frame->SetToolTipText(FText::Format(NSLOCTEXT("CharInfo", "EquippedSlotTip", "{0}\n\n우클릭: 해제"),
			UEquipmentComponent::DescribeEquipment(*Instance)));
	}
}

bool UCharacterInfoWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UCharacterInfoWidget::HandleCloseClicked()
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->SetCharacterInfoOpen(false);
	}
}

void UCharacterInfoWidget::HandleDetailClicked()
{
	bDetailOpen = !bDetailOpen;
	if (DetailBox)
	{
		DetailBox->SetVisibility(bDetailOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailButtonText)
	{
		DetailButtonText->SetText(bDetailOpen ? NSLOCTEXT("CharInfo", "DetailClose", "상세정보 닫기") : NSLOCTEXT("CharInfo", "DetailOpen", "상세정보"));
	}
}

FReply UCharacterInfoWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 제목 줄을 좌클릭 — 창 끌기 시작
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// 미리보기를 좌클릭 — 끄는 동안 캐릭터 회전
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && PreviewArea && PreviewActor.IsValid()
		&& PreviewArea->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bRotatingPreview = true;
		LastPreviewMouseX = InMouseEvent.GetScreenSpacePosition().X;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	// 장착 칸 우클릭 — 해제해서 소지품으로 (빠른 연타의 두 번째 클릭은 이미 빈 칸이라 아무 일 없음)
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		for (int32 i = 0; i < GearSlots.Num(); ++i)
		{
			if (GearSlots[i].Box && GearSlots[i].Box->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
			{
				if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
				{
					PC->HandleEquipSlotRightClick(static_cast<EEquipSlot>(i));
				}
				break;
			}
		}
	}

	// 창 안 빈 곳 클릭도 먹는다 (루트가 SelfHitTestInvisible이라 창 밖 클릭은 여기까지 안 옴)
	return FReply::Handled();
}

FReply UCharacterInfoWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 두 번째 클릭은 DoubleClick으로 온다 — 안 먹으면 창 뒤의 맵으로 클릭 이동이 샌다
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCharacterInfoWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRotatingPreview)
	{
		const float MouseX = InMouseEvent.GetScreenSpacePosition().X;
		if (ACharacterPreviewActor* Preview = PreviewActor.Get())
		{
			// 오른쪽으로 끌면 캐릭터가 오른쪽(시계 반대 방향, 위에서 볼 때)으로 돈다
			Preview->AddPreviewYaw(-(MouseX - LastPreviewMouseX) * CharInfo_PreviewRotateSpeed);
		}
		LastPreviewMouseX = MouseX;
		return FReply::Handled();
	}
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UCharacterInfoWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if ((WindowDrag.bDragging || bRotatingPreview) && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		bRotatingPreview = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	// 창 위에서 뗀 버튼도 게임으로 넘기지 않는다
	return FReply::Handled();
}

void UCharacterInfoWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	bRotatingPreview = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
