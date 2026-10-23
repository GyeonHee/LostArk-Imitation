#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillTooltipWidget.generated.h"

class UBorder;
class UVerticalBox;
class USkillManagerComponent;

/**
 * 스킬 툴팁 — HUD 스킬 칸·스킬트리(K) 줄 위에 마우스를 올리면 커서 옆에 뜬다(로아 스킬 툴팁 배치).
 *   이름 / [아이콘] 스킬 종류·재사용 대기시간 / 스킬 레벨·마나 소모 / 설명(현재 스펙의 피해 수치) / 카운터·캐스팅 시간 / 장착 보석
 * WBP 없이 C++ 트리, HitTestInvisible(클릭을 막지 않음). 컨트롤러가 매 Tick 커서 아래 스킬을 찾아 ShowSkill/HideTooltip
 * 설명 문구는 cpp의 표(행 이름별) — DT_Skills에 설명 칸이 없어서. 피해 수치는 {D} 자리에 ULoADamageLibrary::CalcOutgoingDamage(치명 전)
 * 내용은 마우스를 올린 스킬이 바뀔 때 한 번만 만든다(Tick에선 위치만). 재사용 대기시간·캐스팅 시간은 쿨감·속도 적용 전 기본값.
 * 예전엔 0.5초마다 통째로 지우고 다시 만들어서, 새 내용의 크기가 계산되기 전 프레임에 옛 크기로 자리를 잡아 글이 겹쳐 깜빡였다
 */
UCLASS()
class LOA_API USkillTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 행의 툴팁을 커서 옆에 — 같은 행이면 위치만 옮긴다 */
	void ShowSkill(FName RowName, USkillManagerComponent* Skills);
	void HideTooltip();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Build(FName RowName, USkillManagerComponent* Skills);

	FName ShownRow;

	UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Content;
	TWeakObjectPtr<USkillManagerComponent> ShownSkills;
};
