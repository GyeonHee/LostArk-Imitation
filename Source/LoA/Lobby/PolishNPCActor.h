#pragma once

#include "CoreMinimal.h"
#include "Lobby/InteractableNPCActor.h"
#include "PolishNPCActor.generated.h"

/**
 * 장신구 연마 NPC — 마네킹·이름표·[G]·발광·카메라는 AInteractableNPCActor.
 * 메뉴를 열면 컨트롤러가 UAccessoryPolishWidget(착용 장신구 5칸 연마)을 띄운다. 규칙은 LoAPolish(Item/AccessoryPolish)
 */
UCLASS()
class LOA_API APolishNPCActor : public AInteractableNPCActor
{
	GENERATED_BODY()

public:
	APolishNPCActor();
};
