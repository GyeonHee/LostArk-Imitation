#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/BuildTypes.h"
#include "EngravingComponent.generated.h"

class UInventoryComponent;
class UStatComponent;

DECLARE_MULTICAST_DELEGATE(FOnEngravingsChanged);

/**
 * 각인 5칸 (7단계, 로아 T4). 각인마다 읽은 유물 각인서 권수로 단계가 정해진다 — 1권 = 배움(유물 0단계), 이후 5권마다 +1 (최대 4).
 * 효과는 UStatComponent 수정치 두 출처로: "Engraving"(장착만 하면 붙는 것) / "EngravingBuff"(아드레날린·마나의 흐름 중첩).
 * 중첩은 USkillManagerComponent::OnSkillUsed(스킬 슬롯 0~7의 쿨타임이 시작될 때)마다 +1, 지속시간이 끝나면 전부 사라진다(로아와 같음).
 * 장착·해제·각인서 읽기는 보스 맵 밖에서만. 첫 실행(또는 7단계 이전 저장)엔 원한·아드레날린·예리한 둔기·저주받은 인형·타격의 대가를 유물 0단계로 장착.
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API UEngravingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEngravingComponent();

	const TArray<FName>& GetEquipped() const { return Equipped; }

	/** 읽은 권수 → 단계 (-1 = 안 배움) */
	int32 GetLevel(FName EngravingId) const;

	/** 착용 중인 어빌리티 스톤이 그 각인에 주는 레벨 0~4 — 장착한 각인일 때만 (로아 T4) */
	int32 GetStoneLevel(FName EngravingId) const;
	int32 GetBooksRead(FName EngravingId) const;

	/** 배운 각인을 칸에 장착 — 이미 다른 칸에 있으면 그 칸과 맞바꾼다 */
	bool EquipEngraving(int32 SlotIndex, FName EngravingId, FText& OutFailReason);
	bool UnequipEngraving(int32 SlotIndex, FText& OutFailReason);

	/** 5칸을 통째로 바꾼다 — 각인 효과 창의 [저장]. 안 배운 각인·중복은 빈 칸으로 */
	bool SetAllEquipped(const TArray<FName>& NewEquipped, FText& OutFailReason);

	/** 소지품의 유물 각인서 1권을 읽는다 (최대 단계면 거부) */
	bool ReadBook(FName EngravingId, FText& OutFailReason);

	int32 GetAdrenalineStacks() const { return AdrenalineStacks; }
	int32 GetMagickStreamStacks() const { return MagickStreamStacks; }
	int32 GetEtherStacks() const { return EtherStacks; }

	/** 카운터 성공 — 분쇄의 주먹 (보스 카운터·거울 카운터가 부른다) */
	void NotifyCounterSuccess();

	/** 이동기(대시) 사용 — 추진력 */
	void NotifyMovementSkill();

	/** 장착 중인지 */
	bool IsEquipped(FName EngravingId) const { return Equipped.Contains(EngravingId); }

	FOnEngravingsChanged OnEngravingsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FName> Equipped;

	UPROPERTY(Transient)
	TMap<FName, int32> BooksRead;

	int32 AdrenalineStacks = 0;
	int32 MagickStreamStacks = 0;
	FTimerHandle AdrenalineTimer;
	FTimerHandle MagickStreamTimer;

	int32 EtherStacks = 0;
	double LastEtherGainTime = -1000.0;
	FTimerHandle EtherTimer;
	bool bCrushingFistActive = false;
	FTimerHandle CrushingFistTimer;
	bool bPropulsionActive = false;
	FTimerHandle PropulsionTimer;

	UInventoryComponent* GetInventory() const;
	UStatComponent* GetStats() const;

	void HandleSkillUsed(int32 SlotIndex);
	void ExpireAdrenaline();
	void ExpireMagickStream();

	void ApplyStatic();
	void ApplyBuffs();
	void HandleEquipmentChanged() { ApplyStatic(); ApplyBuffs(); }
	void SaveToGameInstance(bool bWriteDisk) const;
};
