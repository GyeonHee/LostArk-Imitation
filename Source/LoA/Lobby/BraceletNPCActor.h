#pragma once

#include "CoreMinimal.h"
#include "Lobby/InteractableNPCActor.h"
#include "BraceletNPCActor.generated.h"

/**
 * [팔찌 전문가] NPC — 메뉴를 열면 컨트롤러가 UBraceletEnchantWidget(팔찌 효과 부여 창)을 띄운다. 규칙은 LoABracelet
 * 이름은 임시(공식 가이드에 없음), 미니맵 아이콘은 유저가 준 로아 미니맵 캡처에서 잘라냄(T_MinimapIcon_BraceletNPC)
 */
UCLASS()
class LOA_API ABraceletNPCActor : public AInteractableNPCActor
{
	GENERATED_BODY()

public:
	ABraceletNPCActor();
};

/**
 * [어빌리티 스톤 세공사] 어비르 (공식 가이드 각인 페이지) — 메뉴를 열면 UStoneFacetWidget(어빌리티 스톤 세공 창). 규칙은 LoAStone
 * 미니맵 아이콘은 유저가 준 로아 미니맵 캡처에서 잘라냄(T_MinimapIcon_StoneNPC)
 */
UCLASS()
class LOA_API AStoneNPCActor : public AInteractableNPCActor
{
	GENERATED_BODY()

public:
	AStoneNPCActor();
};
