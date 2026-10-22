#include "UI/ClearWidget.h"
#include "LoAPlayerController.h"
#include "Item/ItemTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

namespace
{
	UTextBlock* MakeClearText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, FLinearColor Color, FLinearColor OutlineColor, int32 OutlineSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = OutlineSize;
		Font.OutlineSettings.OutlineColor = OutlineColor;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}

	// 가로 전체를 덮는 띠 — 화면 세로 AnchorY 위치에 Height 두께
	UBorder* AddClearBand(UWidgetTree* Tree, UCanvasPanel* Root, const TCHAR* Name, FLinearColor Color, float AnchorY, float Height)
	{
		UBorder* Band = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Band->SetBrushColor(Color);
		if (UCanvasPanelSlot* BandSlot = Root->AddChildToCanvas(Band))
		{
			BandSlot->SetAnchors(FAnchors(0.f, AnchorY, 1.f, AnchorY));
			BandSlot->SetAlignment(FVector2D(0.f, 0.5f));
			BandSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, Height));
		}
		return Band;
	}

	// 어두운 버튼 + 가운데 글자 (고정 크기)
	USizeBox* MakeClearButton(UWidgetTree* Tree, const TCHAR* Name, const FText& Label, float Width, UButton*& OutButton, UTextBlock*& OutText)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*(FString(Name) + TEXT("Box"))));
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(34.f);
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(FLinearColor(0.16f, 0.14f, 0.10f, 1.f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(0.32f, 0.26f, 0.14f, 1.f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(0.10f, 0.09f, 0.06f, 1.f));
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.10f, 0.10f, 0.11f, 0.8f));
		Button->SetStyle(Style);
		Box->SetContent(Button);
		UTextBlock* Text = MakeClearText(Tree, *(FString(Name) + TEXT("Text")), 11, FLinearColor(0.95f, 0.92f, 0.85f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.f), 0);
		Text->SetText(Label);
		Button->SetContent(Text);
		OutButton = Button;
		OutText = Text;
		return Box;
	}
}

