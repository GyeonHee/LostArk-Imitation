#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourceBarWidget.generated.h"

class UTextBlock;
class UInventoryComponent;

/**
 * 화면 좌상단 자원 띠 — 로아 상단 바에서 현재 시간 · 실링 · 골드 · 운명의 파편만 가져옴.
 * C++ 트리(WBP 없음). 루트가 HitTestInvisible이라 클릭 이동을 막지 않는다.
 * 값은 인벤토리 델리게이트로만 갱신하고, 시계는 분이 바뀔 때만 글자를 바꾼다.
 * 폰이 늦게 빙의되거나 레벨 이동으로 바뀌어도 NativeTick이 지금 폰의 인벤토리로 다시 묶는다.
 */
UCLASS()
class LOA_API UResourceBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 화면 좌상단 모서리에서 떨어진 거리 (슬레이트 단위)
	UPROPERTY(EditDefaultsOnly, Category = "ResourceBar")
	FVector2D ScreenOffset = FVector2D(12.f, 8.f);

	UPROPERTY(EditDefaultsOnly, Category = "ResourceBar")
	TSoftObjectPtr<class UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

	UPROPERTY(EditDefaultsOnly, Category = "ResourceBar")
	TSoftObjectPtr<class UTexture2D> GoldIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Gold.T_Gold")));

	UPROPERTY(EditDefaultsOnly, Category = "ResourceBar")
	TSoftObjectPtr<class UTexture2D> FragmentIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_FateFragment.T_FateFragment")));

	/** 띠 높이(배경 아래쪽 끝) — 같은 좌상단을 쓰는 다른 UI를 이만큼 내릴 때 쓴다 */
	float GetBottomY() const { return ScreenOffset.Y + 28.f; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void BindInventory(UInventoryComponent* InInventory);
	void HandleSillingChanged(int64 NewAmount);
	void HandleGoldChanged(int64 NewAmount);
	void HandleSlotChanged(int32 Index);
	void RefreshClock();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SillingText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FragmentText;

	TWeakObjectPtr<UInventoryComponent> Inventory;
	FDelegateHandle SillingHandle;
	FDelegateHandle GoldHandle;
	FDelegateHandle SlotHandle;

	// 마지막으로 표시한 시:분 (분이 바뀔 때만 갱신)
	int32 ShownMinuteOfDay = -1;
};
