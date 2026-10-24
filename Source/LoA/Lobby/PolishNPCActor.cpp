#include "Lobby/PolishNPCActor.h"

APolishNPCActor::APolishNPCActor()
{
	NPCTitle = NSLOCTEXT("NPC", "PolishTitle", "[장신구 연마]");
	NPCName = NSLOCTEXT("NPC", "PolishName", "페메르");
	GreetingLine = NSLOCTEXT("NPC", "PolishGreeting", "보석은 다듬을수록 빛나는 법이죠. 어떤 힘이 깃들지는 저도 알 수 없답니다.");
	MinimapIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Minimap/T_MinimapIcon_PolishNPC.T_MinimapIcon_PolishNPC")));

	// 상점(금색)·재련(파랑)과 구분되게 보랏빛 테두리
	HighlightColor = FLinearColor(0.75f, 0.45f, 1.f, 1.f);
}
