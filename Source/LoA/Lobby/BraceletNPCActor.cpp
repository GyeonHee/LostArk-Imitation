#include "Lobby/BraceletNPCActor.h"

ABraceletNPCActor::ABraceletNPCActor()
{
	NPCTitle = NSLOCTEXT("NPC", "BraceletTitle", "[팔찌 전문가]");
	NPCName = NSLOCTEXT("NPC", "BraceletName", "리엔");
	GreetingLine = NSLOCTEXT("NPC", "BraceletGreeting", "팔찌에 깃들 힘은 매번 달라요. 마음에 드는 건 잠가 두세요.");
	MinimapIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Minimap/T_MinimapIcon_BraceletNPC.T_MinimapIcon_BraceletNPC")));
	HighlightColor = FLinearColor(1.f, 0.6f, 0.25f, 1.f);  // 주황
}

AStoneNPCActor::AStoneNPCActor()
{
	NPCTitle = NSLOCTEXT("NPC", "StoneTitle", "[어빌리티 스톤 세공사]");
	NPCName = NSLOCTEXT("NPC", "StoneName", "어비르");
	GreetingLine = NSLOCTEXT("NPC", "StoneGreeting", "돌은 정직하지. 두드린 만큼 답을 하거든. 운이 따른다면 말이야.");
	MinimapIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Minimap/T_MinimapIcon_StoneNPC.T_MinimapIcon_StoneNPC")));
	HighlightColor = FLinearColor(0.35f, 0.85f, 1.f, 1.f);  // 하늘색
}
