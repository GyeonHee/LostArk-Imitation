#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Item/BuildTypes.h"
#include "Stat/EngravingComponent.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

/**
 * 보석·각인 UI 공용 조각 (P창 [보석]/[각인] 탭, 각인 효과 창) — 전부 C++ 트리, 텍스처 없이 RoundedBox 원으로 그린다.
 * 로아 캐릭터 정보 창 색: 거의 검은 남색 바탕 + 금색/주황 글자
 */
namespace LoABuildUI
{
	inline const FLinearColor PanelColor(0.028f, 0.032f, 0.045f, 0.97f);
	inline const FLinearColor SectionColor(0.055f, 0.06f, 0.08f, 1.f);
	inline const FLinearColor RowColor(0.075f, 0.08f, 0.10f, 1.f);
	inline const FLinearColor UiGold(0.93f, 0.80f, 0.52f, 1.f);
	inline const FLinearColor UiOrange(1.f, 0.55f, 0.18f, 1.f);         // 각인 이름 (로아 주황)
	inline const FLinearColor SkillYellow(1.f, 0.85f, 0.25f, 1.f);    // 보석 효과의 스킬 이름
	inline const FLinearColor UiGreen(0.55f, 0.95f, 0.35f, 1.f);        // "기본 공격력 총합"
	inline const FLinearColor UiFrame(0.42f, 0.36f, 0.22f, 1.f);
	inline const FLinearColor UiLabel(0.82f, 0.82f, 0.84f, 1.f);
	inline const FLinearColor UiDim(0.45f, 0.45f, 0.48f, 1.f);
	inline const FLinearColor ButtonColor(0.12f, 0.13f, 0.17f, 1.f);
	inline const FLinearColor ButtonOnColor(0.36f, 0.29f, 0.14f, 1.f);
	inline const FLinearColor PresetOn(0.70f, 0.12f, 0.10f, 1.f);     // 프리셋 1 (로아 빨간 칸)
	inline const FLinearColor BoardColor(0.06f, 0.07f, 0.10f, 1.f);
	inline const FLinearColor RingIdle(0.20f, 0.21f, 0.26f, 1.f);
	inline const FLinearColor RingActive(0.25f, 0.55f, 1.f, 1.f);     // 장착된 각인 칸 (로아 파란 테두리)
	inline const FLinearColor DamageGem(0.85f, 0.45f, 0.10f, 1.f);    // 겁화 테두리
	inline const FLinearColor CooldownGem(0.55f, 0.25f, 0.85f, 1.f);  // 작열 테두리

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	inline UImage* MakeCircleImage(UWidgetTree* Tree, const FLinearColor& Color)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Color);
		return Image;
	}

	/** 테두리 원 + 안쪽 어두운 원 + 아이콘(원형 텍스처 권장). 반환 = 크기 고정 SizeBox */
	inline USizeBox* MakeRingIcon(UWidgetTree* Tree, float Diameter, const FLinearColor& Ring, UTexture2D* Icon, float RingWidth = 3.f)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Diameter);
		Box->SetHeightOverride(Diameter);
		UOverlay* Layers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Box->SetContent(Layers);

		auto AddFill = [Layers](UWidget* W, float Pad)
		{
			if (UOverlaySlot* S = Layers->AddChildToOverlay(W))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
				S->SetVerticalAlignment(VAlign_Fill);
				S->SetPadding(FMargin(Pad));
			}
		};
		AddFill(MakeCircleImage(Tree, Ring), 0.f);
		AddFill(MakeCircleImage(Tree, FLinearColor(0.03f, 0.03f, 0.05f, 1.f)), RingWidth);
		if (Icon)
		{
			UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
			Image->SetBrushFromTexture(Icon);
			AddFill(Image, RingWidth + 2.f);
		}
		return Box;
	}

	/** 각인 단계 — 채운 ◆ = 단계, 빈 ◇ = 남은 칸 (최대 4) */
	inline UTextBlock* MakeLevelDiamonds(UWidgetTree* Tree, int32 Level, int32 Size = 9)
	{
		FString Text;
		for (int32 i = 0; i < LoAEngravings::MaxLevel; ++i) Text += i < Level ? TEXT("◆") : TEXT("◇");
		return MakeText(Tree, Text, Size, Level > 0 ? UiOrange : UiDim);
	}

	/** 클릭 판정용 버튼 — Border + 가운데 글자 */
	inline UBorder* MakeButton(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Fill, const FLinearColor& TextColor = UiLabel)
	{
		UBorder* Button = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Button->SetBrushColor(Fill);
		Button->SetPadding(FMargin(10.f, 4.f));
		Button->SetHorizontalAlignment(HAlign_Center);
		Button->SetVerticalAlignment(VAlign_Center);
		Button->SetContent(MakeText(Tree, Text, Size, TextColor));
		return Button;
	}

	/** 프리셋 줄 — 1번만 쓰고 나머지는 잠금 표시 (프리셋 저장은 아직 없음) */
	inline UWidget* MakePresetBar(UWidgetTree* Tree, int32 Count, float CellWidth = 34.f)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 i = 0; i < Count; ++i)
		{
			UBorder* Cell = MakeButton(Tree, i == 0 ? TEXT("1") : FString::FromInt(i + 1), 11, i == 0 ? PresetOn : SectionColor, i == 0 ? FLinearColor::White : UiDim);
			if (i > 0) Cell->SetToolTipText(NSLOCTEXT("Build", "PresetSoon", "프리셋은 아직 1번만 쓸 수 있습니다"));
			USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(CellWidth);
			Box->SetHeightOverride(26.f);
			Box->SetContent(Cell);
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Box)) S->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
		}
		return Row;
	}

	/** 각인 판 — 큰 원 + 정오각형 꼭짓점 5칸(위 → 시계 방향). 칸 위치(중심)를 돌려준다 */
	inline void GetEngravingSlotCenters(float BoardSize, TArray<FVector2D>& OutCenters)
	{
		const FVector2D C(BoardSize * 0.5f, BoardSize * 0.5f);
		const float R = BoardSize * 0.37f;
		OutCenters.Reset();
		for (int32 i = 0; i < LoAEngravings::SlotCount; ++i)
		{
			const float A = FMath::DegreesToRadians(-90.f + 72.f * i);
			OutCenters.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
	}

	inline void PlaceAt(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Center, const FVector2D& Size)
	{
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Child))
		{
			S->SetAutoSize(false);
			S->SetSize(Size);
			S->SetPosition(Center - Size * 0.5f);
		}
	}

	/**
	 * 각인 판 — 어두운 큰 원 + 안쪽 원 + 가운데 장식, 정오각형 꼭짓점 5칸(위 → 시계 방향).
	 * 칸마다 위에 이름(주황), 원 아이콘(장착이면 파란 테두리), 아래 ◆ 단계. OutSlotWidgets[i] = 칸의 원(클릭 판정용)
	 */
	inline UCanvasPanel* MakeEngravingBoard(UWidgetTree* Tree, float BoardSize, const TArray<FName>& Slots,
		const UEngravingComponent* Engravings, TArray<UWidget*>& OutSlotWidgets, float SlotDiameter = 64.f)
	{
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		const FVector2D Center(BoardSize * 0.5f, BoardSize * 0.5f);
		PlaceAt(Canvas, MakeCircleImage(Tree, FLinearColor(0.10f, 0.11f, 0.15f, 1.f)), Center, FVector2D(BoardSize * 0.92f));
		PlaceAt(Canvas, MakeCircleImage(Tree, BoardColor), Center, FVector2D(BoardSize * 0.88f));
		PlaceAt(Canvas, MakeCircleImage(Tree, FLinearColor(0.12f, 0.13f, 0.17f, 1.f)), Center, FVector2D(BoardSize * 0.44f));
		PlaceAt(Canvas, MakeCircleImage(Tree, BoardColor), Center, FVector2D(BoardSize * 0.41f));
		PlaceAt(Canvas, MakeCircleImage(Tree, FLinearColor(0.35f, 0.25f, 0.55f, 1.f)), Center, FVector2D(BoardSize * 0.09f));

		TArray<FVector2D> Centers;
		GetEngravingSlotCenters(BoardSize, Centers);
		OutSlotWidgets.Reset();
		for (int32 i = 0; i < Centers.Num(); ++i)
		{
			const FName Id = Slots.IsValidIndex(i) ? Slots[i] : NAME_None;
			const FEngravingDefinition* Def = LoAEngravings::Find(Id);
			const int32 Level = (Def && Engravings) ? FMath::Max(0, Engravings->GetLevel(Id)) : 0;

			UVerticalBox* Cell = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UTextBlock* Name = MakeText(Tree, Def ? Def->Name.ToString() : TEXT(""), 10, UiOrange);
			Name->SetShadowOffset(FVector2D(1.f, 1.f));
			Name->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
			if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Name)) S->SetHorizontalAlignment(HAlign_Center);
			USizeBox* Ring = MakeRingIcon(Tree, SlotDiameter, Def ? RingActive : RingIdle, Def ? LoAEngravings::LoadIcon(Id) : nullptr, Def ? 4.f : 3.f);
			if (Def)
			{
				Ring->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s  유물 %d단계\n%s"), *Def->Name.ToString(), Level, *LoAEngravings::Describe(Id, Level))));
			}
			if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Ring)) S->SetHorizontalAlignment(HAlign_Center);
			UTextBlock* Diamonds = MakeLevelDiamonds(Tree, Def ? Level : 0, 9);
			if (!Def) Diamonds->SetText(FText::GetEmpty());
			if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Diamonds)) S->SetHorizontalAlignment(HAlign_Center);
			PlaceAt(Canvas, Cell, Centers[i] + FVector2D(0.f, -4.f), FVector2D(SlotDiameter + 60.f, SlotDiameter + 40.f));
			OutSlotWidgets.Add(Ring);
		}
		return Canvas;
	}
}
