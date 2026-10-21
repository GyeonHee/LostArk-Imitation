#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/ItemTypes.h"
#include "EquipmentComponent.generated.h"

class UInventoryComponent;

DECLARE_MULTICAST_DELEGATE(FOnEquipmentChanged);

/**
 * 장착 장비 6부위 (캐릭터에 부착) — 부위마다 장비 인스턴스 GUID 하나. 인스턴스 데이터는 UInventoryComponent의 맵에 있다.
 *  - 장착/해제 = 소지품 칸과 장착 칸의 GUID를 맞바꾼다(인스턴스는 그대로)
 *  - 바뀔 때마다 UStatComponent에 "Equipment" 수정치로 지능·무기공격력·체력을 다시 등록한다
 *  - 장착 아이템 레벨 = 6부위 아이템 레벨 평균(빈 부위는 0) — 로아와 같음
 *  - 보스 맵에서는 장착 변경 불가 (로비에서는 어디서나)
 *  - 첫 실행(또는 장비 이전 저장 파일)에는 시작 장비 1640 세트를 장착한 채로 시작 — 장비 도입 전 기준 스펙과 같다
 */
UCLASS(ClassGroup=(LoA), meta=(BlueprintSpawnableComponent))
class LOA_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent();

	/** 소지품 Index 칸의 장비를 장착 — 같은 부위에 있던 장비는 그 칸으로 돌아간다 */
	bool EquipFromInventory(int32 InventoryIndex, FText& OutFailReason);

	/** 부위 해제 — 소지품 첫 빈 칸으로. 가득 차면 실패 */
	bool Unequip(EEquipSlot Slot, FText& OutFailReason);

	const FEquipmentInstance* GetEquipped(EEquipSlot Slot) const;

	/** 장착 아이템 레벨 (6부위 평균, 소수 둘째 자리까지 의미 있음) */
	UFUNCTION(BlueprintPure, Category="Equipment")
	float GetItemLevel() const;

	/** 보스 맵이면 false — 장착·해제 막음 */
	bool IsEquipChangeAllowed() const;

	/** 장착 장비의 인스턴스가 바뀜(재련 등) — 스탯·창을 다시 계산 */
	void RefreshEquipped();

	/** 테스트용 — 장착 장비의 재련 단계를 바로 바꾼다 (LoASetRefine) */
	bool DebugSetRefine(EEquipSlot Slot, int32 RefineLevel);

	FOnEquipmentChanged OnEquipmentChanged;

	/** 장비 한 개가 주는 스탯 (재련 반영) — 툴팁·비교용 */
	static void GetEquipmentStats(const FEquipmentInstance& Instance, double& OutPrimary, double& OutWeapon, double& OutVitality);

	/** 툴팁 글 — 이름 +재련, 부위, 아이템 레벨, 스탯. Compare(같은 부위에 착용 중인 장비)를 주면 차이를 ▲▼로 붙인다 */
	static FText DescribeEquipment(const FEquipmentInstance& Instance, const FEquipmentInstance* Compare = nullptr);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyStats();
	UInventoryComponent* GetInventory() const;

	// EEquipSlot 순서, 빈 부위는 무효 GUID
	TArray<FGuid> Equipped;
};
