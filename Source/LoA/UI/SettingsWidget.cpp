#include "UI/SettingsWidget.h"
#include "LoAGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FLinearColor Settings_TextColor(0.9f, 0.9f, 0.9f, 1.f);
	const FLinearColor Settings_SubColor(0.7f, 0.72f, 0.76f, 1.f);
	const FLinearColor Settings_Accent(0.85f, 0.7f, 0.35f, 1.f);

	UTextBlock* MakeSettingsText(UWidgetTree* Tree, const TCHAR* Name, const FText& Text, int32 Size, FLinearColor Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetText(Text);
		return Block;
	}

	UButton* MakeSettingsButton(UWidgetTree* Tree, const TCHAR* Name, const FText& Label, FLinearColor Tint)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Tint);
		UTextBlock* Text = MakeSettingsText(Tree, *(FString(Name) + TEXT("Label")), Label, 12, Settings_TextColor);
		Text->SetJustification(ETextJustify::Center);
		Button->SetContent(Text);
		return Button;
	}

	/** 왼쪽 이름(고정 폭) + 슬라이더 + 오른쪽 % */
	USlider* AddSettingsSliderRow(UWidgetTree* Tree, UVerticalBox* Lines, const TCHAR* Name, const FText& Label, UTextBlock*& OutPercent)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(FString(Name) + TEXT("Row")));
		if (UVerticalBoxSlot* RowSlot = Lines->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 6.f));
		}

		USizeBox* LabelBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(FString(Name) + TEXT("LabelBox")));
		LabelBox->SetWidthOverride(64.f);
		LabelBox->SetContent(MakeSettingsText(Tree, *(FString(Name) + TEXT("Label")), Label, 12, Settings_TextColor));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) S->SetVerticalAlignment(VAlign_Center);

		USlider* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
		Slider->SetMinValue(0.f);
		Slider->SetMaxValue(1.f);
		Slider->SetStepSize(0.01f);
		Slider->IsFocusable = false;
		Slider->SetSliderBarColor(FLinearColor(0.3f, 0.3f, 0.34f, 1.f));
		Slider->SetSliderHandleColor(Settings_Accent);
		USizeBox* SliderBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(FString(Name) + TEXT("Box")));
		SliderBox->SetWidthOverride(170.f);
		SliderBox->SetHeightOverride(22.f);
		SliderBox->SetContent(Slider);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(SliderBox)) S->SetVerticalAlignment(VAlign_Center);

		OutPercent = MakeSettingsText(Tree, *(FString(Name) + TEXT("Percent")), FText::GetEmpty(), 12, Settings_SubColor);
		OutPercent->SetJustification(ETextJustify::Right);
		USizeBox* PercentBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(FString(Name) + TEXT("PercentBox")));
		PercentBox->SetWidthOverride(44.f);
		PercentBox->SetContent(OutPercent);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PercentBox)) S->SetVerticalAlignment(VAlign_Center);
		return Slider;
	}

	void SettingsReturnFocus()
	{
		// 슬라이더·체크박스를 만진 뒤에도 키 입력(스킬·이동)이 게임으로 가게
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

TSharedRef<SWidget> USettingsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsRoot"));
		WidgetTree->RootWidget = Root;

		// 우측 맨 아래 [설정]
		ToggleButton = MakeSettingsButton(WidgetTree, TEXT("SettingsToggle"), NSLOCTEXT("Settings", "Button", "설정"), FLinearColor(0.12f, 0.14f, 0.2f, 0.9f));
		ToggleButton->OnClicked.AddDynamic(this, &USettingsWidget::HandleToggleClicked);
		if (UCanvasPanelSlot* ButtonSlot = Root->AddChildToCanvas(ToggleButton))
		{
			ButtonSlot->SetAnchors(FAnchors(1.f, 1.f));
			ButtonSlot->SetAlignment(FVector2D(1.f, 1.f));
			ButtonSlot->SetPosition(FVector2D(-12.f, -12.f));
			ButtonSlot->SetSize(FVector2D(72.f, 30.f));
		}

		// 버튼 바로 위 창 — 평소엔 숨김
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SettingsPanel"));
		Panel->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.95f));
		Panel->SetPadding(FMargin(18.f, 12.f));
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(1.f, 1.f));
			PanelSlot->SetAlignment(FVector2D(1.f, 1.f));
			PanelSlot->SetPosition(FVector2D(-12.f, -50.f));
			PanelSlot->SetAutoSize(true);
		}

		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsLines"));
		Panel->SetContent(Lines);

		// 제목 + 닫기
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SettingsHeader"));
		if (UVerticalBoxSlot* HeaderSlot = Lines->AddChildToVerticalBox(Header))
		{
			HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
		if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(
			MakeSettingsText(WidgetTree, TEXT("SettingsTitle"), NSLOCTEXT("Settings", "Title", "게임 설정"), 15, Settings_Accent)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		UButton* Close = MakeSettingsButton(WidgetTree, TEXT("SettingsClose"), NSLOCTEXT("Settings", "Close", "X"), FLinearColor(0.18f, 0.2f, 0.26f, 1.f));
		Close->OnClicked.AddDynamic(this, &USettingsWidget::HandleCloseClicked);
		if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(Close)) S->SetVerticalAlignment(VAlign_Center);

		UTextBlock* SoundHeader = MakeSettingsText(WidgetTree, TEXT("SoundHeader"), NSLOCTEXT("Settings", "Sound", "사운드"), 12, Settings_SubColor);
		if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(SoundHeader)) S->SetPadding(FMargin(0.f, 2.f, 0.f, 4.f));

		// 배경음 음소거
		UHorizontalBox* MuteRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MuteRow"));
		if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(MuteRow)) S->SetPadding(FMargin(0.f, 4.f));
		MuteCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("MuteCheck"));
		MuteCheck->OnCheckStateChanged.AddDynamic(this, &USettingsWidget::HandleMuteChanged);
		if (UHorizontalBoxSlot* S = MuteRow->AddChildToHorizontalBox(MuteCheck)) S->SetVerticalAlignment(VAlign_Center);
		if (UHorizontalBoxSlot* S = MuteRow->AddChildToHorizontalBox(
			MakeSettingsText(WidgetTree, TEXT("MuteLabel"), NSLOCTEXT("Settings", "Mute", "배경음 음소거"), 12, Settings_TextColor)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		}

		UTextBlock* MusicPct = nullptr;
		UTextBlock* SfxPct = nullptr;
		MusicSlider = AddSettingsSliderRow(WidgetTree, Lines, TEXT("MusicSlider"), NSLOCTEXT("Settings", "Music", "배경음"), MusicPct);
		SfxSlider = AddSettingsSliderRow(WidgetTree, Lines, TEXT("SfxSlider"), NSLOCTEXT("Settings", "Sfx", "효과음"), SfxPct);
		MusicPercentText = MusicPct;
		SfxPercentText = SfxPct;
		MusicSlider->OnValueChanged.AddDynamic(this, &USettingsWidget::HandleMusicSlider);
		SfxSlider->OnValueChanged.AddDynamic(this, &USettingsWidget::HandleSfxSlider);
		MusicSlider->OnMouseCaptureEnd.AddDynamic(this, &USettingsWidget::HandleSliderReleased);
		SfxSlider->OnMouseCaptureEnd.AddDynamic(this, &USettingsWidget::HandleSliderReleased);

		RefreshFromSettings();
	}

	// 루트 캔버스는 클릭을 안 먹는다 — 빈 곳 클릭은 클릭 이동으로
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Super::RebuildWidget();
}

