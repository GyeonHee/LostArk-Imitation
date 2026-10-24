#pragma once

#include "CoreMinimal.h"
#include "Item/ItemTypes.h"
#include "Stat/StatTypes.h"

class UInventoryComponent;
class UEquipmentComponent;

/**
 * 팔찌 (로아 T4 유물) — 수치는 Maxroll 게임 데이터 itemRandom 213300023#5
 *  - 효과 = 고정 효과(획득 시 무작위, 변경 불가) + 부여 효과([팔찌 효과 부여] NPC). 종류별 상한 기본 2 · 전투 특성 2 · 특수 4(고정·부여 공유), 같은 종류는 1개만
 *  - 줄을 뽑을 때 분류 비중 기본 35 : 전투 특성 35 : 특수 30, 특수 효과 하·중·상 6 : 3 : 1
 *  - 기본 효과 주 스탯 6,400~12,800 / 체력 3,000~5,000, 전투 특성 41~100
 *  - 유물 = 고정 2 + 부여 3칸, 부여·변환 4회(로아 화면 "4회 부여 가능") — 칸 수는 추정
 *  - 비용(유저 지정, 실링): 효과 부여·변환 24,000 / 재변환 60,000 — 둘 다 남은 횟수 1회를 쓴다
 *  - 첫 부여는 빈 칸을 채우고, 그 뒤는 잠그지 않은 부여 효과를 다시 굴려 **기존 / 신규 중 하나를 고른다**(T4 [효과 선택]).
 *    고르기 전에 [재변환]하면 신규 효과만 다시 굴린다. 잠금은 무료. 재변환권 아이템은 없음
 *  - 도약 포인트 유물 +9 (고대 +18). 데이터의 설명 없는 특수 효과(type 3)는 뺐다
 */
namespace LoABracelet
{
	constexpr int32 LeapPointsRelic = 9;
	constexpr int32 RelicFixedCount = 2;
	constexpr int32 RelicEnchantSlots = 3;
	constexpr int32 RelicEnchantChances = 4;
	constexpr int64 EnchantSillingCost = 24000;
	constexpr int64 RerollSillingCost = 60000;

	enum class ECategory : uint8 { Base, CombatStat, Special };

	struct FEffectDef
	{
		EBraceletEffect Effect = EBraceletEffect::Crit;
		const TCHAR* Name = TEXT("");
		float Values[3] = { 0.f, 0.f, 0.f };  // 특수 효과 하 / 중 / 상 — bPercent면 % 단위
		bool bPercent = false;
		bool bHasEffect = true;
	};

	LOA_API const FEffectDef& GetDef(EBraceletEffect Effect);
	LOA_API bool IsBaseEffect(EBraceletEffect Effect);
	LOA_API bool IsCombatStat(EBraceletEffect Effect);
	LOA_API ECategory GetCategory(EBraceletEffect Effect);
	LOA_API int32 GetCategoryLimit(ECategory Category);
	/** 그 분류에서 부여될 수 있는 효과 전부 (창의 "부여 가능한 효과" 목록) */
	LOA_API TArray<EBraceletEffect> GetEffectsIn(ECategory Category);

	/** 기본 효과·전투 특성의 수치 범위 (하·중·상 판정과 굴림에 씀) */
	LOA_API void GetValueRange(EBraceletEffect Effect, int32& OutMin, int32& OutMax);
	/** 이 줄의 등급 0~2 — 특수 효과는 Grade, 수치형은 범위를 3등분 */
	LOA_API int32 GetGrade(const FBraceletEffectRoll& Roll);

	/** "치명 +80" / "치명타 적중률 +3.40%" — PrimaryStatName은 주 스탯 줄에 쓸 이름(소서리스 = 지능) */
	LOA_API FString Describe(const FBraceletEffectRoll& Roll, const FString& PrimaryStatName = TEXT("지능"));
	/** 부여 가능한 효과 목록 한 줄 — "지능 [+6,400~+12,800]" / "치명타 적중률 [+2.60%~+4.20%]" */
	LOA_API FString DescribeRange(EBraceletEffect Effect, const FString& PrimaryStatName = TEXT("지능"));

	/** 팔찌 효과 → 스탯 수정치 */
	LOA_API void AppendModifiers(const FEquipmentInstance& Instance, TArray<FStatModifier>& OutMods);

	/** 부여 효과만 (BraceletEffects에서 bFixed가 아닌 것, 순서대로) */
	LOA_API TArray<FBraceletEffectRoll> GetEnchants(const FEquipmentInstance& Instance);
	LOA_API int32 GetFixedCount(const FEquipmentInstance& Instance);

