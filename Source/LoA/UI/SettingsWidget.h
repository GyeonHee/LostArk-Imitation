#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class USlider;
class UTextBlock;

/**
 * 게임 설정 — 우측 맨 아래 [설정] 버튼 + 그 위에 뜨는 창(배경음 음소거 · 배경음 볼륨 · 효과음 볼륨).
 * 값은 ULoAGameInstance(디스크 저장)에 두고, 배경음은 OnAudioSettingsChanged로 바로, 효과음은 다음 재생부터 반영.
 * WBP 없이 C++ 트리. 루트는 SelfHitTestInvisible — 버튼·창만 클릭을 받는다(창 빈 곳 클릭은 컨트롤러가 IsCursorOverPanel로 막음)
 */
UCLASS()
class LOA_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 버튼 또는 열린 창 위에 커서가 있는지 — 클릭 이동·기본공격 차단용 */
	bool IsCursorOverPanel() const;

	void SetPanelOpen(bool bOpen);
	bool IsPanelOpen() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void HandleToggleClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleMuteChanged(bool bIsChecked);

	UFUNCTION()
	void HandleMusicSlider(float Value);

	UFUNCTION()
	void HandleSfxSlider(float Value);

	/** 슬라이더를 놓을 때 디스크 저장(끄는 동안은 매 프레임 저장하지 않게) */
	UFUNCTION()
	void HandleSliderReleased();

	/** 게임 인스턴스 값 → 체크박스·슬라이더·% 글자 */
	void RefreshFromSettings();
	void ApplyToSettings(bool bSave);
	void UpdatePercentTexts();

	UPROPERTY(Transient)
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> MuteCheck;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SfxSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MusicPercentText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SfxPercentText;
};
