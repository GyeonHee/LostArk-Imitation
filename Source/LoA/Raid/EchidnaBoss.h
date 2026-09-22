#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EchidnaBoss.generated.h"

class UBossDirectionIndicatorComponent;
class ADamageNumberActor;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBossLineChanged, int32 /*NewLine*/);

// 줄 단위가 아니라 HP가 조금이라도 깎일 때마다 — HP 바를 부드럽게 채우기 위해 UI가 구독한다
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBossHPChanged, float /*NewHP*/, float /*MaxHP*/);

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

	// 풀피 기준 총 줄 수 (에키드나 2관문 솔로 = 210줄, 다인 하드는 285줄)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 TotalLines = 210;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float HP = 0.f;

	// 1줄 = 100,000 = 플레이어 최대체력 1개분 (줄당 체감을 고정하기 위한 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MaxHP = 21000000.f;

	// HP 감소에 따라 한 번씩 발동하는 대형 패턴 목록 (거울 카운터는 285줄 기준 210줄 → 솔로 210줄 환산 시 155줄)
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

	// 데미지를 받을 때마다 브로드캐스트 — 보스 HP UI(UBossHPWidget)가 구독
	FOnBossHPChanged OnHPChanged;

private:
	UPROPERTY(Transient)
	TSet<FName> TriggeredPatterns;

	int32 LastBroadcastLine = -1;

	// 나중에 맞은 숫자가 앞에 그려지도록 계속 증가시키는 카운터 (ADamageNumberActor의 TranslucentSortPriority로 전달)
	int32 DamageNumberCounter = 0;

	void SpawnDamageNumber(float DamageAmount);
};
