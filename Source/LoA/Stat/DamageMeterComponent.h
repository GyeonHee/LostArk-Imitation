#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageMeterComponent.generated.h"

// 피격 한 번의 결과 — ULoADamageLibrary::ReportDamageDealt가 만들어 넘긴다
struct FLoADamageEvent
{
	FName SkillRowName;           // DT_Skills 행 이름 (태그가 없으면 None → "기타")
	float FinalDamage = 0.f;      // 치명·제압까지 적용된 최종값
	bool bCritical = false;
	bool bTrainingTarget = false; // 허수아비 — 5초 무피격이면 세션 종료
	TWeakObjectPtr<const AActor> Target;
};

// 스킬 하나의 누적 기록
struct FDamageMeterSkillEntry
{
	FName SkillRowName;
	double TotalDamage = 0.0;
	int32 Hits = 0;
	int32 CritHits = 0;
	double MaxHit = 0.0;
};

DECLARE_MULTICAST_DELEGATE(FOnDamageMeterChanged);

/**
 * 데미지 미터 (플레이어 컨트롤러에 부착) — 보스·허수아비에 준 피해를 세션 단위로 스킬별 집계한다.
 *  - 세션 시작 = 첫 피격. 허수아비 세션은 마지막 피격 후 IdleTimeout(5초) 무피격이면 끝나고, 다음 피격이 새 세션을 연다
 *  - 보스 세션은 시간 제한 없이 이어진다(패턴으로 몇 초씩 못 때리는 구간이 있어서). 레벨이 바뀌면 컴포넌트째 새로 생김
 *  - 전투 시간 = 첫 피격 ~ (진행 중이면 지금 / 끝났으면 마지막 피격)
 */
UCLASS(ClassGroup=(LoA), meta=(BlueprintSpawnableComponent))
class LOA_API UDamageMeterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageMeterComponent();

	void RecordHit(const FLoADamageEvent& Event);

	/** 기록을 지우고 다음 피격부터 새 세션 */
	void ResetSession();

	bool HasSession() const { return SessionStartTime >= 0.0; }
	bool IsTrainingSession() const { return HasSession() && bTrainingSession; }
	bool IsSessionActive() const;
	double GetElapsedSeconds() const;
	double GetTotalDamage() const { return TotalDamage; }
	double GetDPS() const;
	int32 GetTotalHits() const { return TotalHits; }
	int32 GetTotalCritHits() const { return TotalCritHits; }

	/** 총피해가 큰 순서 */
	TArray<FDamageMeterSkillEntry> GetSortedEntries() const;

	/** 콘솔 LoADpsTest — Seconds초 뒤 세션 결과를 로그(CSV 한 줄 + 스킬별)로 남긴다. 지금 기록은 지우고 시작 */
	void StartDpsTest(float Seconds);

	/** 기록이 바뀔 때(피격·초기화) — 창이 즉시 다시 그리게 */
	FOnDamageMeterChanged OnChanged;

	/** 허수아비 세션이 끝나는 무피격 시간(초) */
	UPROPERTY(EditAnywhere, Category="DamageMeter")
	float IdleTimeout = 5.f;

private:
	void FinishDpsTest();
	void LogSession(const TCHAR* Label) const;

	TMap<FName, FDamageMeterSkillEntry> Entries;
	double TotalDamage = 0.0;
	int32 TotalHits = 0;
	int32 TotalCritHits = 0;
	double SessionStartTime = -1.0;
	double LastHitTime = -1.0;
	bool bTrainingSession = false;

	FTimerHandle DpsTestTimer;
	float DpsTestSeconds = 0.f;
};
