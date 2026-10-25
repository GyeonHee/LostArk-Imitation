#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stat/ArkPassiveTypes.h"
#include "ArkPassiveComponent.generated.h"

class UStatComponent;

DECLARE_MULTICAST_DELEGATE(FOnArkPassiveChanged);

/**
 * 아크 패시브 배치(노드별 레벨)와 효과. 효과는 UStatComponent 수정치 두 출처로:
 *   "ArkPassive"     — 레벨만 있으면 붙는 상시 효과 (LoAArkPassive::BuildStaticModifiers)
 *   "ArkPassiveBuff" — 발동형: 달인(스킬 사용 시 중첩) / 마력 충전·응집되는 마력(점멸 사용 시)
 * 환류의 기운(점멸 시 마나 7% 회복)도 여기서. 바꾸기는 각인과 같은 규칙 — 보스 맵 밖에서만, [저장]을 눌러야 적용
 * 포인트는 장착 장비에서 계산(LoAArkPassive::ComputeBudget) — 장비가 바뀔 때마다 + BeginPlay 다음 틱에 다시
 * (컴포넌트 BeginPlay 순서는 보장되지 않아 장비 장착 전에 계산될 수 있다). 장비를 벗어 쓴 포인트가 모자라면
 * 배치는 그대로 두고 낮은 티어·왼쪽 노드부터 들어가는 만큼만 적용된다(창에 "포인트 부족" 표시)
 * 첫 실행(또는 아크 패시브 이전 저장)엔 추천 세팅(점화)으로 시작 — K(AttackPowerScale)가 이 배치를 기준으로 맞춰져 있다
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API UArkPassiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArkPassiveComponent();

	const TMap<FName, int32>& GetLevels() const { return Levels; }
	int32 GetLevel(FName Id) const { return LoAArkPassive::GetLevel(Levels, Id); }

	/** 지금 장비로 쓸 수 있는 포인트 */
	const FArkPassiveBudget& GetBudget() const { return Budget; }

	/** 그 트리에 쓴 포인트가 지금 포인트 안인지 — 아니면 앞 티어부터 들어가는 만큼만 적용된다 */
	bool IsTreeActive(EArkPassiveTree Tree) const;

	/** 실제로 적용 중인 레벨 (포인트가 모자라면 낮은 티어·왼쪽 노드부터 들어가는 만큼) */
	const TMap<FName, int32>& GetEffectiveLevels() const { return EffectiveLevels; }

	/** 아크 패시브 창의 [저장] — 규칙에 맞지 않거나 레이드 중이면 false */
	bool SetAllLevels(const TMap<FName, int32>& NewLevels, FText& OutFailReason);

	/** 점멸(소서리스 X — USorceressIdentityComponent) — 환류의 기운·마력 충전·응집되는 마력.
	 *  마력 해방에 걸린 노드(점화·점화의 불씨·발화·마나 순환·점멸 폭발·해방 봉인)는 아이덴티티 컴포넌트가 직접 읽는다 */
	void NotifyMovementSkill();

	int32 GetMasterStacks() const { return MasterStacks; }
	bool IsMagickChargeActive() const { return bMagickCharge; }
	int32 GetAccelStacks() const { return AccelStacks; }

	FOnArkPassiveChanged OnArkPassiveChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TMap<FName, int32> Levels;

	FArkPassiveBudget Budget;

	UPROPERTY(Transient)
	TMap<FName, int32> EffectiveLevels;

	int32 MasterStacks = 0;
	FTimerHandle MasterTimer;
	bool bMagickCharge = false;
	FTimerHandle MagickChargeTimer;
	int32 AccelStacks = 0;
	FTimerHandle AccelTimer;

	UStatComponent* GetStats() const;
	void RefreshBudget();
	void RefreshAll();
	int32 GetActiveLevel(FName Id) const;  // 트리가 꺼져 있으면 0
	void HandleSkillUsed(int32 SlotIndex);
	void ApplyStatic();
	void ApplyBuffs();
	void ClearBuffs();
	void SaveToGameInstance(bool bWriteDisk) const;
};
