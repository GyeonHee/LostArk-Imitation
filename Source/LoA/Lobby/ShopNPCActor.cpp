#include "Lobby/ShopNPCActor.h"
#include "Item/ItemTypes.h"
#include "Item/BuildTypes.h"

AShopNPCActor::AShopNPCActor()
{
	NPCTitle = NSLOCTEXT("NPC", "ShopTitle", "[물약 상점]");
	NPCName = NSLOCTEXT("NPC", "ShopName", "에일린");
	GreetingLine = NSLOCTEXT("NPC", "ShopGreeting", "어서 오세요! 레이드 전에 회복약은 넉넉히 챙기셨나요?");
	MinimapIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Minimap/T_MinimapIcon_Shop.T_MinimapIcon_Shop")));

	FShopItemEntry Potion;
	Potion.ItemId = LoAItems::SpiritRecoveryPotion;
	Potion.Price = 1;
	ShopItems.Add(Potion);

	// 재련 숨결 — 실링으로 사는 선택 재료. 빙하 = 방어구, 용암 = 무기 (BP에서 ShopItems를 고쳤다면 거기에도 추가할 것)
	FShopItemEntry Glacier;
	Glacier.ItemId = LoAItems::GlacierBreath;
	Glacier.Price = 1000;
	ShopItems.Add(Glacier);

	FShopItemEntry Lava;
	Lava.ItemId = LoAItems::LavaBreath;
	Lava.Price = 1000;
	ShopItems.Add(Lava);

	// 1레벨 보석 상자 (7단계) — 골드. 1레벨 10개 = 1,000골드(개당 100). 7레벨 1개 = 1레벨 729개 = 상자 73개 ≒ 클리어 12회분 골드
	FShopItemEntry GemBoxEntry;
	GemBoxEntry.ItemId = LoAGems::GemBox;
	GemBoxEntry.Price = LoAGems::BoxGoldPrice;
	GemBoxEntry.bGoldPrice = true;
	ShopItems.Add(GemBoxEntry);
}
