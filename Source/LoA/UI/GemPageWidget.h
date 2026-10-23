#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GemPageWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UGemComponent;
class UInventoryComponent;
class USkillManagerComponent;

/**
 * 캐릭터 정보 창(P) [보석] 탭 — 로아 보석 탭 배치(추천 세팅 버튼은 뺌).
 *   왼쪽: 프리셋 줄(1번만) + 보석 11개 원(2·2 / 3 / 2·2 배치, 아래 Lv.N) — 우클릭 = 빼기 (합성은 소지품의 보석 합성 버튼)
 *   오른쪽: "장착 중인 보석 효과" — 기본 공격력 총합 + 보석마다 스킬 아이콘·이름·효과 줄
 * 장착은 소지품에서 보석을 우클릭(이 탭이 열려 있을 때) → 이 페이지 위에 스킬 선택 창 → 스킬 클릭
 * 버튼은 Border, 클릭은 NativeOnMouseButtonDown 위치 판정(목록 줄마다 다른 값을 넘기려고)
 */
UCLASS()
class LOA_API UGemPageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSources(UGemComponent* InGems, UInventoryComponent* InInventory, USkillManagerComponent* InSkills);

	/** 소지품에서 우클릭한 보석 — 스킬 선택 창을 띄운다 */
	void BeginEquip(FName GemItemId);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EHit : uint8 { GemSlot, PickSkill, PickCancel };
	struct FHitTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		EHit Kind = EHit::GemSlot;
		int32 Index = INDEX_NONE;
		FName Id;
	};
	TArray<FHitTarget> Targets;
	TArray<FHitTarget> PickerTargets;

	void Refresh();
	void RefreshPicker();
	void Notify(const FText& Message) const;
	FString GetSkillName(FName Row) const;
	class UTexture2D* GetSkillIcon(FName Row) const;
	TArray<FName> GetSkillRows() const;

	void HandleChanged() { Refresh(); }
	void Unbind();

	TWeakObjectPtr<UGemComponent> Gems;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<USkillManagerComponent> Skills;
	FDelegateHandle GemsHandle;

	FName PendingGem;  // 스킬 선택 창이 떠 있으면 그 보석

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Board;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AttackTotalText;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> EffectList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UBorder> Picker;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> PickerList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PickerTitle;
};
