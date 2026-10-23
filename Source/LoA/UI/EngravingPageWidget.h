#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EngravingPageWidget.generated.h"

class UBorder;
class USizeBox;
class UScrollBox;
class UTextBlock;
class UWidget;
class UEngravingComponent;

/**
 * 캐릭터 정보 창(P) [각인] 탭 — 로아 각인 탭 배치.
 *   왼쪽: 프리셋 줄(1번만) + 각인 판(정오각형 5칸, 장착이면 파란 테두리 + 이름 + ◆단계) + [각인 효과 설정하기] → UEngravingSetupWidget
 *   오른쪽: "장착 중인 각인" — 각인마다 주황 이름 + ◆ x 단계, 그 아래 효과 이름 / 값 줄
 * 이 탭에선 보기만 한다(장착은 각인 효과 창, 각인서는 소지품 우클릭으로 읽기)
 */
UCLASS()
class LOA_API UEngravingPageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEngravings(UEngravingComponent* InEngravings);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void Refresh();
	void HandleChanged() { Refresh(); }
	void Unbind();

	TWeakObjectPtr<UEngravingComponent> Engravings;
	FDelegateHandle Handle;

	UPROPERTY(Transient) TObjectPtr<USizeBox> BoardBox;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> EffectList;
	UPROPERTY(Transient) TObjectPtr<UBorder> SetupButton;
};
