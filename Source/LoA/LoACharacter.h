// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Skill/SkillManagerComponent.h"
#include "Data/CharacterDataAsset.h"
#include "LoACharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHPChanged, float /*NewHP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMPChanged, float /*NewMP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharmGaugeChanged, int32 /*NewCharmGauge*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnockdownChanged, bool /*bKnockedDown*/);

/**
 *  A controllable top-down perspective character
 */
UCLASS(abstract)
class ALoACharacter : public ACharacter
{
	GENERATED_BODY()

private:

	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	float MPRegenAccum = 0.f;

	// 눕는 단계 진입 후 KnockdownDuration 뒤 자동 기상시키는 타이머
	FTimerHandle KnockdownTimerHandle;

	// 마지막 타격 후 KnockdownHopSettleTime 뒤 실제로 눕는 단계로 전환시키는 타이머 — SettleKnockdown() 호출
	FTimerHandle KnockdownSettleTimerHandle;

	// 넉다운 중 아직 "튕겨나가는 중"(정착 타이머 대기)인지 — true인 동안 재히트하면 타이머가 갱신되어 계속 공중에 떠 있는 것처럼 보임
	bool bKnockdownAirborne = false;

	void SettleKnockdown();

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skills")
	TObjectPtr<USkillManagerComponent> SkillManager;

	// 에디터에서 DA_Sorceress 등 할당 — BeginPlay에서 스탯 초기화에 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
	TObjectPtr<UCharacterDataAsset> CharacterData;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float AttackPower = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float HP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float MP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxMP;

	// 매혹 게이지 — 특수 타일(똥장판 등)에 의해 누적, 가득 차면 별도 디버프 트리거용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	int32 CharmGauge = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 MaxCharmGauge = 10;

	// 넉다운 상태 여부 — 특정 패턴에 맞아 쓰러진 동안 true (이동/스킬 입력 차단은 컨트롤러 쪽에서 처리)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knockdown")
	bool bIsKnockedDown = false;

	// 쓰러진 뒤 자동으로 일어나기까지 걸리는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockdown")
	float KnockdownDuration = 3.f;

	// 넉다운 시 공격 반대 방향으로 튕겨나가는 수평 힘 (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockdown")
	float KnockdownHopStrength = 500.f;

	// 넉다운 시 위로 띄우는 힘 (cm/s) — 있어야 제자리에서 즉시 넘어지지 않고 점프하듯 포물선을 그리며 넘어짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockdown")
	float KnockdownHopUpwardStrength = 300.f;

	// 마지막 타격 후 이 시간(초)이 지나야 실제로 눕는 단계로 전환됨 — MovementMode의 Falling→Walking 전환에
	// 기대지 않고 타이머로 직접 보장. 이 시간 안에 다시 맞으면 타이머가 갱신되어 계속 "튕겨나가는 중"으로 유지됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockdown")
	float KnockdownHopSettleTime = 0.4f;

	FOnHPChanged OnHPChanged;
	FOnMPChanged OnMPChanged;
	FOnCharmGaugeChanged OnCharmGaugeChanged;
	FOnKnockdownChanged OnKnockdownChanged;

	/** Controller가 매 프레임 설정하는 원하는 이동 속도벡터 (ZeroVector = 이동 없음) */
	FVector ControllerMoveVelocity = FVector::ZeroVector;

protected:
	/** 초당 MaxMP의 몇 배를 회복할지 (서브클래스에서 오버라이드 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MPRegenRate = 0.044f;

public:
	/** Constructor */
	ALoACharacter();

	virtual void PostInitializeComponents() override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	float GetHP() const { return HP; }
	float GetMaxHP() const { return MaxHP; }
	float GetMP() const { return MP; }
	float GetMaxMP() const { return MaxMP; }

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** 대쉬 실행 — 서브클래스에서 오버라이드하여 직업별 이동 방식 구현 */
	UFUNCTION(BlueprintNativeEvent, Category="Movement")
	void ExecuteDash(const FVector& TargetLocation);
	virtual void ExecuteDash_Implementation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void ReceiveDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual bool ConsumeMP(float Amount);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void RestoreMP(float Amount);

	/** 매혹 게이지 누적 (MaxCharmGauge에서 클램프) */
	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void AddCharmGauge(int32 Amount);

	/** 넉다운 상태로 전환 — SourceLocation 반대 방향으로 뒤로 튕겨나가 넘어지고, KnockdownHopSettleTime 뒤 실제로 눕는 단계로 전환됨.
	 * 이미 튕겨나가는 중일 때 다시 호출하면(연속 틱 데미지 등) 정착 타이머가 갱신되어 계속 공중에 떠 있는 것처럼 보임 —
	 * 완전히 누운 뒤에는 재호출해도 무시됨 */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual void ApplyKnockdown(const FVector& SourceLocation);

	/** TargetLocation 방향으로 수평으로 끌어당김 (예: 에키드나 "줄기" 촉수 패턴) — 넉다운과 달리 상태 전환/입력 차단 없이
	 * 순수하게 이동만 발생. 수직 속도는 건드리지 않으므로(LaunchCharacter의 bZOverride=false) bConstrainToPlane을
	 * 따로 풀어줄 필요 없음 */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual void ApplyPull(const FVector& TargetLocation, float PullStrength);

	/** 넉다운 즉시 해제 (자동 기상/즉시 기상 공통 진입점) */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual void GetUpFromKnockdown();

	/** 스페이스바 즉시 기상 시도 — 넉다운 상태이고 SkillManager의 GetUpSlotIndex(19) 쿨타임이 다 찼을 때만 성공.
	 * 쿨타임/아이콘은 대시(슬롯18)와 완전히 같은 방식으로 SkillManagerComponent가 관리 —
	 * UI도 대시 쿨타임 UI와 똑같이 SkillManager->GetCooldownRatio(19) / IsSlotOnCooldown(19)에 바인딩하면 됨 */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual bool TryInstantGetUp();

	UFUNCTION(BlueprintPure, Category="Knockdown")
	bool IsKnockedDown() const { return bIsKnockedDown; }

	/** true인 동안은 아직 튕겨나가는 중(공중) — 실제로 바닥에 눕는 포즈는 이 값이 false로 바뀐 뒤(OnKnockdownSettled)에 재생할 것.
	 * IsKnockedDown()만 보고 눕는 애니메이션을 틀면 아직 공중에 떠 있는 동안에도 누운 포즈가 재생되어 "공중에 뜬 채 누워있는" 것처럼 보임 */
	UFUNCTION(BlueprintPure, Category="Knockdown")
	bool IsKnockdownAirborne() const { return bKnockdownAirborne; }

	/** 넉다운 상태가 바뀔 때 호출 — 쓰러짐/기상 애니메이션은 BP에서 구현.
	 * bKnockedDown=true는 "튕겨나가기 시작"하는 시점(아직 공중)에 호출되므로, 실제 누운 포즈 재생은
	 * 이 이벤트가 아니라 아래 OnKnockdownSettled(바닥에 닿은 시점)에 걸어야 함 */
	UFUNCTION(BlueprintImplementableEvent, Category="Knockdown")
	void OnKnockdownVisualChanged(bool bKnockedDown);

	/** 튕겨나가던 캐릭터가 실제로 바닥에 닿아 정착한 순간(SettleKnockdown) 호출 — 여기서부터 KnockdownDuration 동안 누운 포즈 재생 */
	UFUNCTION(BlueprintImplementableEvent, Category="Knockdown")
	void OnKnockdownSettled();

	/** Returns the camera component **/
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** Returns the Camera Boom component **/
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

};

