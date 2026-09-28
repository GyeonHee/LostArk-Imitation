#include "UI/AbandonRaidWidget.h"
#include "LoAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

namespace
{
	UTextBlock* MakeAbandonText(UWidgetTree* Tree, const TCHAR* Name, const FText& Text, int32 Size, FLinearColor Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetJustification(ETextJustify::Center);
		Block->SetText(Text);
		return Block;
	}

	// 어두운 배경 + 글자 한 줄짜리 버튼
	UButton* MakeAbandonButton(UWidgetTree* Tree, const TCHAR* Name, const FText& Label, FLinearColor Tint)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Tint);
		UTextBlock* Text = MakeAbandonText(Tree, *(FString(Name) + TEXT("Label")), Label, 12, FLinearColor(0.92f, 0.92f, 0.92f, 1.f));
		Button->SetContent(Text);
		return Button;
	}
}

TSharedRef<SWidget> UAbandonRaidWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AbandonRoot"));
		WidgetTree->RootWidget = Root;

		// 좌상단 "중단하기"
		AbandonButton = MakeAbandonButton(WidgetTree, TEXT("AbandonButton"), NSLOCTEXT("Abandon", "Button", "중단하기"), FLinearColor(0.12f, 0.14f, 0.2f, 0.9f));
		AbandonButton->OnClicked.AddDynamic(this, &UAbandonRaidWidget::HandleAbandonClicked);
		if (UCanvasPanelSlot* ButtonSlot = Root->AddChildToCanvas(AbandonButton))
		{
			ButtonSlot->SetAnchors(FAnchors(0.f, 0.f));
			ButtonSlot->SetPosition(ButtonPosition);
			ButtonSlot->SetSize(FVector2D(120.f, 32.f));
		}

		// 화면 가운데 확인 창 — 평소엔 숨김
		ConfirmPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmPanel"));
		ConfirmPanel->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.95f));
		ConfirmPanel->SetPadding(FMargin(40.f, 20.f));
		ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(ConfirmPanel))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.4f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetAutoSize(true);
		}

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmLines"));
		ConfirmPanel->SetContent(Lines);

		UTextBlock* Question = MakeAbandonText(WidgetTree, TEXT("Question"), NSLOCTEXT("Abandon", "Question", "레이드를 중단하시겠습니까?"), 16, FLinearColor::White);
		UTextBlock* Detail = MakeAbandonText(WidgetTree, TEXT("Detail"), NSLOCTEXT("Abandon", "Detail", "레이드 진행 상황은 사라지고 정비소로 돌아갑니다."), 12, FLinearColor(0.75f, 0.75f, 0.75f, 1.f));
		for (UTextBlock* Line : { Question, Detail })
		{
			if (UVerticalBoxSlot* LineSlot = Lines->AddChildToVerticalBox(Line))
			{
				LineSlot->SetHorizontalAlignment(HAlign_Center);
				LineSlot->SetPadding(FMargin(0.f, 3.f));
			}
		}

		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmButtons"));
		if (UVerticalBoxSlot* ButtonsSlot = Lines->AddChildToVerticalBox(Buttons))
		{
			ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonsSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}

		UButton* Confirm = MakeAbandonButton(WidgetTree, TEXT("ConfirmButton"), NSLOCTEXT("Abandon", "Confirm", "확인"), FLinearColor(0.5f, 0.12f, 0.1f, 1.f));
		Confirm->OnClicked.AddDynamic(this, &UAbandonRaidWidget::HandleConfirmClicked);
		UButton* Cancel = MakeAbandonButton(WidgetTree, TEXT("CancelButton"), NSLOCTEXT("Abandon", "Cancel", "취소"), FLinearColor(0.18f, 0.2f, 0.26f, 1.f));
		Cancel->OnClicked.AddDynamic(this, &UAbandonRaidWidget::HandleCancelClicked);
		for (UButton* Button : { Confirm, Cancel })
		{
			if (UHorizontalBoxSlot* BtnSlot = Buttons->AddChildToHorizontalBox(Button))
			{
				BtnSlot->SetPadding(FMargin(8.f, 0.f));
			}
		}
	}

	// 루트 캔버스 자체는 클릭을 안 먹고, 버튼·확인 창만 받는다 — 빈 곳 클릭은 클릭 이동으로
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Super::RebuildWidget();
}

void UAbandonRaidWidget::HandleAbandonClicked()
{
	if (ConfirmPanel)
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAbandonRaidWidget::HandleConfirmClicked()
{
	CloseConfirm();
	if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(GetOwningPlayer()))
	{
		PC->AbandonRaid();
	}
}

void UAbandonRaidWidget::HandleCancelClicked()
{
	CloseConfirm();
}

void UAbandonRaidWidget::CloseConfirm()
{
	if (ConfirmPanel)
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
