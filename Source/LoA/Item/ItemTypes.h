#pragma once

#include "CoreMinimal.h"
#include "ItemTypes.generated.h"

class UTexture2D;

/** 정렬 순서 = 선언 순서 (배틀아이템이 맨 앞) */
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	BattleItem,
	Consumable,
	Material,
	Equipment,
	Etc,
};

/**
 * 아이템 한 종류의 정의 — 변하지 않는 데이터. 인벤토리 칸은 ItemId·개수만 들고 있고 정의는 LoAItems::Find()로 찾는다(플라이웨이트).
 * 같은 회복약 100개가 이름·아이콘·수치를 100번 복사해 들고 있지 않게 하려는 것
 */
USTRUCT(BlueprintType)
struct FItemDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EItemCategory Category = EItemCategory::Etc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	// 인벤토리 한 칸에 쌓이는 최대 개수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 999;

	// 같은 카테고리 안 정렬 순서 (작을수록 앞)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 SortOrder = 0;

	// 상점에 팔 때 개당 가격 (실링) — 0이면 판매 불가
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int64 SellPrice = 0;

	// ── 배틀아이템 (HUD 1~4 칸에 등록 가능) ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Battle")
	bool bIsBattleItem = false;

	// 배틀아이템 칸 하나에 장착할 수 있는 최대 개수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Battle", meta = (ClampMin = "1"))
	int32 BattleEquipMax = 1;

	// 사용 시 최대 HP의 이 비율만큼 즉시 회복 (0.6 = 60%)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Battle")
	float HealMaxHPRatio = 0.f;

	// 재사용 대기시간 (초) — 아이템 종류별로 걸린다(칸을 옮기거나 뺐다 껴도 유지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Battle")
	float Cooldown = 0.f;
};

/** 인벤토리 한 칸 — 저장 파일에도 이 그대로 들어간다 */
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Count = 0;

	// Ctrl+우클릭 잠금 — 정렬해도 그 자리에 남는다(드래그로 옮기는 건 가능). 잠금은 칸이 아니라 아이템에 붙어 같이 움직인다
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	bool bLocked = false;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
};

/** 아이템 정의 등록소 — 지금은 C++ 표(ItemRegistry.cpp). 아이템이 많아지면 DataTable로 옮길 것 */
namespace LoAItems
{
	LOA_API const FItemDefinition* Find(FName ItemId);

	// 정령의 회복약
	LOA_API extern const FName SpiritRecoveryPotion;

	/** 세 자리마다 쉼표 — 100000000 → "100,000,000". 언어 설정과 무관하게 항상 쉼표 */
	LOA_API FString FormatWithCommas(int64 Value);
}
