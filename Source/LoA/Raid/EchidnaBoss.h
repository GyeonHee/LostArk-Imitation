#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EchidnaBoss.generated.h"

class UBossDirectionIndicatorComponent;
class ADamageNumberActor;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBossLineChanged, int32 /*NewLine*/);

// 광폭화 타이머가 다 되어 광폭화에 들어가는 순간 1회
DECLARE_MULTICAST_DELEGATE(FOnBossEnraged);

// 정산 게이지 값이 바뀔 때마다 (0~100)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettlementGaugeChanged, float /*NewGauge*/);

// 줄 단위가 아니라 HP가 조금이라도 깎일 때마다 — HP 바를 부드럽게 채우기 위해 UI가 구독한다
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBossHPChanged, double /*NewHP*/, double /*MaxHP*/);

// 정산/반정산 등 HP-줄 기준으로 한 번만 발동하는 대형 패턴 트리거 1개
USTRUCT(BlueprintType)
struct FBossPatternThreshold
{
	GENERATED_BODY()

	// 이 줄 이하로 내려가면 발동 대상 (예: 210)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 TriggerLine = 0;

	// StateTree의 Condition/Task에서 참조하는 식별자 (예: "MirrorCounter")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PatternName;
};

/**
 * 에키드나 2관문 보스
 * HP를 로스트아크 HP바 표기 방식과 동일하게 "줄" 단위로도 조회 가능하게 하고,
 * 특정 줄에 도달하면 한 번만 발동하는 대형 패턴(거울 카운터 등)을 추적한다.
 * 실제 패턴 실행 로직은 StateTree(AEchidnaBossAIController)에서 이 액터의 상태를 참조해 처리한다.
 */
UCLASS()
class LOA_API AEchidnaBoss : public ACharacter
{
	GENERATED_BODY()

public:
	AEchidnaBoss();

	virtual void BeginPlay() override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ReceiveDamage(float DamageAmount);

	/** 발밑 앞/뒤 방향 표시(정면은 가운데가 뾰족한 호, 후방은 매끈한 호) — 보스에 붙어 있어
	 *  보스가 회전하면 같이 돈다. 모양/색은 이 컴포넌트의 Details에서 조절 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Indicator")
	TObjectPtr<UBossDirectionIndicatorComponent> DirectionIndicator;

	/** 피격 시 띄울 데미지 숫자 액터 — BP_Echidna에서 BP_DamageNumber 할당. 비어 있으면 숫자를 안 띄운다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<ADamageNumberActor> DamageNumberClass;

	// 보스 중심(캡슐 중심) 기준으로 숫자가 뜨는 높이 오프셋 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float DamageNumberHeight = 60.f;

	// 연타 시 숫자가 완전히 겹쳐 하나처럼 보이지 않도록 흩뿌리는 반경 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float DamageNumberJitter = 70.f;

	// 풀피 기준 총 줄 수 (에키드나 싱글모드 = 285줄)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 TotalLines = 285;

	// double인 이유: 최대 체력이 40억대라 float(유효숫자 약 7자리)로는 512 단위로 뭉개져
	// 표기 숫자가 실제 값과 달라지고 작은 데미지가 오차에 묻힌다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	double HP = 0.0;

	// 에키드나 싱글모드 실제 최대 체력 (1줄 ≒ 16,655,155)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	double MaxHP = 4746719168.0;

	// HP 감소에 따라 한 번씩 발동하는 대형 패턴 목록 (거울 카운터 = 210줄)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TArray<FBossPatternThreshold> BigPatternThresholds;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	double GetHP() const { return HP; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 GetCurrentLine() const;

	// StateTree Condition에서 사용 — 이 패턴이 아직 발동 전인지
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	bool IsPatternTriggered(FName PatternName) const;

	// StateTree Task에서 대형 패턴 상태 진입 시 호출 — 다시 발동되지 않도록 표시만 함
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void MarkPatternTriggered(FName PatternName);

	// 발동 표시를 지워 다시 발동 가능하게 — 정산 게이지가 한 바퀴 돌아 0으로 돌아갈 때 사용
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void UnmarkPatternTriggered(FName PatternName);

	/** 시간 패턴(똥장판·랜잡 등) 진행 중 표시 — Task가 Enter에서 켜고 Exit에서 끈다.
	 *  Boss Enrage Time Reached 조건은 이게 켜져 있으면 무조건 false →
	 *  StateTree 조건의 PatternName이 Task와 달라도 매 틱 재진입(패턴이 영원히 처음부터 다시 시작)하지 않는다 */
	void SetActiveTimedPattern(FName PatternName) { ActiveTimedPattern = PatternName; }
	void ClearActiveTimedPattern() { ActiveTimedPattern = NAME_None; }

	UFUNCTION(BlueprintPure, Category = "Pattern")
	bool IsTimedPatternActive() const { return !ActiveTimedPattern.IsNone(); }

	// 정수 "줄" 값이 바뀔 때만 브로드캐스트 (HP 델타 자체는 신경쓰지 않음)
	FOnBossLineChanged OnLineChanged;

	// 데미지를 받을 때마다 브로드캐스트 — 보스 HP UI(UBossHPWidget)가 구독
	FOnBossHPChanged OnHPChanged;

	// ── 광폭화 ──────────────────────────────────────────────
	// 레이드 시작(보스 BeginPlay)부터 이 시간이 지나면 광폭화 (초, 기본 9분)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage")
	float EnrageTimeLimit = 540.f;

	// 광폭화 시 보스·AI·패턴 액터에 거는 CustomTimeDilation — 이동/애니메이션/StateTree/패턴 진행이 전부 이 배율로 빨라진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage")
	float EnrageSpeedMultiplier = 2.f;

