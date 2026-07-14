#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EchidnaBoss.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBossLineChanged, int32 /*NewLine*/);

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

	// 풀피 기준 총 줄 수 (에키드나 2관문 = 285줄)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 TotalLines = 285;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float HP = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MaxHP = 285000.f;

	// HP 감소에 따라 한 번씩 발동하는 대형 패턴 목록 (기본값: 210줄 = 거울 카운터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TArray<FBossPatternThreshold> BigPatternThresholds;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	float GetHP() const { return HP; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 GetCurrentLine() const;

	// StateTree Condition에서 사용 — 이 패턴이 아직 발동 전인지
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	bool IsPatternTriggered(FName PatternName) const;

	// StateTree Task에서 대형 패턴 상태 진입 시 호출 — 다시 발동되지 않도록 표시만 함
	UFUNCTION(BlueprintCallable, Category = "Pattern")
	void MarkPatternTriggered(FName PatternName);

	// 정수 "줄" 값이 바뀔 때만 브로드캐스트 (HP 델타 자체는 신경쓰지 않음)
	FOnBossLineChanged OnLineChanged;

private:
	UPROPERTY(Transient)
	TSet<FName> TriggeredPatterns;

	int32 LastBroadcastLine = -1;
};
