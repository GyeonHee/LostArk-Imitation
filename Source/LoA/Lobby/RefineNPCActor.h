#pragma once

#include "CoreMinimal.h"
#include "Lobby/InteractableNPCActor.h"
#include "RefineNPCActor.generated.h"

/**
 * 재련 NPC — 마네킹·이름표·[G]·발광·카메라는 AInteractableNPCActor.
 * 메뉴를 열면 컨트롤러가 URefineWidget(착용 장비 6부위 재련)을 띄운다. 재련 규칙은 LoARefine(Item/RefineLibrary)
 */
UCLASS()
class LOA_API ARefineNPCActor : public AInteractableNPCActor
{
	GENERATED_BODY()

public:
	ARefineNPCActor();
};
