#pragma once

#include "CoreMinimal.h"
#include "Lobby/InteractableNPCActor.h"
#include "ShopNPCActor.generated.h"

/** 상점 판매 목록 한 줄 */
USTRUCT(BlueprintType)
struct FShopItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FName ItemId;

	// 개당 가격 (bGoldPrice면 골드, 아니면 실링)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	int64 Price = 1;

	// 골드로 사는 물건 (보석 상자 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	bool bGoldPrice = false;
};

/**
 * 상점 NPC — 마네킹·이름표·[G]·발광·카메라는 AInteractableNPCActor, 여기는 판매 목록만.
 * 메뉴를 열면 컨트롤러가 UShopWidget(구매/판매) + 소지품 창을 띄운다
 */
UCLASS()
class LOA_API AShopNPCActor : public AInteractableNPCActor
{
	GENERATED_BODY()

public:
	AShopNPCActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Shop")
	TArray<FShopItemEntry> ShopItems;

	// 구매 목록 칸 하나에 담을 수 있는 최대 개수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Shop", meta = (ClampMin = "1"))
	int32 CartSlotMaxCount = 99;
};