FReply USettingsWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빠른 두 번째 클릭이 DoubleClick으로 와서 맵 이동으로 새지 않게(UI 공통 규칙)
	if (IsCursorOverPanel()) return FReply::Handled();
	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

bool USettingsWidget::IsCursorOverPanel() const
{
	if (!IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	if (ToggleButton && ToggleButton->GetCachedGeometry().IsUnderLocation(CursorPos)) return true;
	return Panel && Panel->IsVisible() && Panel->GetCachedGeometry().IsUnderLocation(CursorPos);
}

void USettingsWidget::SetPanelOpen(bool bOpen)
{
	if (!Panel) TakeWidget();
	if (!Panel) return;
	if (bOpen) RefreshFromSettings();
	Panel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool USettingsWidget::IsPanelOpen() const
{
	return Panel && Panel->IsVisible();
}

void USettingsWidget::HandleToggleClicked()
{
	SetPanelOpen(!IsPanelOpen());
	SettingsReturnFocus();
}

void USettingsWidget::HandleCloseClicked()
{
	SetPanelOpen(false);
	SettingsReturnFocus();
}

void USettingsWidget::HandleMuteChanged(bool bIsChecked)
{
	ApplyToSettings(true);
	SettingsReturnFocus();
}

void USettingsWidget::HandleMusicSlider(float Value)
{
	ApplyToSettings(false);
}

void USettingsWidget::HandleSfxSlider(float Value)
{
	ApplyToSettings(false);
}

void USettingsWidget::HandleSliderReleased()
{
	ApplyToSettings(true);
	SettingsReturnFocus();
}

void USettingsWidget::RefreshFromSettings()
{
	const ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>();
	if (!GI || !MuteCheck || !MusicSlider || !SfxSlider) return;
	// SetValue·SetIsChecked는 OnValueChanged를 부르지 않는다 — 되먹임 없음
	MuteCheck->SetIsChecked(GI->bMusicMuted);
	MusicSlider->SetValue(GI->MusicVolumeSetting);
	SfxSlider->SetValue(GI->SfxVolumeSetting);
	UpdatePercentTexts();
}

void USettingsWidget::ApplyToSettings(bool bSave)
{
	ULoAGameInstance* GI = GetGameInstance<ULoAGameInstance>();
	if (!GI || !MuteCheck || !MusicSlider || !SfxSlider) return;
	GI->SetAudioSettings(MusicSlider->GetValue(), MuteCheck->IsChecked(), SfxSlider->GetValue(), bSave);
	UpdatePercentTexts();
}

void USettingsWidget::UpdatePercentTexts()
{
	const bool bMuted = MuteCheck && MuteCheck->IsChecked();
	if (MusicPercentText && MusicSlider)
	{
		MusicPercentText->SetText(bMuted
			? NSLOCTEXT("Settings", "Muted", "끔")
			: FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(MusicSlider->GetValue() * 100.f))));
	}
	if (SfxPercentText && SfxSlider)
	{
		SfxPercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(SfxSlider->GetValue() * 100.f))));
	}
}
