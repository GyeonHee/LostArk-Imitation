#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WindowDragHelper.h"
#include "Stat/ArkPassiveTypes.h"
#include "ArkPassiveWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UArkPassiveComponent;

/**
 * 아크 패시브 창 — '.' 키 또는 P창 왼쪽의 [아크 패시브] 칸으로 연다. 로아 [아크 패시브] 배치, C++ 트리(WBP 없음).
 *   위: [진화][깨달음][도약] 탭 + [추천 세팅]
 *   왼쪽: 포인트 출처 · 카르마(아직 없음) · "<트리> 아크 패시브 종합 효과"(배운 노드 목록)
 *   오른쪽: "<트리> 포인트 N / 최대" + 티어 마름모(사용 포인트/상한) + 노드(원 + nP + Lv.x/y)
 *   아래: 프리셋 줄(1번만) · [되돌리기][초기화][저장]
 * 노드 좌클릭 = +1, 우클릭 = -1, Shift = ±10 (로아와 같음). 바꾼 내용은 [저장]을 눌러야 적용 — 그 전엔 Pending에만 있다
 */
UCLASS()
class LOA_API UArkPassiveWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 열 때마다 — 지금 배치로 Pending을 초기화 */
	void Open(UArkPassiveComponent* InArkPassive);

	bool IsCursorOverPanel() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	enum class EHit : uint8 { Tab, Node, Recommend, Revert, Reset, Save, Close };
	struct FHitTarget
	{
		TWeakObjectPtr<UWidget> Widget;
		EHit Kind = EHit::Node;
		int32 Index = INDEX_NONE;
		FName Id;
	};
	TArray<FHitTarget> Targets;

	void Refresh();
	void HandleClick(const FHitTarget& Target, bool bLeft, bool bShift);
	void Notify(const FText& Message, bool bError = true) const;
	UWidget* MakeTierCell(int32 Tier);
	UWidget* MakeNodeCell(const FArkPassiveNode& Node);
	FString MakeNodeTooltip(const FArkPassiveNode& Node) const;
	EArkPassiveTree GetTree() const { return static_cast<EArkPassiveTree>(CurrentTab); }

	TWeakObjectPtr<UArkPassiveComponent> ArkPassive;
	FDelegateHandle ChangedHandle;
	TMap<FName, int32> Pending;
	int32 CurrentTab = 0;

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> CloseButton;
	UPROPERTY(Transient) TObjectPtr<UBorder> RecommendButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> TabBorders;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TabTexts;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SourceTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SourceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SummaryTitle;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> SummaryList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GuideText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PointsText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TreeRows;
	UPROPERTY(Transient) TObjectPtr<UBorder> RevertButton;
	UPROPERTY(Transient) TObjectPtr<UBorder> ResetButton;
	UPROPERTY(Transient) TObjectPtr<UBorder> SaveButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	FLoAWindowDrag WindowDrag;
};
