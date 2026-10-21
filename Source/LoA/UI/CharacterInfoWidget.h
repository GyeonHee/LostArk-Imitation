#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WindowDragHelper.h"
#include "Stat/StatTypes.h"
#include "CharacterInfoWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UStatComponent;

/**
 * 캐릭터 정보 창 (P키) — 로아 [캐릭터 정보] 장비 탭 배치를 따라 C++ 트리로 만든다(WBP 없음).
 *   왼쪽: 직업·이름 프로필 + 특수장비 칸
 *   가운데: 방어구 5 + 무기 칸(왼쪽 줄) / 3D 캐릭터 미리보기(ACharacterPreviewActor, 끌어서 회전) / 장신구 5 + 팔찌 + 어빌리티 스톤 칸(오른쪽 줄)
 *   오른쪽: 장착 아이템 레벨, 기본 특성(공격력·최대 생명력), 전투 특성 6종, [상세정보](실제 % 값)
 * 수치는 UStatComponent::OnStatsChanged를 받을 때만 다시 쓴다. 장비 칸은 3단계(장비) 전까지 빈 칸.
 * 루트는 SelfHitTestInvisible — 창 밖 클릭은 게임(클릭 이동)으로, 창 안 클릭은 여기서 먹는다(소지품 창과 동일)
 */
UCLASS()
class LOA_API UCharacterInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatComponent(UStatComponent* InStats);

	/** 가운데 3D 미리보기 — 컨트롤러가 만든 미리보기 액터의 렌더 타깃을 띄운다 */
	void SetPreviewActor(class ACharacterPreviewActor* InPreview);

	// 커서가 창 위에 있는지 — 컨트롤러가 클릭 이동·NPC 호버 처리에 쓴다
	bool IsCursorOverPanel() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleDetailClicked();

	void HandleStatsChanged(const FCombatStats& NewStats);
	void Refresh();

	// ── 트리 조립 헬퍼 ──
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color, FName Name = NAME_None);
	// 장착 칸 하나의 위젯들 — 트리가 소유하고 여기선 참조만. Refresh가 아이콘·+재련·툴팁을 채운다
	struct FGearSlotWidgets
	{
		UWidget* Box = nullptr;
		UBorder* Frame = nullptr;
		class UImage* Icon = nullptr;
		UTextBlock* PartText = nullptr;
		UTextBlock* RefineText = nullptr;
		FText PartName;
	};
	TArray<FGearSlotWidgets> GearSlots; // EEquipSlot 순서

	UWidget* MakeEquipSlot(const FText& PartName, FName Name, FGearSlotWidgets* OutWidgets = nullptr);
	void RefreshGearSlots();
	UWidget* MakeSectionTitle(const FText& Title);
	UWidget* MakeStatRow(const FText& Label, UTextBlock*& OutValue, FName Name);
	UWidget* BuildProfileColumn();
	UWidget* BuildEquipmentArea();
	UWidget* BuildStatColumn();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	// 창 이동 손잡이 — 제목 줄을 좌클릭으로 끌면 창이 따라온다
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> HeaderBox;

	FLoAWindowDrag WindowDrag;

	// ── 3D 미리보기 — 좌클릭으로 끌면 캐릭터가 돈다 ──
	UPROPERTY(Transient) TObjectPtr<UBorder> PreviewArea;
	UPROPERTY(Transient) TObjectPtr<class UImage> PreviewImage;
	TWeakObjectPtr<class ACharacterPreviewActor> PreviewActor;
	bool bRotatingPreview = false;
	float LastPreviewMouseX = 0.f;

	TWeakObjectPtr<UStatComponent> StatComponent;
	FDelegateHandle StatsChangedHandle;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemLevelText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemLevelFractionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ItemLevelNoteText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AttackPowerText;
	UPROPERTY(Transient) TObjectPtr<UWidget> AttackPowerRow; // 실제 계산 공격력 툴팁용
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MaxHPText;

	// ELoACombatStat 순서(치명·특화·신속·제압·인내·숙련)로 들고 있는다 — 화면 배치 순서와는 별개
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> CombatStatTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> CombatStatRows;

	// [상세정보] — 특성을 실제 % 값으로 풀어 보여주는 칸 (기본은 접힘)
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> DetailBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailButtonText;
	bool bDetailOpen = false;
};
