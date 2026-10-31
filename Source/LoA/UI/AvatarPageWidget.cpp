#include "UI/AvatarPageWidget.h"
#include "UI/BuildUIHelpers.h"
#include "UI/CharacterPreviewActor.h"
#include "Item/AvatarComponent.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Character.h"

using namespace LoABuildUI;

namespace
{
	constexpr float Avatar_PreviewHeight = 500.f;
	constexpr float Avatar_PreviewWidth = Avatar_PreviewHeight * 2.f / 3.f;
	constexpr float Avatar_TileWidth = 128.f;
	constexpr float Avatar_TileHeight = 40.f;
	constexpr float Avatar_RotateSpeed = 0.6f;	// 1px 끌 때 몇 도 (장비 탭과 같음)
	const FLinearColor Avatar_TileEquipped(0.36f, 0.29f, 0.14f, 1.f);
}

TSharedRef<SWidget> UAvatarPageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AvatarPageRoot"));
		WidgetTree->RootWidget = Root;

		// ── 왼쪽: 3D 미리보기 ──
		PreviewArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PreviewArea->SetBrushColor(FLinearColor::Black);
		PreviewArea->SetPadding(FMargin(0.f));
		PreviewArea->SetHorizontalAlignment(HAlign_Center);
		PreviewArea->SetVerticalAlignment(VAlign_Center);
		PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		PreviewImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));	// 렌더 타깃이 붙기 전엔 숨김
		FSlateBrush PreviewBrush;
		PreviewBrush.ImageSize = FVector2D(Avatar_PreviewWidth, Avatar_PreviewHeight);
		PreviewImage->SetBrush(PreviewBrush);
		PreviewArea->SetContent(PreviewImage);
		USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PreviewSize->SetWidthOverride(Avatar_PreviewWidth);
		PreviewSize->SetContent(PreviewArea);
		Root->AddChildToHorizontalBox(PreviewSize);

		// ── 오른쪽: 칸 6개 + 목록 ──
		UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(Right))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}

		UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Right->AddChildToVerticalBox(SlotRow);
		for (int32 i = 0; i < LoAAvatars::SlotCount; ++i)
		{
			UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			SlotBorder->SetPadding(FMargin(4.f, 6.f));
			SlotBorder->SetHorizontalAlignment(HAlign_Center);
			UVerticalBox* SlotColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			SlotBorder->SetContent(SlotColumn);
			UTextBlock* NameText = MakeText(WidgetTree, LoAAvatars::GetSlotName(static_cast<EAvatarSlot>(i)).ToString(), 12, UiLabel);
			if (UVerticalBoxSlot* S = SlotColumn->AddChildToVerticalBox(NameText)) S->SetHorizontalAlignment(HAlign_Center);
			UTextBlock* ItemText = MakeText(WidgetTree, TEXT(""), 9, UiDim);
			if (UVerticalBoxSlot* S = SlotColumn->AddChildToVerticalBox(ItemText)) S->SetHorizontalAlignment(HAlign_Center);
			if (UHorizontalBoxSlot* S = SlotRow->AddChildToHorizontalBox(SlotBorder))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetPadding(FMargin(i == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f));
			}
			SlotBorders.Add(SlotBorder);
			SlotNameTexts.Add(NameText);
			SlotItemTexts.Add(ItemText);

			FHitTarget& Target = SlotTargets.AddDefaulted_GetRef();
			Target.Widget = SlotBorder;
			Target.Kind = EHit::SlotTab;
			Target.Index = i;
		}

		ListTitle = MakeText(WidgetTree, TEXT(""), 12, UiGold);
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(ListTitle)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f));

		UBorder* ListBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ListBack->SetBrushColor(BoardColor);
		ListBack->SetPadding(FMargin(8.f));
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(ListBack)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		ListBack->SetContent(ListScroll);
		ListWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		ListWrap->SetInnerSlotPadding(FVector2D(6.f, 6.f));
		ListScroll->AddChild(ListWrap);

		UTextBlock* Hint = MakeText(WidgetTree, TEXT("아바타를 클릭하면 바로 입습니다. [기본] = 벗기. 미리보기는 끌어서 돌리기"), 9, UiDim);
		if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(Hint))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();
	RefreshSlots();
	RefreshList();
	return Result;
}

void UAvatarPageWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UAvatarPageWidget::Unbind()
{
	if (UAvatarComponent* Old = Avatar.Get())
	{
		Old->OnAvatarChanged.Remove(AvatarHandle);
	}
	AvatarHandle.Reset();
}

void UAvatarPageWidget::SetAvatar(UAvatarComponent* InAvatar)
{
	if (Avatar.Get() == InAvatar) return;
	Unbind();
	Avatar = InAvatar;
	if (InAvatar)
	{
		AvatarHandle = InAvatar->OnAvatarChanged.AddUObject(this, &UAvatarPageWidget::HandleAvatarChanged);
	}
	if (!ListWrap) TakeWidget();
	RefreshSlots();
	RefreshList();
}

void UAvatarPageWidget::SetPreviewActor(ACharacterPreviewActor* InPreview)
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

void UAvatarPageWidget::HandleAvatarChanged()
{
	RefreshSlots();
	RefreshList();
	RefreshPreview();
}

