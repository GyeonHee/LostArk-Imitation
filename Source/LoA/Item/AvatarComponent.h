#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Item/AvatarTypes.h"
#include "AvatarComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * 아바타(의상) — 칸마다 착용 아바타 Id(EAvatarSlot 순서). 전부 보유라 목록은 LoAAvatars 표 그대로, 저장은 착용 Id만.
 * 모습: 캐릭터 몸 메시(원본 통짜 모델)는 애니메이션 리더 — 얼굴·눈·머리카락 섹션만 보인다(파츠에는 얼굴이 없음).
 * 칸마다 파츠 메시가 SetLeaderPoseComponent로 포즈를 따른다.
 * 무기는 캐릭터의 WeaponMeshComp(손 소켓)에 메시만 바꿔 넣는다. 빈 칸은 기본 옷·기본 지팡이(LoAAvatars::GetDefaultMesh)
 */
UCLASS(ClassGroup=(LoA), meta=(BlueprintSpawnableComponent))
class LOA_API UAvatarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	FName GetEquipped(EAvatarSlot Slot) const;

	/** 착용 — 칸은 아바타 정의에서. 같은 걸 다시 고르면 무시 */
	void Equip(FName AvatarId);
	void Unequip(EAvatarSlot Slot);

	/** 칸에 실제로 보일 메시 — 착용 중이면 그 메시, 비었으면 기본, 드레스에 가려진 하의·없는 칸은 nullptr. 미리보기 액터도 이걸 쓴다 */
	USkeletalMesh* ResolveMesh(EAvatarSlot Slot) const;

	FSimpleMulticastDelegate OnAvatarChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyToOwner();
	void SaveToGameInstance(bool bWriteDisk) const;

	TArray<FName> Equipped;

	// 무기를 뺀 칸마다 하나 (EAvatarSlot 순서, 무기 칸은 비어 있음)
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> PartComps;
};
