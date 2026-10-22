#pragma once

#include "CoreMinimal.h"
#include "Stat/StatTypes.h"
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

/** 장비 부위 — 앞 6칸(방어구 5 + 무기)이 재련 장비, 장착 아이템 레벨은 이 6칸 평균 (로아와 같음).
 *  뒤 5칸은 장신구(6단계) — 재련 없음, 전투 특성만 준다. 저장 파일의 장착 GUID 배열이 이 순서라 **뒤에만 추가할 것** */
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
	Helmet   UMETA(DisplayName="투구"),
	Shoulder UMETA(DisplayName="어깨"),
	Chest    UMETA(DisplayName="상의"),
	Pants    UMETA(DisplayName="하의"),
	Gloves   UMETA(DisplayName="장갑"),
	Weapon   UMETA(DisplayName="무기"),
	Necklace UMETA(DisplayName="목걸이"),
	Earring1 UMETA(DisplayName="귀걸이"),
	Earring2 UMETA(DisplayName="귀걸이"),
	Ring1    UMETA(DisplayName="반지"),
	Ring2    UMETA(DisplayName="반지"),
	MAX      UMETA(Hidden),
};

/** 장신구에 붙은 전투 특성 하나 (예: 치명 +385) */
USTRUCT(BlueprintType)
struct FStatRoll
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	ELoACombatStat Stat = ELoACombatStat::Crit;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Value = 0;
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

	// 스탯(무기 공격력·방어력·주 스탯·체력)은 정의에 없다 — 부위와 재련 단계로 LoAItems::GetGearStats(로아 T4 업화 표)에서 읽는다

	// ── 장신구 (6단계) — 인스턴스를 만들 때 특성 AccessoryStatCount종을 [Min, Max]로 굴린다(서로 다른 종류). 0이면 장신구 아님 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	int32 AccessoryStatCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	int32 AccessoryStatMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	int32 AccessoryStatMax = 0;

	bool IsAccessory() const { return IsEquipment() && AccessoryStatCount > 0; }

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

	// 장신구 전투 특성 (6단계) — 재련 장비는 비어 있다
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FStatRoll> StatRolls;

	// 장비 성장 — 다음 단계 재련 전에 운명의 파편 + 실링으로 경험치를 100% 채워야 한다(로아). 재련에 성공하면 다시 false
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool bGrowthDone = false;
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
	constexpr int32 SlotCount = static_cast<int32>(EEquipSlot::MAX);  // 장신구까지 전부 (장착 GUID 배열 크기)
	constexpr int32 GearSlotCount = 6;                                 // 재련 장비 (방어구 5 + 무기) — 아이템 레벨 평균·재련 대상
	inline bool IsGearSlot(EEquipSlot Slot) { return static_cast<int32>(Slot) < GearSlotCount; }
	/** Def의 부위(귀걸이·반지는 1번 칸으로 정의)가 장착 칸 Slot에 들어갈 수 있는지 — 귀걸이·반지는 두 칸 다 */
	LOA_API bool CanEquipInSlot(EEquipSlot DefSlot, EEquipSlot Slot);
	constexpr int32 ItemLevelPerRefine = 5;   // 재련 1단계 = 부위 아이템 레벨 +5
	constexpr int32 MaxRefineLevel = 25;
	constexpr int32 StarterRefineLevel = 10;  // 시작 세트 +10 = 아이템 레벨 1640
}

/** 재련 장비 한 부위의 기본 효과 — 무기는 무기 공격력만, 방어구는 나머지 넷 */
struct FGearStats
{
	double PrimaryStat = 0.0;   // 직업 주 스탯(힘·민첩·지능 중 그 직업 것)에 더해진다
	double WeaponPower = 0.0;
	double Vitality = 0.0;
	double PhysDefense = 0.0;   // 직업 방어 계수 곱하기 전
	double MagicDefense = 0.0;
};

/** 아이템 정의 등록소 — 지금은 C++ 표(ItemRegistry.cpp). 아이템이 많아지면 DataTable로 옮길 것 */
namespace LoAItems
{
	LOA_API const FItemDefinition* Find(FName ItemId);

	// 정령의 회복약
	LOA_API extern const FName SpiritRecoveryPotion;

	// 재련 재료 — 수호석(방어구) / 파괴석(무기) / 돌파석(공통) / 숨결(선택, 확률 +기본 확률: 빙하 = 방어구, 용암 = 무기)
	LOA_API extern const FName GuardianStone;
	LOA_API extern const FName DestructionStone;
	LOA_API extern const FName LeapStone;
	LOA_API extern const FName GlacierBreath;  // 빙하의 숨결 — ID는 숨결이 하나이던 시절의 "RefineBreath" 그대로(저장 파일 호환)
	LOA_API extern const FName LavaBreath;     // 용암의 숨결
	LOA_API extern const FName FateFragment;   // 운명의 파편 — 장비 성장(재련 전 단계) 재료

	/** 부위별 시작 장비 (1640 세트 = 재련 +10) — EEquipSlot 순서 */
	LOA_API FName GetStarterEquipment(EEquipSlot Slot);

	/** 장신구 정의로 전투 특성을 굴린다 — 치명·특화·신속·제압 중 서로 다른 AccessoryStatCount종 (인내·숙련은 효과가 없어 빼둠) */
	LOA_API TArray<FStatRoll> RollAccessoryStats(const FItemDefinition& Def);

	/** 칸 왼쪽 위 표시 — 재련 장비는 "+10", 장신구는 특성 첫 글자("치특") */
	LOA_API FString GetEquipmentBadge(const FEquipmentInstance& Instance);

	/** 시작 장신구 (EEquipSlot::Necklace ~ Ring2 순서) — 정의 ID와 고정 특성 */
	LOA_API FName GetStarterAccessory(EEquipSlot Slot);
	LOA_API TArray<FStatRoll> GetStarterAccessoryStats(EEquipSlot Slot);

	/** 레이드 드랍 장신구 — 목걸이·귀걸이·반지 중 하나 (특성은 인스턴스를 만들 때 굴림) */
	LOA_API FName GetRandomDropAccessory();

	/** 장비 인스턴스의 부위 아이템 레벨 */
	LOA_API int32 GetEquipmentItemLevel(const FEquipmentInstance& Instance);

	/** 재련 장비 부위의 RefineLevel(0~25) 단계 기본 효과 — 로아 T4 운명의 업화 표 (부위마다, 단계마다 오르는 양이 다르다) */
	LOA_API FGearStats GetGearStats(EEquipSlot Slot, int32 RefineLevel);

	/** 세 자리마다 쉼표 — 100000000 → "100,000,000". 언어 설정과 무관하게 항상 쉼표 */
	LOA_API FString FormatWithCommas(int64 Value);
}
