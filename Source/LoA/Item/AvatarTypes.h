#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "AvatarTypes.generated.h"

class USkinnedMeshComponent;

/** 아바타 칸 — 로아 장비창 순서. 저장 파일의 착용 배열이 이 순서라 **뒤에만 추가할 것** */
UENUM(BlueprintType)
enum class EAvatarSlot : uint8
{
	Head     UMETA(DisplayName="머리"),
	Shoulder UMETA(DisplayName="어깨"),
	Chest    UMETA(DisplayName="상의"),
	Gloves   UMETA(DisplayName="장갑"),
	Pants    UMETA(DisplayName="하의"),
	Weapon   UMETA(DisplayName="무기"),
	MAX      UMETA(Hidden),
};

/** 아바타 하나 = 원본 의상 파츠 메시 하나(소서리스 스켈레톤 공용, 무기만 자체 스켈레톤). 소지품 칸을 쓰지 않는다 — 전부 보유 */
struct FLoAAvatarDef
{
	FName Id;
	FText Name;
	EAvatarSlot Slot = EAvatarSlot::MAX;
	FSoftObjectPath MeshPath;
	// 상하의 일체(드레스) — 입으면 하의 칸 메시를 숨긴다
	bool bHidesPants = false;
};

/** 아바타 표 — AvatarRegistry.cpp */
namespace LoAAvatars
{
	constexpr int32 SlotCount = static_cast<int32>(EAvatarSlot::MAX);

	const TArray<FLoAAvatarDef>& All();
	const FLoAAvatarDef* Find(FName Id);
	TArray<const FLoAAvatarDef*> ForSlot(EAvatarSlot Slot);

	/** 칸이 비었을 때 모습(기본 옷·기본 지팡이) — 없으면 빈 경로 */
	FSoftObjectPath GetDefaultMesh(EAvatarSlot Slot);

	FText GetSlotName(EAvatarSlot Slot);

	/** 몸(통짜 SK_Sorceress, 애니메이션 리더)은 얼굴·눈·속눈썹·머리카락 섹션만 보이고 옷·맨살은 숨긴다 —
	 *  의상 파츠에는 얼굴이 없다(머리 파츠는 모자·장식뿐). 캐릭터·미리보기 공용 */
	void ShowOnlyBaseBodySections(USkinnedMeshComponent* Body);
}
