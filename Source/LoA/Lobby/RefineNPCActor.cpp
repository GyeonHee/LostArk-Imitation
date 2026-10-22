#include "Lobby/RefineNPCActor.h"

ARefineNPCActor::ARefineNPCActor()
{
	NPCTitle = NSLOCTEXT("NPC", "RefineTitle", "[장비 재련]");
	NPCName = NSLOCTEXT("NPC", "RefineName", "칼도르");
	GreetingLine = NSLOCTEXT("NPC", "RefineGreeting", "실패해도 장비는 멀쩡하네. 다만 재료는 돌려주지 않지.");

	// 상점(금색)과 구분되게 푸른 테두리
	HighlightColor = FLinearColor(0.35f, 0.7f, 1.f, 1.f);
}
