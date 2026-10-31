#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/AvatarTypes.h"
#include "AvatarPageWidget.generated.h"

class UBorder;
class UImage;
class UScrollBox;
class UTextBlock;
class UWrapBox;
class UWidget;
class UAvatarComponent;
class ACharacterPreviewActor;

/**
 * 캐릭터 정보 창(P) [아바타] 탭 — 왼쪽 3D 미리보기(장비 탭과 같은 렌더 타깃, 끌어서 회전),
 * 오른쪽 칸 6개(머리·어깨·상의·장갑·하의·무기, 착용 이름) + 고른 칸의 아바타 목록. 목록 클릭 = 착용, [기본] = 해제.
 * 아바타는 전부 보유(UAvatarComponent). 버튼은 Border + NativeOnMouseButtonDown 위치 판정(보석 탭과 같은 방식)
 */
UCLASS()
class LOA_API UAvatarPageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAvatar(UAvatarComponent* InAvatar);
	void SetPreviewActor(ACharacterPreviewActor* InPreview);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	enum class EHit : uint8 { SlotTab, Avatar, Default };
	struct FHitTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		EHit Kind = EHit::SlotTab;
		int32 Index = INDEX_NONE;
		FName Id;
	};
	TArray<FHitTarget> SlotTargets;
	TArray<FHitTarget> ListTargets;

	void RefreshSlots();
	void RefreshList();
	void RefreshPreview();
	void HandleAvatarChanged();
	void Unbind();

	TWeakObjectPtr<UAvatarComponent> Avatar;
	FDelegateHandle AvatarHandle;
	TWeakObjectPtr<ACharacterPreviewActor> PreviewActor;

	EAvatarSlot SelectedSlot = EAvatarSlot::Chest;
	bool bRotatingPreview = false;
	float LastPreviewMouseX = 0.f;

	UPROPERTY(Transient) TObjectPtr<UBorder> PreviewArea;
	UPROPERTY(Transient) TObjectPtr<UImage> PreviewImage;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> SlotBorders;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotNameTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotItemTexts;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ListTitle;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> ListScroll;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> ListWrap;
};
