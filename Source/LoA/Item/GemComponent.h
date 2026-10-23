#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/BuildTypes.h"
#include "GemComponent.generated.h"

class UInventoryComponent;
class UStatComponent;

DECLARE_MULTICAST_DELEGATE(FOnGemsChanged);

/**
 * 보석 11칸 (7단계, 로아 T4). 칸 = 보석 아이템 + 그 보석을 건 스킬 행 이름.
 * 스킬 하나에 겁화 1개 + 작열 1개까지 — 같은 종류를 또 끼우면 그 칸의 보석과 바꾼다.
 * 효과: 겁화·작열은 스킬별로 UStatComponent::SetSkillGemBonuses, 기본 공격력 %는 "Gem" 수정치.
 * 장착·해제·합성은 보스 맵 밖(대기 지역)에서만. 칸 배치는 GameInstance·SaveGame으로 유지.
 * 첫 실행(또는 7단계 이전 저장)엔 시작 보석 Lv5 11개를 바로 장착 — 겁화 6(딜 비중 높은 스킬 순) + 작열 5.
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API UGemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGemComponent();

	const TArray<FGemSlot>& GetSlots() const { return Slots; }

	/** 소지품의 보석(GemItemId) 1개를 SkillRowName에 장착 — 같은 스킬의 같은 종류 보석이 있으면 바꿔 끼우고 원래 것은 소지품으로 */
	bool EquipGem(FName GemItemId, FName SkillRowName, FText& OutFailReason);

	/** 칸의 보석을 빼서 소지품으로 */
	bool UnequipGem(int32 SlotIndex, FText& OutFailReason);

	// ── 보석 합성 (로아 T4) — 같은 레벨 3개 → 다음 레벨 1개, 100%. 겁화·작열을 섞어도 되고 결과 종류는 재료 3개 중 랜덤 ──
	// 재료 칸에 올린 보석은 소지품에서 빠져 여기 있다(FuseInputs). 내리거나 창을 닫으면 돌아가고,
	// 저장 파일에도 들어가 있어(GameInstance::GemFuseInputs) 꺼졌다 켜지면 BeginPlay에서 소지품으로 돌려준다

	const TArray<FName>& GetFuseInputs() const { return FuseInputs; }

	/** 소지품의 보석 1개를 재료 칸에 — 같은 레벨끼리, 최대 3개 */
	bool AddFuseInput(FName GemItemId, FText& OutFailReason);

	/** 재료 칸 하나를 소지품으로 */
	bool RemoveFuseInput(int32 Index, FText& OutFailReason);

	/** 재료 전부 소지품으로 (창 닫을 때) */
	void ReturnAllFuseInputs();

	/** 목표 레벨 버튼 — 재료를 비우고, 목표보다 낮은 레벨 중 3개 이상 있는 가장 낮은 레벨에서 랜덤 3개를 올린다 */
	bool FillFuseInputsForTarget(int32 TargetLevel, FText& OutFailReason);

	/** [합성하기] — 재료 3개로 1번 합성. 성공하면 재료 칸은 비고 결과는 소지품에 */
	bool FuseInputs3(FName& OutResult, FText& OutFailReason);

	FOnGemsChanged OnFuseInputsChanged;

	/** 보석·각인을 바꿀 수 있는 곳인가 — 보스 맵에선 불가 */
	static bool IsBuildChangeAllowed(const UWorld* World);

	/** 이 스킬에 꽂힌 보석 (없으면 0) */
	int32 GetGemLevelOnSkill(FName SkillRowName, EGemType Type) const;

	FOnGemsChanged OnGemsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FGemSlot> Slots;

	UPROPERTY(Transient)
	TArray<FName> FuseInputs;

	UInventoryComponent* GetInventory() const;
	UStatComponent* GetStats() const;

	void GrantStarterGems();
	void ApplyStats();
	void SaveToGameInstance(bool bWriteDisk) const;
};
