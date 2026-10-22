#pragma once

#include "CoreMinimal.h"
#include "RaidReward.generated.h"

/** 보상 한 줄 — ItemId가 LoARaidReward::Gold/Silling이면 화폐(칸 안 차지), 아니면 아이템. 대기 보상으로 저장 파일에도 들어간다 */
USTRUCT(BlueprintType)
struct FLoARewardItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int64 Count = 0;

	FLoARewardItem() = default;
	FLoARewardItem(FName InItemId, int64 InCount) : ItemId(InItemId), Count(InCount) {}
};

/**
 * 레이드 클리어 보상 (계획서 5단계) — 지금은 C++ 표. 난이도·레이드가 늘면 DataTable(DT_RaidReward)로 옮길 것
 *  - 기본 보상: 수호석·파괴석·돌파석·골드·실링 (각각 범위 안 랜덤), 확률 보상: 숨결, 장신구 1개(6단계 — 특성은 소지품에 들어갈 때 굴림)
 *  - 첫 클리어: 기본 보상 × FirstClearMultiplier (테스트 명령을 쓴 판은 제외)
 *  - 더보기: 클리어 화면에서 BonusCost 골드를 내면 재료(화폐 제외)를 BonusRatio만큼 더 (로아처럼 골드)
 *  - 실패·중단은 보상 없음
 * 약 80회 클리어에 전 부위 +25가 되도록 4단계 재련 기대값에서 역산한 양
 */
namespace LoARaidReward
{
	LOA_API extern const FName Silling;
	LOA_API extern const FName Gold;

	/** 칸을 차지하지 않는 화폐 줄인지 */
	inline bool IsCurrency(FName ItemId) { return ItemId == Silling || ItemId == Gold; }

	constexpr int32 FirstClearMultiplier = 3;
	constexpr int64 BonusCost = 1500;   // 골드 — 한 판 골드 보상(평균 6,100)의 약 1/4
	constexpr float BonusRatio = 0.5f;

	/** 에키드나 클리어 보상 굴림 */
	LOA_API TArray<FLoARewardItem> RollEchidna(bool bFirstClear);

	/** 더보기 — 받은 보상 중 재료(화폐 제외)만 BonusRatio만큼 (올림) */
	LOA_API TArray<FLoARewardItem> MakeBonus(const TArray<FLoARewardItem>& Rewards);

	/** 같은 ItemId끼리 합친다 (기본 + 더보기를 한 줄로 보여줄 때) */
	LOA_API void Merge(TArray<FLoARewardItem>& Into, const TArray<FLoARewardItem>& Add);
}
