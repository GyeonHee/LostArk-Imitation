#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClashQTEWidget.generated.h"

class UCanvasPanel;
class UHorizontalBox;
class UVerticalBox;
class UTextBlock;
class UImage;
class UProgressBar;
class USizeBox;

/**
 * 0줄 발악 격돌 QTE 화면 — AEchidnaClashQTEActor가 만들고 값만 넣는다(판정 로직 없음). WBP 없이 C++ 트리.
 * 타이밍 단계: 키 원(노트)들이 한 줄로 뜨고, 각 노트 바깥 링이 줄어들어 키 원 테두리에 닿는 순간이 판정 시각.
 * 연타 단계: 키 두 개 + 게이지 바. 다음에 눌러야 할 키가 커지고 밝아진다.
 * 판정 글자·연타 박동은 NativeTick에서 튀었다가 가라앉는다
 */
UCLASS()
class LOA_API UClashQTEWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStageText(const FText& Text);

	/** 타이밍 단계 시작 — 노트를 Keys 순서대로 새로 깐다 (연타 패널은 숨김) */
	void ShowNotes(const TArray<FKey>& Keys);

	/** 0 = 링이 가장 큼(막 나타남) ~ 1 = 판정 시각(키 원과 겹침). 1을 넘으면 링을 숨긴다 */
	void SetNoteApproach(int32 Index, float Alpha);

	/** 0 대기, 1 Perfect, 2 Good, 3 Miss — 키 원 색이 바뀌고 링이 사라진다 */
	void SetNoteResult(int32 Index, int32 Result);

	/** 가운데 큰 판정 글자 (PERFECT / GOOD / MISS / 성공!) — 튀어 오르며 나타남 */
	void ShowJudge(const FText& Text, const FLinearColor& Color);

	/** 연타 단계 시작 — 키 두 개 표시, 노트 줄은 비움 */
	void ShowMash(const FKey& KeyA, const FKey& KeyB);

	/** 게이지(0~1), 다음 키(0/1), 남은 시간 */
	void SetMash(float Gauge, int32 NextKeyIndex, float TimeLeft);

	/** 연타 한 번 맞음 — 게이지·키가 한 번 튄다 */
	void PulseMash();

	void HideAll();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FNoteWidgets
	{
		TObjectPtr<UWidget> Root;
		TObjectPtr<UImage> Circle;
		TObjectPtr<UImage> Ring;
		TObjectPtr<UTextBlock> KeyText;
	};

	void EnsureTree();
	UImage* MakeCircleImage(const FName& Name, float Size, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth);
	UTextBlock* MakeText(const FName& Name, int32 Size, const FLinearColor& Color);
	void SetCircleColors(UImage* Image, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StageText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> NotesRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> JudgeText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MashPanel;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> MashKeysRow;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> MashBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MashTimeText;

	// 위젯 포인터는 트리(WidgetTree)가 들고 있어 GC 안전 — 이 배열은 접근용
	TArray<FNoteWidgets> Notes;
	TArray<FNoteWidgets> MashKeys;

	float JudgePop = 0.f;
	float MashPop = 0.f;
	int32 MashNextIndex = 0;

	static constexpr float NoteSize = 96.f;
	static constexpr float RingMaxScale = 2.6f;
};
