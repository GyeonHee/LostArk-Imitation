#include "UI/DamageMeterWidget.h"
#include "Stat/DamageMeterComponent.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Item/ItemTypes.h"
#include "LoAPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	const FLinearColor Meter_PanelColor(0.028f, 0.032f, 0.045f, 0.92f);
	const FLinearColor Meter_Gold(0.93f, 0.80f, 0.52f, 1.f);
	const FLinearColor Meter_Label(0.75f, 0.76f, 0.80f, 1.f);
	const FLinearColor Meter_Value(0.96f, 0.96f, 0.96f, 1.f);
	const FLinearColor Meter_BarColor(0.62f, 0.38f, 0.14f, 0.85f);

	constexpr float Meter_PanelWidth = 340.f;
	constexpr float Meter_RowHeight = 24.f;
	constexpr float Meter_RefreshInterval = 0.25f;

	// 로아식 큰 숫자 — 1억 이상 "47.47억", 1만 이상 "2,670만", 그 아래는 그대로
	FString Meter_FormatAmount(double Value)
	{
		if (Value >= 1e8) return FString::Printf(TEXT("%.2f억"), Value / 1e8);
		if (Value >= 1e4) return LoAItems::FormatWithCommas(static_cast<int64>(Value / 1e4)) + TEXT("만");
		return LoAItems::FormatWithCommas(static_cast<int64>(Value));
	}

	FString Meter_FormatTime(double Seconds)
	{
		const int32 Total = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}

	void Meter_FlatButtonStyle(UButton* Button)
	{
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.06f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.18f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.3f));
		Style.NormalPadding = FMargin(6.f, 2.f);
		Style.PressedPadding = FMargin(6.f, 2.f);
		Button->SetStyle(Style);
	}
}

