#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbandonRaidWidget.generated.h"

class UButton;
class UBorder;

/**
 * 보스 맵 좌상단 "중단하기" 버튼 + 확인 창("레이드를 중단하시겠습니까?" 확인/취소).
 * 확인을 누르면 ALoAPlayerController::AbandonRaid() → 정비소 복귀.
 * WBP 없이 C++만으로 트리를 만든다. 루트는 SelfHitTestInvisible이라 빈 곳 클릭은 게임(클릭 이동)으로 그대로 간다
 */
UCLASS()
class LOA_API UAbandonRaidWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 버튼 위치 — 화면 좌상단 기준 (좌상단 초상화·정산 게이지 아래)
	UPROPERTY(EditDefaultsOnly, Category = "Abandon")
	FVector2D ButtonPosition = FVector2D(20.f, 200.f);

	void CloseConfirm();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UFUNCTION()
	void HandleAbandonClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> AbandonButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ConfirmPanel;
};
