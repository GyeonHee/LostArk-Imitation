#pragma once

#include "CoreMinimal.h"
#include "Item/ItemTypes.h"
#include "Stat/StatTypes.h"

class UEquipmentComponent;
class UInventoryComponent;

/**
 * 장신구 연마 (로아 T4) — 수치는 Maxroll 게임 데이터 itemRandom 5009(목걸이)·5109(귀걸이)·5209(반지)
 *  - 유물 장신구는 최대 3단계. 단계마다 효과 1개를 무작위 부여(이미 붙은 종류는 제외), 등급 하 63% · 중 30% · 상 7%
 *  - 단계가 오르면 기본 효과(주 스탯·체력, LoAItems::GetAccessoryStats)와 깨달음 포인트(LoAArkPassive::GetAccessoryEnlightenment)도 오른다
 *  - 재료 = 영롱한 보석 파편 + 골드. 1단계 10개·600골드는 로아 화면 그대로, 2·3단계는 추정(20·1,200 / 30·1,800)
 *  - 서포터·파티 효과(낙인력·게이지·파티원 회복/보호막·아군 강화)와 상태이상·전투 중 회복은 이 게임에 시스템이 없어 효과 없음
 */
namespace LoAPolish
{
	constexpr int32 MaxLevel = 3;

	struct FEffectDef
	{
		EPolishEffect Effect = EPolishEffect::MaxHP;
		const TCHAR* Name = TEXT("");
		float Values[3] = { 0.f, 0.f, 0.f };  // 하 / 중 / 상 — bPercent면 % 단위(1.6 = 1.6%)
		bool bPercent = false;
		bool bHasEffect = true;
	};

	struct FCost
	{
		int32 Shards = 0;
		int64 Gold = 0;
	};

	/** 그 부위(목걸이·귀걸이·반지)에 붙을 수 있는 효과 10종 — 부위 전용 4종 + 공통 6종, 로아 목록 순서 */
	LOA_API TArray<EPolishEffect> GetEffectsFor(EEquipSlot Part);
	LOA_API const FEffectDef& GetDef(EPolishEffect Effect);

	/** "추가 피해 +1.60%" */
	LOA_API FString DescribeValue(EPolishEffect Effect, int32 Grade);
	LOA_API FString Describe(const FPolishEffectRoll& Roll);
	/** 하 파랑 · 중 보라 · 상 노랑 (로아 캐릭터 정보 창의 장신구 ◆ 색) */
	LOA_API FLinearColor GetGradeColor(int32 Grade);
	LOA_API const TCHAR* GetGradeName(int32 Grade);
	/** 등급 확률 (하 0.63 / 중 0.30 / 상 0.07) */
	LOA_API float GetGradeChance(int32 Grade);

	/** NextLevel(1~3) 단계로 연마하는 비용 */
	LOA_API FCost GetCost(int32 NextLevel);

	/** 이 장신구에 붙은 연마 효과 → 스탯 수정치 (효과 없는 종류는 건너뜀) */
	LOA_API void AppendModifiers(const FEquipmentInstance& Instance, TArray<FStatModifier>& OutMods);

	/** 아직 안 붙은 종류 중 하나를 고르고 등급을 굴린다 */
	LOA_API FPolishEffectRoll Roll(EEquipSlot Part, const TArray<FPolishEffectRoll>& Existing);

	enum class EResult : uint8 { Success, NoAccessory, MaxLevel, NotEnoughShards, NotEnoughGold };

	/** 장신구 인스턴스(Id)를 한 단계 연마 — 재료 차감 → 효과 부여 → 스탯 갱신(착용 중이면) → 즉시 디스크 저장 */
	LOA_API EResult TryPolish(UInventoryComponent& Inventory, UEquipmentComponent* Equipment, const FGuid& Id, FPolishEffectRoll& OutRoll);

	/** 연마 창 목록 — 소지품(착용 칸 제외)의 장신구 중 연마가 안 끝난 것 (유저 요청, 2026-10-25) */
	LOA_API TArray<FGuid> CollectPolishable(const UInventoryComponent* Inventory);
	LOA_API FText GetResultText(EResult Result);

	/** 테스트용 고정 세팅 — 3단계 + 부위별 중·중·하 (목걸이 적주피·추피 / 귀걸이 공퍼·무공 / 반지 치적·치피) */
	LOA_API void ApplyPresetFullPolish(FEquipmentInstance& Instance);
}