UTextBlock* UDamageMeterWidget::MakeText(const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

TSharedRef<SWidget> UDamageMeterWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DamageMeterRoot"));
		WidgetTree->RootWidget = Root;

		// 창 — 화면 오른쪽, 미니맵(우상단 220px) 아래
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DamageMeterPanel"));
		Panel->SetBrushColor(Meter_PanelColor);
		Panel->SetPadding(FMargin(10.f, 4.f, 10.f, 10.f));
		Panel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAutoSize(true);
			S->SetAnchors(FAnchors(1.f, 0.f));
			S->SetAlignment(FVector2D(1.f, 0.f));
			S->SetPosition(FVector2D(-20.f, 270.f));
		}

		USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PanelSize->SetWidthOverride(Meter_PanelWidth);
		Panel->SetContent(PanelSize);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		PanelSize->SetContent(Column);

		// ── 헤더: 제목 · [초기화] · X (제목 줄을 끌면 창 이동) ──
		HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HeaderBox"));
		HeaderBox->SetHeightOverride(30.f);
		Column->AddChildToVerticalBox(HeaderBox);

		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		HeaderBox->SetContent(Header);
		if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(MakeText(TEXT("전투 분석"), 13, FLinearColor(0.92f, 0.87f, 0.72f))))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		UButton* ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResetButton"));
		Meter_FlatButtonStyle(ResetButton);
		ResetButton->SetContent(MakeText(TEXT("초기화"), 10, Meter_Label));
		ResetButton->OnClicked.AddDynamic(this, &UDamageMeterWidget::HandleResetClicked);
		if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(ResetButton))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}

		UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		Meter_FlatButtonStyle(CloseButton);
		CloseButton->SetContent(MakeText(TEXT("X"), 11, Meter_Label));
		CloseButton->OnClicked.AddDynamic(this, &UDamageMeterWidget::HandleCloseClicked);
		if (UHorizontalBoxSlot* S = Header->AddChildToHorizontalBox(CloseButton))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Divider->SetBrushColor(FLinearColor(0.42f, 0.36f, 0.22f, 1.f));
		Divider->SetPadding(FMargin(0.f, 0.5f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Divider))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}

		// ── 요약: 2열 × 2줄 (라벨 금색 / 값 흰색) ──
		auto AddSummaryRow = [this, Column](const TCHAR* LabelA, TObjectPtr<UTextBlock>& ValueA, const TCHAR* LabelB, TObjectPtr<UTextBlock>& ValueB)
		{
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Line))
			{
				S->SetPadding(FMargin(0.f, 1.f));
			}
			auto AddPair = [this, Line](const TCHAR* Label, TObjectPtr<UTextBlock>& Value)
			{
				if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(MakeText(Label, 10, Meter_Gold)))
				{
					S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				}
				Value = MakeText(TEXT("-"), 11, Meter_Value);
				if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Value))
				{
					S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			};
			AddPair(LabelA, ValueA);
			AddPair(LabelB, ValueB);
		};
		AddSummaryRow(TEXT("전투 시간"), TimeText, TEXT("총 피해"), TotalText);
		AddSummaryRow(TEXT("DPS"), DPSText, TEXT("치명률"), CritText);

		UBorder* Divider2 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Divider2->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
		Divider2->SetPadding(FMargin(0.f, 0.5f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Divider2))
		{
			S->SetPadding(FMargin(0.f, 6.f));
		}

		EmptyText = MakeText(TEXT("허수아비나 보스를 공격하면 기록됩니다"), 10, FLinearColor(0.45f, 0.45f, 0.48f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(EmptyText))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 6.f));
		}

		// ── 스킬 행 MaxRows개를 미리 만들어 두고 Refresh가 채우거나 숨긴다 ──
		Rows.Reset();
		for (int32 i = 0; i < MaxRows; ++i)
		{
			FRowWidgets Row;

			USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			RowSize->SetHeightOverride(Meter_RowHeight);
			Row.Root = RowSize;
			if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(RowSize))
			{
				S->SetPadding(FMargin(0.f, 1.f));
			}

			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			RowSize->SetContent(Line);

			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			IconBox->SetWidthOverride(Meter_RowHeight);
			IconBox->SetHeightOverride(Meter_RowHeight);
			Row.Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			IconBox->SetContent(Row.Icon);
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(IconBox))
			{
				S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
			}

			// 막대 위에 글자를 겹친다 — 막대 길이 = 1등 스킬 대비 비율
			UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(BarOverlay))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}

			Row.Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
			FProgressBarStyle BarStyle = Row.Bar->GetWidgetStyle();
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.05f));
			BarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
			Row.Bar->SetWidgetStyle(BarStyle);
			Row.Bar->SetFillColorAndOpacity(Meter_BarColor);
			if (UOverlaySlot* S = BarOverlay->AddChildToOverlay(Row.Bar))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
				S->SetVerticalAlignment(VAlign_Fill);
			}

			UHorizontalBox* Texts = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UOverlaySlot* S = BarOverlay->AddChildToOverlay(Texts))
			{
				S->SetHorizontalAlignment(HAlign_Fill);
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f));
			}
			Row.Name = MakeText(TEXT(""), 10, Meter_Value);
			if (UHorizontalBoxSlot* S = Texts->AddChildToHorizontalBox(Row.Name))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			Row.Share = MakeText(TEXT(""), 10, Meter_Gold);
			if (UHorizontalBoxSlot* S = Texts->AddChildToHorizontalBox(Row.Share))
			{
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			Row.DPS = MakeText(TEXT(""), 10, Meter_Value);
			Texts->AddChildToHorizontalBox(Row.DPS);

			RowSize->SetVisibility(ESlateVisibility::Collapsed);
			Rows.Add(Row);
		}
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

void UDamageMeterWidget::SetSources(UDamageMeterComponent* InMeter, USkillManagerComponent* InSkills)
{
	SkillManager = InSkills;
	if (Meter.Get() != InMeter)
	{
		if (UDamageMeterComponent* Old = Meter.Get())
		{
			Old->OnChanged.Remove(MeterChangedHandle);
		}
		MeterChangedHandle.Reset();
		Meter = InMeter;
		if (InMeter)
		{
			MeterChangedHandle = InMeter->OnChanged.AddUObject(this, &UDamageMeterWidget::Refresh);
		}
	}
	Refresh();
}

void UDamageMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 전투 시간·DPS는 피격이 없어도 흐르므로 주기적으로 다시 쓴다
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= Meter_RefreshInterval)
	{
		RefreshAccum = 0.f;
		Refresh();
	}
}

FText UDamageMeterWidget::GetSkillDisplayName(FName SkillRowName) const
{
	if (SkillRowName.IsNone()) return NSLOCTEXT("DamageMeter", "Other", "기타");
	const USkillManagerComponent* Skills = SkillManager.Get();
	if (Skills && Skills->SkillDataTable)
	{
		if (const FSkillData* Row = Skills->SkillDataTable->FindRow<FSkillData>(SkillRowName, TEXT("DamageMeter"), false))
		{
			if (!Row->SkillName.IsEmpty()) return Row->SkillName;
		}
	}
	return FText::FromName(SkillRowName);
}

