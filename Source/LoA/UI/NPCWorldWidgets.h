#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPCWorldWidgets.generated.h"

class UTextBlock;

/**
 * NPC 머리 위 이름표 — 위에 초록 칭호("[물약 상점]"), 아래에 이름. WBP 없이 C++ 트리(ZoneLabelWidget과 같은 방침).
 * AShopNPCActor의 WidgetComponent(Screen)가 쓴다
 */
UCLASS()
class LOA_API UNPCNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetNames(const FText& Title, const FText& Name);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;
};

/**
 * 상호작용 안내 — [G] 대화하기. 플레이어가 NPC 상호작용 거리 안에 들어오면 NPC 옆에 뜬다
 */
UCLASS()
class LOA_API UInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPrompt(const FText& Key, const FText& Action);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionText;
};