	/** 새 팔찌 — 고정 효과 2개 굴림 + 빈 부여 칸 3, 부여 4회 */
	LOA_API void Roll(FEquipmentInstance& Instance);

	enum class EEnchantResult : uint8 { Applied, NeedChoice, NoChances, NotEnoughSilling, AllLocked, Invalid };
	/**
	 * 부여·변환 한 번 — 실링 24,000 차감 + 횟수 -1 → 잠그지 않은 부여 칸을 새로 굴린 결과를 OutProposal(부여 효과 전체, 칸 순서)에 담는다.
	 * 바꿀 칸이 전부 비어 있었으면(첫 부여) 바로 적용(Applied), 아니면 NeedChoice — ApplyChoice로 기존/신규를 고른다
	 */
	LOA_API EEnchantResult Enchant(FEquipmentInstance& Instance, const TArray<bool>& Locked, UInventoryComponent& Inventory, TArray<FBraceletEffectRoll>& OutProposal);
	/** [효과 선택] 창의 [재변환] — 실링 60,000 + 횟수 -1, 같은 잠금으로 신규 효과만 다시 굴린다(기존 효과는 그대로) */
	LOA_API EEnchantResult Reroll(FEquipmentInstance& Instance, const TArray<bool>& Locked, UInventoryComponent& Inventory, TArray<FBraceletEffectRoll>& OutProposal);
	/** 신규(OutProposal)를 고르면 부여 효과를 통째로 바꾼다. 기존을 고르면 아무것도 안 바뀜(실링·횟수는 이미 썼음) */
	LOA_API void ApplyChoice(FEquipmentInstance& Instance, const TArray<FBraceletEffectRoll>& Proposal, bool bTakeNew);
	LOA_API FText GetEnchantResultText(EEnchantResult Result);

	/** 2026-10-25 유저 요청 세팅 — 고정 치명·특화 중옵(80) + 부여 지능 중 · 치명타 적중률 중 · 치명타 피해 중, 남은 변환 3회 */
	LOA_API void ApplyStarterPreset(FEquipmentInstance& Instance);
}

/**
 * 어빌리티 스톤 (로아 T4 유물) — 증가 각인 2줄 + 감소 효과 1줄, 줄마다 세공 10칸 ([어빌리티 스톤 세공사] NPC)
 *  - 세공 성공 확률 75%에서 시작, 성공하면 -10%, 실패하면 +10% (25~75%) — 세 줄이 확률을 공유(감소 줄에선 "균열 확률")
 *  - 활성 단계: 증가 각인 6 → Lv1 / 7 → Lv2 / 9 → Lv3 / 10 → Lv4, 감소 효과 5 → Lv1 / 7 → Lv2 / 10 → Lv3 (공식 가이드 각인 페이지)
 *  - 스톤 각인은 **장착한 각인에만** 더해진다 (스톤 레벨별 보너스는 LoAEngravings — Maxroll engraving 1xxx spec[0])
 *  - 증가 두 줄 세공 성공 합 14 / 15 / 16회 → 체력 +1,175 / +2,350 / +3,525 (로아 세공 창 화면), 17회부터는 +1,175씩 이어 붙임(추정)
 *  - 증가 두 줄 레벨 합 5 이상 → 기본 공격력 +1.5% (공식 가이드)
 *  - 감소 효과(engraving 1800~1803): 공격력 -2/4/6% · 방어력 -5/10/15% · 공격 속도 -2/4/6% · 이동 속도 -2/4/6%
 *  - 세공 비용 실링 1,925/칸 (로아 세공 창 화면 — 은화), 자동 세공 = 남은 30칸 × 1,925 = 57,750 실링
 *  - 자동 세공: 아직 한 칸도 세공하지 않은 스톤만(공식 가이드). 증가 두 줄 레벨 합 5를 가장 우선으로, 못 미치면 4 > 3 … 순으로 결과가 좋도록 매 칸 줄을 고른다
 *    (확률이 같으면 성공률 45% 이하일 때 감소 줄). 공식 가이드의 "합산 레벨 기준을 충족하도록 성공 확률을 극대화"를 따름
 */
namespace LoAStone
{
	constexpr int32 FacetsPerLine = 10;
	constexpr int32 LineCount = 3;
	constexpr int64 FacetSillingCost = 1925;
	constexpr int32 AutoTargetLevel = 5;          // 자동 세공 목표 — 세공 기회 10이면 합산 레벨 5 (공식 가이드 표)
	constexpr int32 VitalityFirstSuccesses = 14;   // 체력 보너스가 붙기 시작하는 증가 두 줄 성공 합
	constexpr int32 VitalityPerSuccess = 1175;
	constexpr int32 AttackBonusLevel = 5;
	constexpr float AttackBonusPercent = 0.015f;

