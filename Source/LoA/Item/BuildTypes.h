#pragma once

#include "CoreMinimal.h"
#include "Stat/StatTypes.h"
#include "BuildTypes.generated.h"

struct FItemDefinition;
class UInventoryComponent;

/**
 * 7단계 — 보석·각인 (로아 T4 그대로, 2026-10-23)
 * 보석: 11칸, 스킬 하나에 겁화(피해) 1개 + 작열(쿨감) 1개까지. 보석 아이템은 종류·레벨만 있고 스킬은 끼울 때 고른다
 *       (로아는 보석마다 스킬이 정해져 있지만, 스킬이 8개뿐이라 아이템 160종이 되는 걸 피함)
 * 각인: 5칸, 유물 각인서로 0~4단계. 어빌리티 스톤 추가 레벨은 없음
 * 수치 출처: 보석 = Maxroll Gem System Guide(T4 표), 각인 = Maxroll 게임 데이터 engraving 표의 유물 0~4단계(스톤 0)
 */

UENUM(BlueprintType)
enum class EGemType : uint8
{
	Damage   UMETA(DisplayName = "겁화"),  // 그 스킬 피해 +
	Cooldown UMETA(DisplayName = "작열"),  // 그 스킬 쿨타임 -
};

/** 보석 칸 하나 — 빈 칸이면 GemItemId가 None */
USTRUCT(BlueprintType)
struct FGemSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gem")
	FName GemItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Gem")
	FName SkillRowName;

	bool IsEmpty() const { return GemItemId.IsNone(); }
};

namespace LoAGems
{
	constexpr int32 SlotCount = 11;
	constexpr int32 MaxLevel = 10;
	constexpr int32 FuseCount = 3;  // 같은 종류·레벨 3개 → 다음 레벨 1개 (100%)

	/** "Gem_Dmg_05" / "Gem_Cd_10" */
	LOA_API FName MakeGemId(EGemType Type, int32 Level);
	LOA_API bool ParseGemId(FName ItemId, EGemType& OutType, int32& OutLevel);

	/** 겁화 = 피해 비율(Lv5 0.24), 작열 = 쿨감 비율(Lv5 0.14) */
	LOA_API float GetEffect(EGemType Type, int32 Level);

	/** 보석마다 붙는 기본 공격력 % (Lv5 0.003) */
	LOA_API float GetAttackPowerPercent(int32 Level);

	/** "5레벨 겁화의 보석" */
	LOA_API FText GetGemName(EGemType Type, int32 Level);

	/** "스킬 피해 +24.00% / 기본 공격력 +0.30%" */
	LOA_API FString DescribeGem(EGemType Type, int32 Level);

	void RegisterItems(TMap<FName, FItemDefinition>& Map);

	/** 보석 상자 — 1레벨 보석 BoxGemCount개(겁화·작열 랜덤). 상점에서 골드로 산다 */
	extern LOA_API const FName GemBox;
	constexpr int32 BoxGemCount = 10;
	constexpr int64 BoxGoldPrice = 1000;

	/** 소지품 칸의 보석 상자를 연다 — 보석은 칸마다 1개라 빈 칸이 BoxGemCount개 있어야 함 */
	LOA_API bool OpenGemBox(UInventoryComponent* Inventory, int32 SlotIndex, FText& OutFailReason);
}

/** 각인 하나의 정의 — 효과는 LoAEngravings가 단계별로 계산 */
struct FEngravingDefinition
{
	FName Id;
	FText Name;
	FName BookItemId;   // "유물 <이름> 각인서"
	FString IconName;   // /Game/LostArk/UI/Engraving/ 텍스처 이름 (없으면 창에서 색 네모)
	bool bHasEffect = true;  // 이 게임에 그 시스템이 있어 실제로 효과가 있는가 (없으면 장착만 됨)
};

namespace LoAEngravings
{
	constexpr int32 SlotCount = 5;
	constexpr int32 MaxLevel = 4;
	constexpr int32 BooksPerLevel = 5;
	constexpr int32 MaxBooks = BooksPerLevel * MaxLevel;  // 로아처럼 0 / 20권 — 5권마다 +1단계, 20권이면 4단계 (처음부터 전부 유물 0단계로 장착 가능)

	// 런타임 중첩 각인 — 효과 계산에 중첩 수가 필요하다
	extern LOA_API const FName Adrenaline;
	extern LOA_API const FName MagickStream;
	extern LOA_API const FName EtherPredator;   // 스킬 사용마다(4초에 한 번) +1중첩, 90초, 최대 30 — 로아의 "에테르 획득"을 스킬 사용으로 대신
	extern LOA_API const FName CrushingFist;    // 카운터 성공 → 3초 공격력
	extern LOA_API const FName Propulsion;      // 대시 → 5초 스킬 피해
	constexpr int32 EtherMaxStacks = 30;
	constexpr float EtherDuration = 90.f;
	constexpr float EtherGainCooldown = 4.f;
	constexpr float CrushingFistDuration = 3.f;
	constexpr float PropulsionDuration = 5.f;
	constexpr int32 AdrenalineMaxStacks = 6;
	constexpr float AdrenalineDuration = 6.f;
	constexpr float AdrenalineAttackPerStack = 0.009f;
	constexpr int32 MagickStreamMaxStacks = 10;
	constexpr float MagickStreamDuration = 10.f;
	constexpr float MagickStreamMPRegenPerStack = 0.02f;

	LOA_API const TArray<FEngravingDefinition>& GetAll();
	LOA_API const FEngravingDefinition* Find(FName Id);
	LOA_API const FEngravingDefinition* FindByBook(FName BookItemId);

	/** 첫 번째 효과 수치 (그 단계) — 위기 모면 쿨타임 등 */
	LOA_API float GetValueA(FName Id, int32 Level);

	/** 읽은 권수 → 단계 0~4 (0권도 유물 0단계) */
	LOA_API int32 LevelFromBooks(int32 BooksRead);

	/** 장착만 해도 붙는 효과 (중첩 각인은 중첩과 무관한 부분만) */
	LOA_API void AppendStaticModifiers(FName Id, int32 Level, TArray<FStatModifier>& Out);

	/** 중첩 각인의 현재 중첩에 따른 효과 */
	LOA_API void AppendStackModifiers(FName Id, int32 Level, int32 Stacks, TArray<FStatModifier>& Out);

	/** 창에 쓰는 효과 설명 한 줄 */
	LOA_API FString Describe(FName Id, int32 Level);

	/** P창 "장착 중인 각인" 목록 — (효과 이름, 값) 줄들. 로아 각인 창처럼 */
	LOA_API void DescribeLines(FName Id, int32 Level, TArray<TPair<FString, FString>>& Out);

	/** 아이콘 텍스처 (/Game/LostArk/UI/Engraving/T_Eng_<Id>) — 없으면 null */
	LOA_API class UTexture2D* LoadIcon(FName Id);

	void RegisterItems(TMap<FName, FItemDefinition>& Map);
}