TSharedRef<SWidget> UClearWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ClearRoot"));
		WidgetTree->RootWidget = Root;

		// 상단 안내 — 반투명 어두운 띠 위에 3줄 (처치 문구 / 이동까지 남은 시간 / N초)
		UBorder* TopBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBand"));
		TopBand->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		TopBand->SetPadding(FMargin(80.f, 10.f));
		if (UCanvasPanelSlot* TopSlot = Root->AddChildToCanvas(TopBand))
		{
			TopSlot->SetAnchors(FAnchors(0.5f, 0.15f));
			TopSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TopSlot->SetAutoSize(true);
		}
		UVerticalBox* TopLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TopLines"));
		TopBand->SetContent(TopLines);

		const FLinearColor Shadow(0.f, 0.f, 0.f, 0.8f);
		HeadlineText = MakeClearText(WidgetTree, TEXT("HeadlineText"), 13, FLinearColor(0.85f, 0.85f, 0.85f, 1.f), Shadow, 1);
		UTextBlock* RemainLabel = MakeClearText(WidgetTree, TEXT("RemainLabel"), 13, FLinearColor(0.85f, 0.85f, 0.85f, 1.f), Shadow, 1);
		RemainLabel->SetText(NSLOCTEXT("Clear", "RemainLabel", "정비소 이동까지 남은 시간"));
		CountdownText = MakeClearText(WidgetTree, TEXT("CountdownText"), 15, AccentColor, Shadow, 1);
		for (UTextBlock* Line : { HeadlineText.Get(), RemainLabel, CountdownText.Get() })
		{
			if (UVerticalBoxSlot* LineSlot = TopLines->AddChildToVerticalBox(Line))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
				LineSlot->SetPadding(FMargin(0.f, 2.f));
			}
		}

		// 가운데 금빛 광채 — 넓고 옅은 띠 위에 좁고 밝은 띠를 겹쳐 빛이 번지는 느낌
		AddClearBand(WidgetTree, Root, TEXT("GlowWide"), GlowColor, 0.5f, 170.f);
		AddClearBand(WidgetTree, Root, TEXT("GlowCore"), GlowCoreColor, 0.5f, 6.f);

		// 큰 문구
		UTextBlock* ClearText = MakeClearText(WidgetTree, TEXT("ClearText"), 56, FLinearColor(1.f, 0.98f, 0.94f, 1.f), FLinearColor(0.55f, 0.28f, 0.02f, 0.95f), 3);
		ClearText->SetText(NSLOCTEXT("Clear", "DungeonClear", "던전 클리어"));
		if (UCanvasPanelSlot* ClearSlot = Root->AddChildToCanvas(ClearText))
		{
			ClearSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ClearSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ClearSlot->SetAutoSize(true);
		}

		// 결과 줄 — 어두운 띠 위 한 줄 (레퍼런스의 전리품 줄 자리)
		UBorder* ResultBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultBand"));
		ResultBand->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.8f));
		ResultBand->SetPadding(FMargin(60.f, 10.f));
		if (UCanvasPanelSlot* ResultSlot = Root->AddChildToCanvas(ResultBand))
		{
			ResultSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ResultSlot->SetAlignment(FVector2D(0.5f, 0.f));
			ResultSlot->SetPosition(FVector2D(0.f, 110.f));
			ResultSlot->SetAutoSize(true);
		}
		ResultBand->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UVerticalBox* ResultLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultLines"));
		ResultBand->SetContent(ResultLines);

		ResultText = MakeClearText(WidgetTree, TEXT("ResultText"), 14, FLinearColor(0.92f, 0.92f, 0.92f, 1.f), Shadow, 1);
		if (UVerticalBoxSlot* S = ResultLines->AddChildToVerticalBox(ResultText)) S->SetHorizontalAlignment(HAlign_Center);

		// 전리품 줄
		RewardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RewardRow"));
		if (UVerticalBoxSlot* S = ResultLines->AddChildToVerticalBox(RewardRow))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));
		}
		RewardNoteText = MakeClearText(WidgetTree, TEXT("RewardNote"), 11, AccentColor, Shadow, 1);
		RewardNoteText->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* S = ResultLines->AddChildToVerticalBox(RewardNoteText)) S->SetHorizontalAlignment(HAlign_Center);

		// [더보기] [정비소로 이동]
		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ClearButtons"));
		if (UVerticalBoxSlot* S = ResultLines->AddChildToVerticalBox(Buttons))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		UTextBlock* LeaveText = nullptr;
		UButton* LeaveButton = nullptr;
		UTextBlock* BonusText = nullptr;
		UButton* Bonus = nullptr;
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(MakeClearButton(WidgetTree, TEXT("ClearBonus"), FText::GetEmpty(), 230.f, Bonus, BonusText)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		BonusButton = Bonus;
		BonusButtonText = BonusText;
		Buttons->AddChildToHorizontalBox(MakeClearButton(WidgetTree, TEXT("ClearLeave"), NSLOCTEXT("Clear", "Leave", "정비소로 이동"), 150.f, LeaveButton, LeaveText));
		BonusButton->OnClicked.AddDynamic(this, &UClearWidget::HandleBonusClicked);
		LeaveButton->OnClicked.AddDynamic(this, &UClearWidget::HandleLeaveClicked);
	}

	// 버튼만 클릭을 받는다 — 나머지(빈 화면)는 클릭 이동 그대로
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Super::RebuildWidget();
}

void UClearWidget::SetTexts(const FText& Headline, const FText& ResultLine)
{
	if (!HeadlineText) TakeWidget();
	if (HeadlineText) HeadlineText->SetText(Headline);
	if (ResultText) ResultText->SetText(ResultLine);
}

void UClearWidget::SetCountdown(int32 Seconds)
{
	if (!CountdownText) TakeWidget();
	if (CountdownText)
	{
		CountdownText->SetText(FText::Format(NSLOCTEXT("Clear", "Countdown", "{0}초"), FText::AsNumber(FMath::Max(0, Seconds))));
	}
}

