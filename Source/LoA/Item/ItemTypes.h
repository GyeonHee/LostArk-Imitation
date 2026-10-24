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
	Gem,      // 7단계 보석 (겁화·작열)
	Book,     // 7단계 유물 각인서
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
	Bracelet UMETA(DisplayName="팔찌"),            // 2026-10-25
	AbilityStone UMETA(DisplayName="어빌리티 스톤"),
	MAX      UMETA(Hidden),
};

/** 장신구 연마 효과 종류 (로아 T4) — 앞 12개가 부위 전용(목걸이 4 · 귀걸이 4 · 반지 4), 뒤 6개가 공통. 저장 파일에 들어가니 **뒤에만 추가할 것** */
UENUM(BlueprintType)
enum class EPolishEffect : uint8
{
	AdditionalDamage   UMETA(DisplayName="추가 피해"),
	DamageToEnemies    UMETA(DisplayName="적에게 주는 피해"),
	SupportGauge       UMETA(DisplayName="세레나데·신앙·조화 게이지 획득량"),
	BrandPower         UMETA(DisplayName="낙인력"),
	AttackPowerPercent UMETA(DisplayName="공격력 %"),
	WeaponPowerPercent UMETA(DisplayName="무기 공격력 %"),
	PartyHeal          UMETA(DisplayName="파티원 회복 효과"),
	PartyShield        UMETA(DisplayName="파티원 보호막 효과"),
	CritRate           UMETA(DisplayName="치명타 적중률"),
	CritDamage         UMETA(DisplayName="치명타 피해"),
	AllyAttackBuff     UMETA(DisplayName="아군 공격력 강화 효과"),
	AllyDamageBuff     UMETA(DisplayName="아군 피해량 강화 효과"),
	MaxHP              UMETA(DisplayName="최대 생명력"),
	AttackPowerFlat    UMETA(DisplayName="공격력"),
	WeaponPowerFlat    UMETA(DisplayName="무기 공격력"),
	MaxMP              UMETA(DisplayName="최대 마나"),
	StatusDuration     UMETA(DisplayName="상태이상 공격 지속시간"),
	CombatHPRegen      UMETA(DisplayName="전투 중 생명력 회복량"),
};

/** 팔찌 효과 종류 (로아 T4, Maxroll itemRandom 2133000x3) — 기본 효과 · 전투 특성 · 특수 효과. 저장 파일에 들어가니 **뒤에만 추가할 것** */
UENUM(BlueprintType)
enum class EBraceletEffect : uint8
{
	// 기본 효과 (값 = 수치)
	PrimaryStat        UMETA(DisplayName="주 스탯"),
	Vitality           UMETA(DisplayName="체력"),
	// 전투 특성 (값 = 포인트)
	Crit               UMETA(DisplayName="치명"),
	Specialization     UMETA(DisplayName="특화"),
	Domination         UMETA(DisplayName="제압"),
	Swiftness          UMETA(DisplayName="신속"),
	Endurance          UMETA(DisplayName="인내"),
	Expertise          UMETA(DisplayName="숙련"),
	// 특수 효과 (값 = 등급 0 하 / 1 중 / 2 상)
	DamageToEnemies    UMETA(DisplayName="적에게 주는 피해"),
	AdditionalDamage   UMETA(DisplayName="추가 피해"),
	CritRate           UMETA(DisplayName="치명타 적중률"),
	CritDamage         UMETA(DisplayName="치명타 피해"),
	WeaponPower        UMETA(DisplayName="무기 공격력"),
	OnHitWeaponPower   UMETA(DisplayName="적중 시 무기 공격력"),
	StaggeredDamage    UMETA(DisplayName="적에게 주는 피해(무력화)"),
	NonDirectional     UMETA(DisplayName="비방향성 공격 피해"),
	BackAttack         UMETA(DisplayName="백어택 피해"),
	HeadAttack         UMETA(DisplayName="헤드어택 피해"),
	PhysDefense        UMETA(DisplayName="물리 방어력"),
	MagicDefense       UMETA(DisplayName="마법 방어력"),
	MaxHP              UMETA(DisplayName="최대 생명력"),
	CombatHPRegen      UMETA(DisplayName="전투 중 생명력 회복량"),
	AllyAttackBuff     UMETA(DisplayName="아군 공격력 강화 효과"),
	AllyDamageBuff     UMETA(DisplayName="아군 피해량 강화 효과"),
	ChallengeDamage    UMETA(DisplayName="도전 이하 몬스터 피해"),
	ChallengeDefense   UMETA(DisplayName="도전 이하 몬스터 받는 피해 감소"),
	CCImmunity         UMETA(DisplayName="마비·밀려남 면역"),
};