	extern LOA_API const FName AtkReduction;
	extern LOA_API const FName DefReduction;
	extern LOA_API const FName AtkSpeedReduction;
	extern LOA_API const FName MoveSpeedReduction;

	LOA_API bool IsNegativeLine(int32 LineIndex);
	/** 성공 수 → 활성 단계 (증가 0~4 / 감소 0~3) */
	LOA_API int32 GetLevel(int32 Successes, bool bNegative);
	/** 활성 단계가 오르는 성공 수 (증가 6/7/9/10, 감소 5/7/10) — 창의 칸 아래 "Lv.N" 표시 */
	LOA_API TArray<int32> GetLevelThresholds(bool bNegative);
	LOA_API FString GetLineName(const FStoneLine& Line, bool bNegative);
	/** "저주받은 인형 Lv.2 (7)" */
	LOA_API FString DescribeLine(const FStoneLine& Line, int32 LineIndex);
	/** 감소 효과 단계 수치 설명 ("공격력 -4%") */
	LOA_API FString DescribeNegativeEffect(FName Id, int32 Level);

	/** 증가 두 줄 성공 합 / 레벨 합 */
	LOA_API int32 GetTotalSuccesses(const FEquipmentInstance& Stone);
	LOA_API int32 GetTotalLevel(const FEquipmentInstance& Stone);
	/** 성공 합으로 받는 체력 (0이면 없음) */
	LOA_API int32 GetVitalityBonus(int32 TotalSuccesses);

	/** 이 스톤이 그 각인에 주는 레벨 (0~4) */
	LOA_API int32 GetEngravingLevel(const FEquipmentInstance& Stone, FName EngravingId);
	/** 감소 효과 + 세공 보너스(체력·기본 공격력) → 스탯 수정치 */
	LOA_API void AppendModifiers(const FEquipmentInstance& Stone, TArray<FStatModifier>& OutMods);

	LOA_API bool IsComplete(const FEquipmentInstance& Stone);
	LOA_API bool IsStarted(const FEquipmentInstance& Stone);
	LOA_API bool CanFacet(const FEquipmentInstance& Stone, int32 LineIndex);
	LOA_API int32 GetRemainingFacets(const FEquipmentInstance& Stone);

	enum class EFacetResult : uint8 { Success, Fail, Invalid, Full, NotEnoughSilling, AlreadyStarted };
	/** 한 칸 세공 — 실링 차감 → 굴림 → 확률 갱신. 저장은 부르는 쪽(소지품 갱신 + SaveNow) */
	LOA_API EFacetResult Facet(FEquipmentInstance& Stone, int32 LineIndex, UInventoryComponent& Inventory);
	/** 자동 세공 — 남은 30칸을 한 번에 (실링 30 × 1,925). 세공을 시작한 스톤은 AlreadyStarted */
	LOA_API EFacetResult AutoFacet(FEquipmentInstance& Stone, UInventoryComponent& Inventory);
	LOA_API FText GetFacetResultText(EFacetResult Result);

	/** 새 스톤 — 증가 각인 2개(43종 중 서로 다르게) + 감소 효과 1개, 세공 전 */
	LOA_API void Roll(FEquipmentInstance& Stone);

	/** 2026-10-25 유저 요청 세팅 — 저주받은 인형 7 / 아드레날린 7 (77돌) + 공격력 감소 4(비활성), 세공 완료 */
	LOA_API void ApplyStarterPreset(FEquipmentInstance& Stone);
}

/** 팔찌·스톤 NPC 창 공용 */
namespace LoAGearList
{
	/** 소지품(착용 칸 제외)에서 아직 손볼 게 남은 것만 — 스톤은 세공이 안 끝난 것, 팔찌는 부여 횟수가 남은 것 (유저 요청, 2026-10-25) */
	LOA_API TArray<FGuid> CollectWorkable(const UInventoryComponent* Inventory, EEquipSlot Slot);
	/** 장비 인스턴스를 바꾼 뒤 — 소지품 칸·창 갱신 + 착용 중이면 스탯 다시 적용 + 즉시 디스크 저장 */
	LOA_API void CommitChange(UInventoryComponent* Inventory, UEquipmentComponent* Equipment, const FGuid& Id);
}
