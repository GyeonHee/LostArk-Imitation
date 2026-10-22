#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Item/RaidReward.h"
#include "ClearWidget.generated.h"

class UTextBlock;
class UButton;
class UHorizontalBox;
class UTexture2D;

/**
 * 보스 처치 시 클리어 화면 — 상단 안내 + 카운트다운, 가운데 금빛 띠 + 큰 "던전 클리어", 아래 결과 줄(클리어 시간)
 * + 전리품 줄(아이콘 + 개수) + [더보기] [정비소로 이동] 버튼.
 * WBP 없이 C++만으로 트리를 만든다(DefeatWidget과 같은 방침). 카운트다운·레벨 이동·보상 지급은 ALoAPlayerController가 담당.
 * 루트는 SelfHitTestInvisible — 버튼만 클릭을 받고 나머지는 게임으로 넘긴다
 */
UCLASS()
class LOA_API UClearWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTexts(const FText& Headline, const FText& ResultLine);

	// 상단 카운트다운 숫자 — "3초"
	void SetCountdown(int32 Seconds);

	/** 전리품 줄 — 아이콘 + 개수. Note는 그 아래 작은 안내(첫 클리어·소지품 가득 등, 비우면 숨김) */
	void SetRewards(const TArray<FLoARewardItem>& Items, const FText& Note);

	/** [더보기] 버튼 — bVisible=false면 숨김, bTaken이면 "더보기 완료"로 잠금, 골드가 모자라면 비활성 */
	void SetBonusState(bool bVisible, bool bTaken, bool bAffordable, int64 Cost);

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	TSoftObjectPtr<UTexture2D> SillingIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Silling.T_Silling")));

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	TSoftObjectPtr<UTexture2D> GoldIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_Gold.T_Gold")));

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FLinearColor GlowColor = FLinearColor(1.f, 0.45f, 0.05f, 0.35f);

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FLinearColor GlowCoreColor = FLinearColor(1.f, 0.7f, 0.25f, 0.55f);

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FLinearColor AccentColor = FLinearColor(1.f, 0.72f, 0.3f, 1.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadlineText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RewardRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RewardNoteText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BonusButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BonusButtonText;

	UFUNCTION() void HandleBonusClicked();
	UFUNCTION() void HandleLeaveClicked();
};