	// 광폭화 중 플레이어가 받는 데미지 배율 (ALoACharacter::ReceiveDamage에서 적용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enrage")
	float EnrageDamageMultiplier = 2.f;

	UFUNCTION(BlueprintPure, Category = "Enrage")
	bool IsEnraged() const { return bEnraged; }

	// 광폭화까지 남은 시간(초). 광폭화 후엔 0, 광폭화 전에 보스가 죽으면 그 시점 값에서 멈춘다
	UFUNCTION(BlueprintPure, Category = "Enrage")
	float GetEnrageRemainingTime() const;

	/** 월드의 보스 중 하나라도 광폭화 중이면 그 속도 배율, 아니면 1.
	 *  패턴 액터가 BeginPlay에서 자기 CustomTimeDilation으로 쓰고, 월드 타이머 시간은 이 값으로 나눈다 */
	static float GetEnrageTimeScale(const UObject* WorldContext);

	// 월드의 보스 중 하나라도 광폭화 중이면 그 데미지 배율, 아니면 1
	static float GetEnrageDamageMultiplier(const UObject* WorldContext);

	FOnBossEnraged OnEnraged;

	// ── 정산 게이지 ──────────────────────────────────────────
	// 0~100(%). 2칸짜리 게이지라 50%가 반정산, 100%가 풀정산. 25%/75%에도 패턴이 있다.
	// 패턴 발동은 StateTree의 "Boss Settlement Gauge Reached" 조건이 이 값을 보고 판단하고,
	// 100%에 도달하면 더 이상 오르지 않는다(풀정산 패턴이 "Reset Settlement Gauge" Task로 0으로 돌려놓을 때까지)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementGauge = 0.f;

	// 자연 상승 — SettlementNaturalIntervalMin~Max초(매번 랜덤)마다 SettlementNaturalAmount%씩.
	// 평균 1.6초에 1% → 매혹 없이도 레이드 시작 40초(광폭화까지 8분 20초)쯤 25%
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementNaturalIntervalMin = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementNaturalIntervalMax = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementNaturalAmount = 1.f;

	// 플레이어 매혹 스택이 1 쌓일 때마다 추가 상승 (Min~Max% 랜덤)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementPerCharmStackMin = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementPerCharmStackMax = 3.f;

	// 매혹 3스택(= 매혹 상태) 도달 시 위 스택 보너스에 더해 추가 상승 (Min~Max% 랜덤)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementOnCharmedMin = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	float SettlementOnCharmedMax = 15.f;

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	void AddSettlementGauge(float Amount);

	/** 큰 패턴(시간·정산) 진행 중엔 게이지가 멈춘다 — 자연 상승도 매혹 보너스도 안 쌓임. UI는 이 동안 흑백 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool IsSettlementPaused() const { return IsTimedPatternActive(); }

	/** 정산 패턴 발동 지점(%) — 25/75 = 똥장판, 50/100 = 거울잇기. 한 바퀴(100% 패턴) 동안 각각 한 번씩만 쓰인다.
	 *  이름이 아니라 이 숫자로 추적하므로 StateTree 조건에는 %만 적으면 된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	TArray<int32> SettlementThresholds = { 25, 50, 75, 100 };

	/** 게이지가 이미 넘었는데 아직 패턴을 안 쓴 지점 중 가장 낮은 것 (-1 = 없음).
	 *  게이지가 한 번에 여러 지점을 넘어도(매혹 보너스 등) 낮은 것부터 하나씩 차례로 나온다 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	int32 GetNextSettlementThreshold() const;

	/** 정산 패턴 Task가 진입 시 호출 — GetNextSettlementThreshold()를 "사용함"으로 표시하고 그 값을 돌려준다 */
	int32 ConsumeNextSettlementThreshold();

	// 게이지를 0으로 + 사용한 발동 지점 초기화 — 풀정산(100%) 패턴이 끝날 때 호출, 다음 바퀴에 25/50/75/100이 다시 나온다
	UFUNCTION(BlueprintCallable, Category = "Settlement")
	void ResetSettlementGauge();

	/** 플레이어 매혹 스택이 쌓였을 때 ALoACharacter::AddCharmGauge가 호출 — 월드의 모든 보스 게이지를 올린다.
	 *  bReachedMaxStacks면 매혹 상태 진입 보너스까지 더한다 */
	static void NotifyCharmStackGained(const UObject* WorldContext, bool bReachedMaxStacks);

	FOnSettlementGaugeChanged OnSettlementGaugeChanged;

private:
	UPROPERTY(Transient)
	TSet<FName> TriggeredPatterns;

	FName ActiveTimedPattern = NAME_None;

	FTimerHandle EnrageTimerHandle;

	// 광폭화 타이머 시작 시각 (World TimeSeconds) — UI가 남은 시간을 계산하는 기준
	double EnrageStartTime = 0.0;

	// 광폭화 전에 보스가 죽은 시점의 남은 시간 (음수 = 아직 살아있음)
	float FrozenEnrageRemaining = -1.f;

	bool bEnraged = false;

	void Enrage();

	FTimerHandle SettlementTimerHandle;

	UPROPERTY(Transient)
	TSet<int32> ConsumedSettlementThresholds;

	// 다음 자연 상승을 Min~Max 사이 랜덤 시간 뒤로 예약 (매번 간격이 달라지도록 루핑 대신 단발 재예약)
	void ScheduleNaturalSettlement();
	void TickNaturalSettlement();

	int32 LastBroadcastLine = -1;

	// 나중에 맞은 숫자가 앞에 그려지도록 계속 증가시키는 카운터 (ADamageNumberActor의 TranslucentSortPriority로 전달)
	int32 DamageNumberCounter = 0;

	void SpawnDamageNumber(float DamageAmount);
};