void UAvatarPageWidget::RefreshPreview()
{
	// 미리보기 액터는 열 때 한 번 복사하므로 갈아입을 때마다 다시
	ACharacterPreviewActor* Preview = PreviewActor.Get();
	const UAvatarComponent* Comp = Avatar.Get();
	if (Preview && Comp)
	{
		Preview->SetupFromCharacter(Cast<ACharacter>(Comp->GetOwner()));
	}
}

void UAvatarPageWidget::RefreshSlots()
{
	const UAvatarComponent* Comp = Avatar.Get();
	for (int32 i = 0; i < SlotBorders.Num(); ++i)
	{
		const bool bSelected = i == static_cast<int32>(SelectedSlot);
		if (SlotBorders[i]) SlotBorders[i]->SetBrushColor(bSelected ? FLinearColor(0.20f, 0.15f, 0.08f, 1.f) : SectionColor);
		if (SlotNameTexts.IsValidIndex(i) && SlotNameTexts[i]) SlotNameTexts[i]->SetColorAndOpacity(FSlateColor(bSelected ? UiGold : UiLabel));
		if (SlotItemTexts.IsValidIndex(i) && SlotItemTexts[i])
		{
			const FLoAAvatarDef* Def = Comp ? LoAAvatars::Find(Comp->GetEquipped(static_cast<EAvatarSlot>(i))) : nullptr;
			SlotItemTexts[i]->SetText(Def ? Def->Name : NSLOCTEXT("Avatar", "DefaultLook", "기본"));
		}
	}
}

void UAvatarPageWidget::RefreshList()
{
	if (!ListWrap) return;
	ListWrap->ClearChildren();
	ListTargets.Reset();

	const TArray<const FLoAAvatarDef*> Defs = LoAAvatars::ForSlot(SelectedSlot);
	if (ListTitle)
	{
		ListTitle->SetText(FText::Format(NSLOCTEXT("Avatar", "ListTitle", "{0} 아바타 ({1})"),
			LoAAvatars::GetSlotName(SelectedSlot), FText::AsNumber(Defs.Num())));
	}

	const FName EquippedId = Avatar.IsValid() ? Avatar->GetEquipped(SelectedSlot) : NAME_None;
	auto AddTile = [this](const FString& Label, bool bOn, EHit Kind, FName Id)
	{
		UBorder* Tile = MakeButton(WidgetTree, Label, 10, bOn ? Avatar_TileEquipped : ButtonColor, bOn ? UiGold : UiLabel);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Avatar_TileWidth);
		Box->SetHeightOverride(Avatar_TileHeight);
		Box->SetContent(Tile);
		ListWrap->AddChildToWrapBox(Box);

		FHitTarget& Target = ListTargets.AddDefaulted_GetRef();
		Target.Widget = Tile;
		Target.Kind = Kind;
		Target.Id = Id;
	};

	AddTile(TEXT("기본"), EquippedId.IsNone(), EHit::Default, NAME_None);
	for (const FLoAAvatarDef* Def : Defs)
	{
		AddTile(Def->Name.ToString(), Def->Id == EquippedId, EHit::Avatar, Def->Id);
	}
}

FReply UAvatarPageWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}
	const FVector2D Pos = InMouseEvent.GetScreenSpacePosition();

	for (const FHitTarget& Target : SlotTargets)
	{
		const UWidget* W = Target.Widget.Get();
		if (W && W->GetCachedGeometry().IsUnderLocation(Pos))
		{
			SelectedSlot = static_cast<EAvatarSlot>(Target.Index);
			RefreshSlots();
			RefreshList();
			if (ListScroll) ListScroll->ScrollToStart();
			return FReply::Handled();
		}
	}

	for (const FHitTarget& Target : ListTargets)
	{
		const UWidget* W = Target.Widget.Get();
		if (!W || !W->GetCachedGeometry().IsUnderLocation(Pos)) continue;
		// 착용하면 목록(ListTargets)이 다시 만들어지므로 값을 먼저 복사하고 곧바로 빠져나간다
		const EHit Kind = Target.Kind;
		const FName Id = Target.Id;
		if (UAvatarComponent* Comp = Avatar.Get())
		{
			if (Kind == EHit::Default) Comp->Unequip(SelectedSlot);
			else Comp->Equip(Id);
		}
		return FReply::Handled();
	}

	if (PreviewArea && PreviewActor.IsValid() && PreviewArea->GetCachedGeometry().IsUnderLocation(Pos))
	{
		bRotatingPreview = true;
		LastPreviewMouseX = Pos.X;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UAvatarPageWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 빠른 두 번째 클릭도 같은 처리 — 안 그러면 게임(클릭 이동)으로 샌다
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAvatarPageWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRotatingPreview)
	{
		const float MouseX = InMouseEvent.GetScreenSpacePosition().X;
		if (ACharacterPreviewActor* Preview = PreviewActor.Get())
		{
			Preview->AddPreviewYaw(-(MouseX - LastPreviewMouseX) * Avatar_RotateSpeed);
		}
		LastPreviewMouseX = MouseX;
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UAvatarPageWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRotatingPreview && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bRotatingPreview = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UAvatarPageWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bRotatingPreview = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
