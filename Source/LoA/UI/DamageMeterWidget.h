#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WindowDragHelper.h"
#include "DamageMeterWidget.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UDamageMeterComponent;
class USkillManagerComponent;
class UTexture2D;

/**
 * 데미지 미터 창 (L키, 허수아비를 때리면 자동으로 열림) — 화면 오른쪽 중단, C++ 트리(WBP 없음).
 *  상단: 전투 시간 · 총 피해 · DPS · 치명률 / 본문: 스킬별 한 줄(아이콘 · 이름 · 딜 비중 막대 · 비중% · DPS), 비중 순
 * 숫자는 0.25초마다 다시 쓴다(전투 시간·DPS가 계속 변해서). 루트 SelfHitTestInvisible — 창 밖 클릭은 게임으로
 */
UCLASS()
class LOA_API UDamageMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSources(UDamageMeterComponent* InMeter, USkillManagerComponent* InSkills);

	bool IsCursorOverPanel() const;

	static constexpr int32 MaxRows = 10;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void Refresh();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color);
	FText GetSkillDisplayName(FName SkillRowName) const;
	UTexture2D* GetSkillIcon(FName SkillRowName);

	// 행 위젯들은 WidgetTree가 소유한다 — 여기선 참조만
	struct FRowWidgets
	{
		UWidget* Root = nullptr;
		UImage* Icon = nullptr;
		UProgressBar* Bar = nullptr;
		UTextBlock* Name = nullptr;
		UTextBlock* Share = nullptr;
		UTextBlock* DPS = nullptr;
	};
	TArray<FRowWidgets> Rows;

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<USizeBox> HeaderBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TotalText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DPSText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CritText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EmptyText;

	// 스킬 아이콘 캐시 (DT의 소프트 참조를 한 번만 로드)
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UTexture2D>> IconCache;

	TWeakObjectPtr<UDamageMeterComponent> Meter;
	TWeakObjectPtr<USkillManagerComponent> SkillManager;
	FDelegateHandle MeterChangedHandle;

	FLoAWindowDrag WindowDrag;
	float RefreshAccum = 0.f;
};