/** 팔찌 효과 한 줄 — 기본 효과·전투 특성은 Value(수치), 특수 효과는 Grade(0~2) */
USTRUCT(BlueprintType)
struct FBraceletEffectRoll
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	EBraceletEffect Effect = EBraceletEffect::Crit;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Grade = 0;

	// 고정 효과(획득 시 무작위, 부여·변환으로 안 바뀜) / false = 부여 효과(팔찌 효과 부여 NPC)
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool bFixed = false;
};

/** 어빌리티 스톤 한 줄 — 각인(증가 2줄) 또는 감소 효과(3번째 줄), 세공 10칸의 성공 수·시도 수 */
USTRUCT(BlueprintType)
struct FStoneLine
{
	GENERATED_BODY()

	// 증가 줄 = 각인 Id(LoAEngravings), 감소 줄 = LoAStone 감소 효과 Id(AtkReduction 등)
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Successes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Attempts = 0;

	// 세공 칸 결과 순서 (true = 성공) — 창의 ◆◇ 표시용
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<bool> Results;
};

/** 장신구에 붙은 연마 효과 하나 — 종류 + 등급(0 하 / 1 중 / 2 상). 수치는 LoAPolish 표에서 읽는다 */
USTRUCT(BlueprintType)
struct FPolishEffectRoll
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	EPolishEffect Effect = EPolishEffect::MaxHP;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 Grade = 0;
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

	// 툴팁에 이름 아래로 붙는 설명 — 비우면 이름만
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

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

	// [미사용, 2026-10-25] 장신구 주 스탯·체력 등급 0~3을 굴려 넣던 자리 — 게임 데이터를 다시 보니 이 4단계는 등급이 아니라
	// 연마 단계별 기본 효과였다. 지금은 PolishLevel로 읽는다(LoAItems::GetAccessoryStats). 저장 파일 호환을 위해 남겨둠
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 AccessoryTier = 0;

	// [미사용] 장신구 깨달음 포인트를 굴려 넣던 자리 — 지금은 등급·부위·연마 단계 표(LoAArkPassive::GetAccessoryEnlightenment)로 정한다.
	// 저장 파일 호환을 위해 남겨둠
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 EnlightenmentPoints = 0;

	// 장신구 연마 단계 0~3 — 기본 효과(주 스탯·체력)와 깨달음 포인트가 표대로 오른다(유물 목걸이 4→5→7→10). 연마 NPC(LoAPolish)
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 PolishLevel = 0;

	// 연마 효과 — 단계마다 하나씩 (PolishLevel개)
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FPolishEffectRoll> PolishEffects;

	// 팔찌 효과 (LoABracelet) — 고정 효과(bFixed) 먼저, 그 뒤 부여 효과. 팔찌가 아니면 비어 있다
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FBraceletEffectRoll> BraceletEffects;

	// 팔찌 부여 효과 칸 수 / 남은 부여·변환 횟수
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 BraceletEnchantSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 BraceletEnchantsLeft = 0;

	// 어빌리티 스톤 (LoAStone) — 0·1 = 증가 각인, 2 = 감소 효과. StoneChance = 다음 세공 성공 확률 %(75 → 성공 -10 / 실패 +10, 25~75)
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FStoneLine> StoneLines;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 StoneChance = 75;

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
	// 장신구 5칸 = Necklace ~ Ring2. 그 뒤 팔찌·어빌리티 스톤 (2026-10-25)
	inline bool IsAccessorySlot(EEquipSlot Slot) { return Slot >= EEquipSlot::Necklace && Slot <= EEquipSlot::Ring2; }
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
	LOA_API extern const FName PolishShard;    // 영롱한 보석 파편 — 장신구 연마 재료

	/** 부위별 시작 장비 (1640 세트 = 재련 +10) — EEquipSlot 순서 */
	LOA_API FName GetStarterEquipment(EEquipSlot Slot);

	/** 장신구 정의로 전투 특성을 굴린다 — 치명·특화·신속·제압 중 서로 다른 AccessoryStatCount종 (인내·숙련은 효과가 없어 빼둠) */
	LOA_API TArray<FStatRoll> RollAccessoryStats(const FItemDefinition& Def);

	/** 칸 왼쪽 위 표시 — 재련 장비만 "+10", 장신구·팔찌·스톤은 빈 문자열 */
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

	/** 장신구 부위의 연마 단계(0~3) 기본 효과 주 스탯·체력 — 로아 T4 장신구 표 (목걸이 > 귀걸이 > 반지, itemRandom 5000~5003 = 연마 0~3) */
	constexpr int32 AccessoryTierCount = 4;
	LOA_API FGearStats GetAccessoryStats(EEquipSlot Slot, int32 PolishLevel);

	/** 세 자리마다 쉼표 — 100000000 → "100,000,000". 언어 설정과 무관하게 항상 쉼표 */
	LOA_API FString FormatWithCommas(int64 Value);
}
