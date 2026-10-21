#pragma once

#include "CoreMinimal.h"
#include "ItemTypes.generated.h"

class UTexture2D;
class UNiagaraSystem;

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

/** 장비 부위 — 장착 아이템 레벨은 이 6부위 평균 (로아: 무기 1 + 방어구 5) */
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
	Helmet   UMETA(DisplayName="투구"),
	Shoulder UMETA(DisplayName="어깨"),
	Chest    UMETA(DisplayName="상의"),
	Pants    UMETA(DisplayName="하의"),
	Gloves   UMETA(DisplayName="장갑"),
	Weapon   UMETA(DisplayName="무기"),
	MAX      UMETA(Hidden),
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

	// 사용 시 캐릭터 몸(캡슐 중심)에 붙어 나오는 이펙트 — 비우면 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Battle")
	TSoftObjectPtr<UNiagaraSystem> UseVFX;

	// 이펙트가 루핑이어도 이 시간(초) 뒤엔 생성을 멈추고 남은 입자가 사라지면 정리된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Battle")
	float UseVFXDuration = 1.5f;

	// ── 장비 (Category == Equipment) — 칸 하나에 1개, 재련 단계 등은 FEquipmentInstance가 들고 있다 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
	EEquipSlot EquipSlot = EEquipSlot::MAX;

	// 재련 0단계 아이템 레벨 — 부위 레벨 = 이 값 + 재련 단계 × LoAEquipRules::ItemLevelPerRefine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
	int32 BaseItemLevel = 0;

	// 재련 0단계 스탯 — 단계마다 LoAEquipRules::StatPerRefine씩 오른다. 방어구는 주 스탯·체력, 무기는 무기 공격력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
	double PrimaryStat = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
	double WeaponPower = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Equipment")
	double Vitality = 0.0;

	bool IsEquipment() const { return Category == EItemCategory::Equipment && EquipSlot != EEquipSlot::MAX; }
};

/**
 * 장비 한 개 — 같은 정의(투구)라도 재련 단계·품질이 제각각이라 인스턴스로 따로 들고 있다.
 * 소지품 칸·장착 칸은 InstanceId(GUID)로 이걸 가리키고, 실제 데이터는 UInventoryComponent의 맵 하나에만 있다
 */
USTRUCT(BlueprintType)
struct FEquipmentInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 RefineLevel = 0;

	// 품질 0~100 — 아직 효과 없음(로아: 무기 추가 피해 / 방어구 체력)
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Quality = 0;

	// 4단계 재련용 — 장인의 기운(0~1)과 실패 보정 누적 횟수
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	float ArtisanEnergy = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 FailBonusSteps = 0;
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

	// 장비 칸이면 그 장비 인스턴스 (Count는 1). 일반 아이템은 무효 GUID
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid EquipmentId;

	bool IsEmpty() const { return ItemId.IsNone() || Count <= 0; }
	bool IsEquipment() const { return EquipmentId.IsValid(); }
};

/** 장비 규칙 — 직업과 무관한 게임 규칙이라 코드에 둔다 (StatTypes.h의 LoAStatRules와 같은 방침) */
namespace LoAEquipRules
{
	constexpr int32 SlotCount = static_cast<int32>(EEquipSlot::MAX);
	constexpr int32 ItemLevelPerRefine = 5;   // 재련 1단계 = 부위 아이템 레벨 +5
	constexpr double StatPerRefine = 0.02;    // 재련 1단계 = 부위 스탯 +2% (재련 0단계 기준)
	constexpr int32 MaxRefineLevel = 25;
	constexpr int32 StarterRefineLevel = 10;  // 시작 세트 +10 = 아이템 레벨 1640
}

/** 아이템 정의 등록소 — 지금은 C++ 표(ItemRegistry.cpp). 아이템이 많아지면 DataTable로 옮길 것 */
namespace LoAItems
{
	LOA_API const FItemDefinition* Find(FName ItemId);

	// 정령의 회복약
	LOA_API extern const FName SpiritRecoveryPotion;

	/** 부위별 시작 장비 (1640 세트 = 재련 +10) — EEquipSlot 순서 */
	LOA_API FName GetStarterEquipment(EEquipSlot Slot);

	/** 장비 인스턴스의 부위 아이템 레벨 / 재련 반영 스탯 배율 */
	LOA_API int32 GetEquipmentItemLevel(const FEquipmentInstance& Instance);
	LOA_API double GetRefineStatMultiplier(int32 RefineLevel);

	/** 세 자리마다 쉼표 — 100000000 → "100,000,000". 언어 설정과 무관하게 항상 쉼표 */
	LOA_API FString FormatWithCommas(int64 Value);
}