void UClearWidget::SetRewards(const TArray<FLoARewardItem>& Items, const FText& Note)
{
	if (!RewardRow) TakeWidget();
	if (!RewardRow) return;
	RewardRow->ClearChildren();

	UTexture2D* SillingTexture = SillingIcon.LoadSynchronous();
	UTexture2D* GoldTexture = GoldIcon.LoadSynchronous();
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FLoARewardItem& Item = Items[i];
		// 화폐(골드·실링)는 코인 + 숫자로 — 변수 이름은 예전 그대로 bSilling
		const bool bSilling = LoARaidReward::IsCurrency(Item.ItemId);
		const bool bGold = Item.ItemId == LoARaidReward::Gold;
		const FItemDefinition* Def = bSilling ? nullptr : LoAItems::Find(Item.ItemId);
		UTexture2D* Texture = bSilling ? (bGold ? GoldTexture : SillingTexture) : (Def ? Def->Icon.LoadSynchronous() : nullptr);

		// 아이콘 44 + 오른쪽 아래 개수 (실링은 칸이 넓게 숫자만 옆에)
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("Reward_%d"), i)));
		Box->SetHeightOverride(44.f);
		if (!bSilling) Box->SetWidthOverride(44.f);
		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*FString::Printf(TEXT("RewardOverlay_%d"), i)));
		Box->SetContent(Overlay);
		Box->SetToolTipText(bGold ? NSLOCTEXT("Clear", "GoldName", "골드") : bSilling ? NSLOCTEXT("Clear", "SillingName", "실링") : (Def ? Def->DisplayName : FText::FromName(Item.ItemId)));

		if (bSilling)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("RewardSilling_%d"), i)));
			Overlay->AddChildToOverlay(Row);
			UImage* Coin = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("RewardIcon_%d"), i)));
			if (Texture) Coin->SetBrushFromTexture(Texture);
			Coin->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Coin)) S->SetVerticalAlignment(VAlign_Center);
			UTextBlock* Amount = MakeClearText(WidgetTree, *FString::Printf(TEXT("RewardCount_%d"), i), 13, FLinearColor(0.92f, 0.92f, 0.95f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.9f), 1);
			Amount->SetText(FText::FromString(LoAItems::FormatWithCommas(Item.Count)));
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Amount))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
			}
		}
		else
		{
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("RewardIcon_%d"), i)));
			if (Texture) Icon->SetBrushFromTexture(Texture);
			else Icon->SetColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f, 1.f));
			if (UOverlaySlot* S = Overlay->AddChildToOverlay(Icon))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
				S->SetVerticalAlignment(VAlign_Fill);
			}
			UTextBlock* Count = MakeClearText(WidgetTree, *FString::Printf(TEXT("RewardCount_%d"), i), 11, FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 1.f), 1);
			Count->SetText(FText::FromString(LoAItems::FormatWithCommas(Item.Count)));
			if (UOverlaySlot* S = Overlay->AddChildToOverlay(Count))
			{
				S->SetHorizontalAlignment(HAlign_Right);
				S->SetVerticalAlignment(VAlign_Bottom);
				S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			}
		}
		if (UHorizontalBoxSlot* S = RewardRow->AddChildToHorizontalBox(Box)) S->SetPadding(FMargin(5.f, 0.f));
	}

	RewardNoteText->SetText(Note);
	RewardNoteText->SetVisibility(Note.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UClearWidget::SetBonusState(bool bVisible, bool bTaken, bool bAffordable, int64 Cost)
{
	if (!BonusButton) TakeWidget();
	if (!BonusButton) return;
	if (UWidget* Box = BonusButton->GetParent()) Box->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	BonusButton->SetIsEnabled(bVisible && !bTaken && bAffordable);
	BonusButtonText->SetText(bTaken
		? NSLOCTEXT("Clear", "BonusTaken", "더보기 완료")
		: FText::Format(NSLOCTEXT("Clear", "Bonus", "더보기 (재료 +50%)  {0} 골드"), FText::FromString(LoAItems::FormatWithCommas(Cost))));
}

void UClearWidget::HandleBonusClicked()
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>()) PC->RequestClearBonus();
}

void UClearWidget::HandleLeaveClicked()
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>()) PC->RequestClearLeave();
}