UTexture2D* UDamageMeterWidget::GetSkillIcon(FName SkillRowName)
{
	if (const TObjectPtr<UTexture2D>* Cached = IconCache.Find(SkillRowName))
	{
		return *Cached;
	}
	UTexture2D* Icon = nullptr;
	const USkillManagerComponent* Skills = SkillManager.Get();
	if (Skills && Skills->SkillDataTable && !SkillRowName.IsNone())
	{
		if (const FSkillData* Row = Skills->SkillDataTable->FindRow<FSkillData>(SkillRowName, TEXT("DamageMeter"), false))
		{
			Icon = Row->Icon.LoadSynchronous();
		}
	}
	IconCache.Add(SkillRowName, Icon);
	return Icon;
}

void UDamageMeterWidget::Refresh()
{
	if (!TimeText || !IsVisible()) return;

	const UDamageMeterComponent* M = Meter.Get();
	const bool bHasData = M && M->HasSession();

	TimeText->SetText(FText::FromString(bHasData ? Meter_FormatTime(M->GetElapsedSeconds()) : TEXT("-")));
	TotalText->SetText(FText::FromString(bHasData ? Meter_FormatAmount(M->GetTotalDamage()) : TEXT("-")));
	DPSText->SetText(FText::FromString(bHasData ? Meter_FormatAmount(M->GetDPS()) : TEXT("-")));
	CritText->SetText(FText::FromString(bHasData && M->GetTotalHits() > 0
		? FString::Printf(TEXT("%.1f%%"), 100.0 * M->GetTotalCritHits() / M->GetTotalHits())
		: TEXT("-")));

	// 세션이 끝난(5초 무피격) 허수아비 기록은 흐리게 — 다음 타격이 새로 시작한다는 표시
	const float Dim = (bHasData && !M->IsSessionActive()) ? 0.6f : 1.f;
	DPSText->SetRenderOpacity(Dim);
	TimeText->SetRenderOpacity(Dim);

	EmptyText->SetVisibility(bHasData ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	const TArray<FDamageMeterSkillEntry> Entries = M ? M->GetSortedEntries() : TArray<FDamageMeterSkillEntry>();
	const double Top = Entries.Num() > 0 ? Entries[0].TotalDamage : 0.0;
	const double Total = M ? M->GetTotalDamage() : 0.0;
	const double Seconds = M ? FMath::Max(1.0, M->GetElapsedSeconds()) : 1.0;

	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		FRowWidgets& Row = Rows[i];
		if (!Entries.IsValidIndex(i))
		{
			Row.Root->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FDamageMeterSkillEntry& Entry = Entries[i];
		Row.Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Row.Name->SetText(GetSkillDisplayName(Entry.SkillRowName));
		Row.Share->SetText(FText::FromString(FString::Printf(TEXT("%.1f%%"), Total > 0.0 ? Entry.TotalDamage / Total * 100.0 : 0.0)));
		Row.DPS->SetText(FText::FromString(Meter_FormatAmount(Entry.TotalDamage / Seconds)));
		Row.Bar->SetPercent(Top > 0.0 ? static_cast<float>(Entry.TotalDamage / Top) : 0.f);

		if (UTexture2D* Icon = GetSkillIcon(Entry.SkillRowName))
		{
			Row.Icon->SetBrushFromTexture(Icon);
			Row.Icon->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Row.Icon->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
		}
	}
}

bool UDamageMeterWidget::IsCursorOverPanel() const
{
	if (!Panel || !IsInViewport() || !IsVisible() || !FSlateApplication::IsInitialized()) return false;
	return Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void UDamageMeterWidget::HandleResetClicked()
{
	if (UDamageMeterComponent* M = Meter.Get())
	{
		M->ResetSession();
	}
}

void UDamageMeterWidget::HandleCloseClicked()
{
	if (ALoAPlayerController* PC = GetOwningPlayer<ALoAPlayerController>())
	{
		PC->SetDamageMeterOpen(false);
	}
}

FReply UDamageMeterWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HeaderBox
		&& HeaderBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())
		&& WindowDrag.Begin(Panel, InGeometry, InMouseEvent))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	// 창 안 클릭은 먹는다 (루트가 SelfHitTestInvisible이라 창 밖 클릭은 여기까지 안 옴)
	return FReply::Handled();
}

FReply UDamageMeterWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 두 번째 클릭은 DoubleClick으로 온다 — 안 먹으면 창 뒤의 맵으로 클릭 이동이 샌다
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDamageMeterWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!WindowDrag.bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	WindowDrag.Move(Panel, InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UDamageMeterWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (WindowDrag.bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		WindowDrag.End();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UDamageMeterWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	WindowDrag.End();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
