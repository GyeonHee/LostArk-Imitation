// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Skill/SkillManagerComponent.h"
#include "Data/CharacterDataAsset.h"
#include "LoACharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UWidgetComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHPChanged, float /*NewHP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMPChanged, float /*NewMP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharmGaugeChanged, int32 /*NewCharmGauge*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnockdownChanged, bool /*bKnockedDown*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharmedChanged, bool /*bCharmed*/);

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

	/** 매혹 게이지 머리 위 UI — Widget Class는 캐릭터 BP에서 WBP_CharmGauge(UCharmGaugeWidget 서브클래스)로 할당.
	 * 스택 0이면 위젯 자체가 숨겨짐(UCharmGaugeWidget::SetStacks 참조) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> CharmGaugeWidgetComponent;

	void HandleCharmGaugeChanged(int32 NewGauge);

	float MPRegenAccum = 0.f;

	// 눕는 단계 진입 후 KnockdownDuration 뒤 자동 기상시키는 타이머
	FTimerHandle KnockdownTimerHandle;

	// 마지막 타격 후 KnockdownHopSettleTime 뒤 실제로 눕는 단계로 전환시키는 타이머 — SettleKnockdown() 호출
	FTimerHandle KnockdownSettleTimerHandle;

	// 넉다운 중 아직 "튕겨나가는 중"(정착 타이머 대기)인지 — true인 동안 재히트하면 타이머가 갱신되어 계속 공중에 떠 있는 것처럼 보임
	bool bKnockdownAirborne = false;

	void SettleKnockdown();

	// 경직 지속시간이 끝나면 자동 해제하는 타이머 — ApplyStagger에서 재히트마다 갱신됨
	FTimerHandle StaggerTimerHandle;

	// BeginPlay 시점의 SpringArm 길이 (BP에서 바꿔둔 값 포함) — 줌 오버라이드 해제 시 여기로 돌아간다
	float DefaultCameraArmLength = 0.f;

	// TryConsumeTickDamage — 종류별 마지막으로 틱 데미지를 받은 월드 시각
	TMap<FName, double> LastTickDamageTimes;

	// BeginPlay 시점의 SpringArm 회전 (BP에서 바꾼 값도 존중) / 패턴 오버라이드
	FRotator DefaultCameraRotation = FRotator::ZeroRotator;
	bool bCameraRotationOverride = false;
	FRotator CameraRotationOverride = FRotator::ZeroRotator;
	float CameraArmOverride = -1.f;

	void EndStagger();

	// 기절 지속시간이 끝나면 자동 해제하는 타이머 — ApplyStun에서 재히트마다 갱신됨
	FTimerHandle StunTimerHandle;

	void EndStun();

	// 끌어당기기 — 맞은 직후 멈춰 있는 동안 도는 타이머. 끝나면 실제 드래그가 시작된다
	FTimerHandle PullHoldTimerHandle;

	// 패턴이 ReleasePull()을 못 부르고 끝나버려도 영구히 묶이지 않도록 하는 안전장치
	FTimerHandle PullSafetyTimerHandle;

	bool bPullDragging = false;
	FVector PullDestination = FVector::ZeroVector;
	float PullSpeedCmS = 0.f;
	float PullDragElapsed = 0.f;

	void BeginPullDrag();
	void TickPullDrag(float DeltaSeconds);

	/** 드래그만 끝내고 묶인 상태(bIsPulled)는 그대로 유지 — 해제는 ReleasePull()이 담당 */
	void FinishPullDrag();

	// 스택을 하나씩 깎는 루핑 타이머 — CharmGaugeStackDuration마다 1스택씩 감소, 재히트 시 다시 처음부터 갱신됨
	FTimerHandle CharmGaugeTimerHandle;

	// 매혹 상태 지속 타이머 — CharmedDuration 뒤 EndCharm()이 매혹 해제 + 스택 전체 초기화
	FTimerHandle CharmedTimerHandle;

	void DecayCharmGauge();
	void EndCharm();

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skills")
	TObjectPtr<USkillManagerComponent> SkillManager;

	// 에디터에서 DA_Sorceress 등 할당 — BeginPlay에서 스탯 초기화에 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
	TObjectPtr<UCharacterDataAsset> CharacterData;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float AttackPower = 7911200.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float HP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float MP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxMP;

	// 매혹 게이지(스택) — 똥장판/에키드나 매혹 패턴에 맞을 때마다 1스택씩 쌓임(최대 MaxCharmGauge, 기본 3).
	// 스택별 개별 타이머가 아니라 공용 타이머 하나가 CharmGaugeStackDuration마다 "1스택씩" 깎는다
	// (예: 2스택 → 30초 뒤 1스택 → 다시 30초 뒤 0스택). 재히트하면 그 타이머가 처음부터 다시 갱신됨.
	// 최대 스택에 도달하면 매혹 상태로 들어가고, 그때부터는 CharmedDuration 뒤 스택이 통째로 0이 됨(EndCharm 참조)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	int32 CharmGauge = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	int32 MaxCharmGauge = 3;

	// 매혹 게이지가 1스택 줄어드는 데 걸리는 시간(초) — 재히트 시 이 시간으로 다시 갱신됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float CharmGaugeStackDuration = 30.f;

	// 최대 스택 도달로 들어간 매혹 상태가 유지되는 시간(초) — 끝나면 스택이 전부 0으로 초기화됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float CharmedDuration = 10.f;

	// 매혹 상태 여부 — CharmGauge가 MaxCharmGauge에 도달하면 true. 이 동안은 컨트롤러가 실제 플레이어 입력을
	// 막고 대신 무작위 이동/스킬 사용을 대신 실행함 (ALoAPlayerController::Tick 참조)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	bool bIsCharmed = false;

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

	// 경직 상태 여부 — 넉다운과 달리 캐릭터를 띄우거나 쓰러뜨리지 않고 짧게만 행동불능으로 만드는 약한 히트리액션
	// (예: 두번긋고 도넛장판의 1·2번 슬래시). 넉다운 중엔 이미 더 강한 상태이므로 별도로 겹치지 않음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stagger")
	bool bIsStaggered = false;

	// 경직 지속시간 (초) — 넉다운(KnockdownDuration)보다 훨씬 짧게 잡는 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stagger")
	float StaggerDuration = 0.3f;

	// 기절 상태 여부 — 경직처럼 캐릭터를 띄우거나 쓰러뜨리지 않고 제자리에 완전히 얼어붙지만, 지속시간이
	// 훨씬 길다(예: 전방향 하트발사 3초). 경직과 달리 고정 Duration 필드 없이 ApplyStun() 호출마다 값을 받음 —
	// 패턴마다 기절 시간이 다를 수 있어서
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stun")
	bool bIsStunned = false;

	// 패턴에 붙잡힌 상태 — 랜잡 파리지옥에 먹힘 등. 기절과 달리 시간이 아니라 패턴이 끝날 때 풀어준다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Held")
	bool bIsHeld = false;

	// 끌려가는 중 여부 — "멈춤" 구간과 "드래그" 구간을 모두 포함한다(둘 다 조작 불가)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pull")
	bool bIsPulled = false;

	// 끌기에 맞은 직후 제자리에 멈춰 있는 시간 (초) — 이 뒤에 실제로 끌려가기 시작한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pull")
	float PullHoldDuration = 0.4f;

	// 목표 지점(보스) 앞 이만큼 남기고 멈춤 (cm) — 0이면 보스와 완전히 겹쳐버린다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pull")
	float PullStopDistance = 200.f;

	// 드래그 강제 종료 시간 (초) — 지형에 막혀 목표에 영원히 도달 못 하는 경우를 대비한 안전장치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pull")
	float PullMaxDragTime = 1.5f;

	// 끌려간 뒤 묶여 있는 최대 시간 (초) — 정상적으로는 패턴이 끝나며 ReleasePull()이 풀어주지만,
	// 패턴이 비정상 종료되어 호출되지 않는 경우에도 영구 속박이 되지 않도록 하는 안전장치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pull")
	float PullMaxHoldTime = 8.f;

	FOnHPChanged OnHPChanged;
	FOnMPChanged OnMPChanged;
	FOnCharmGaugeChanged OnCharmGaugeChanged;
	FOnKnockdownChanged OnKnockdownChanged;
	FOnCharmedChanged OnCharmedChanged;

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

	/** 패턴 연출용 카메라 줌 — SpringArm 길이를 ArmLength로 부드럽게 옮긴다. Clear하면 원래 길이로 복귀 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCameraZoomOverride(float ArmLength);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ClearCameraZoomOverride();

	/** 패턴 연출용 카메라 각도 — SpringArm 회전(절대 회전)을 Rotation으로 부드럽게 옮긴다.
	 *  Pitch를 얕게(예: -30) 주면 위에서 내려보던 시점이 내려와 비스듬히 보인다. Clear하면 원래 각도로 복귀 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCameraRotationOverride(FRotator Rotation);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ClearCameraRotationOverride();

	// 줌 보간 속도 (FInterpTo 속도 — 클수록 빨리 따라감)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraZoomInterpSpeed = 2.5f;

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

	/** 같은 종류(Source)의 장판 틱 데미지가 여러 개 겹쳐도 Interval(실제 시간)당 한 번만 받게 — 이번 틱에 맞아도 되면 true.
	 *  예: 거울 카운터의 불길이 여러 줄 겹친 곳에 서 있어도 한 줄분만 들어온다 */
	bool TryConsumeTickDamage(FName Source, float Interval);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual bool ConsumeMP(float Amount);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void RestoreMP(float Amount);

	/** 매혹 게이지 스택 추가 (MaxCharmGauge에서 클램프) — 호출될 때마다 스택 감소 타이머가 CharmGaugeStackDuration으로
	 * 다시 갱신됨. MaxCharmGauge에 도달하면 IsCharmed()가 true로 바뀌고 OnCharmedChanged가 브로드캐스트되며,
	 * CharmedDuration 뒤 EndCharm()이 매혹 해제 + 스택 초기화를 한다.
	 * 이미 매혹 상태면 아무 일도 하지 않는다 — 매혹 지속시간이 재히트로 늘어나면 안 되기 때문 */
	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void AddCharmGauge(int32 Amount);

	UFUNCTION(BlueprintPure, Category="Stats")
	bool IsCharmed() const { return bIsCharmed; }

	/** 넉다운 상태로 전환 — SourceLocation 반대 방향으로 뒤로 튕겨나가 넘어지고, KnockdownHopSettleTime 뒤 실제로 눕는 단계로 전환됨.
	 * 이미 튕겨나가는 중일 때 다시 호출하면(연속 틱 데미지 등) 정착 타이머가 갱신되어 계속 공중에 떠 있는 것처럼 보임 —
	 * 완전히 누운 뒤에는 재호출해도 무시됨 */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual void ApplyKnockdown(const FVector& SourceLocation);

	/** 끌어당기기 (에키드나 "끌고간후 장판터지는"의 줄기 촉수) — 맞는 즉시 PullHoldDuration만큼 제자리에
	 * 멈췄다가(발이 묶인 연출) TargetLocation 쪽으로 PullSpeed(cm/s)로 강제로 끌려간다.
	 * 물리 임펄스가 아니라 Tick에서 위치를 직접 보간하므로 지형/속도와 무관하게 항상 목표 근처까지 확실히
	 * 도달한다. 끌려가는 내내 IsActionLocked()가 true라 조작 불가 */
	UFUNCTION(BlueprintCallable, Category="Pull")
	virtual void ApplyPull(const FVector& TargetLocation, float PullSpeed);

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

	/** 경직 상태로 전환 — 캐릭터를 띄우거나 쓰러뜨리지 않고 StaggerDuration 동안만 이동/스킬 입력을 짧게 끊는다.
	 * 이미 넉다운 중이면 더 강한 상태이므로 무시. 경직 중 재히트하면 타이머가 갱신되어 계속 경직이 유지됨 */
	UFUNCTION(BlueprintCallable, Category="Stagger")
	virtual void ApplyStagger();

	UFUNCTION(BlueprintPure, Category="Stagger")
	bool IsStaggered() const { return bIsStaggered; }

	/** 기절 상태로 전환 — Duration(초) 동안 이동/스킬 입력을 완전히 차단하되, 넉다운과 달리 캐릭터를 띄우거나
	 * 쓰러뜨리지 않고 제자리에 그대로 얼어붙는다(경직과 동일한 방식, 지속시간만 호출마다 다르게 지정).
	 * 이미 넉다운 중이면 더 강한 상태이므로 무시. 기절 중 재히트하면 타이머가 갱신되어 계속 기절이 유지됨 */
	UFUNCTION(BlueprintCallable, Category="Stun")
	virtual void ApplyStun(float Duration);

	UFUNCTION(BlueprintPure, Category="Stun")
	bool IsStunned() const { return bIsStunned; }

	/** 패턴에 붙잡힘/풀림 — 붙잡히면 캐스팅·사거리 이동을 끊고 제자리에 멈춘 채 풀릴 때까지 조작 불가.
	 *  시간 제한이 없으므로 건 쪽(패턴 Task의 ExitState)이 반드시 false로 풀어줘야 한다 */
	UFUNCTION(BlueprintCallable, Category="Held")
	void SetHeldByPattern(bool bHeld);

	UFUNCTION(BlueprintPure, Category="Held")
	bool IsHeldByPattern() const { return bIsHeld; }

	UFUNCTION(BlueprintImplementableEvent, Category="Held")
	void OnHeldVisualChanged(bool bHeld);

	/** 끌려간 뒤에도 유지되는 속박을 푼다 — 끌기를 건 패턴이 끝날 때(StateTree ExitState) 호출할 것.
	 * 호출되지 않아도 PullMaxHoldTime이 지나면 자동으로 풀린다(영구 속박 방지) */
	UFUNCTION(BlueprintCallable, Category="Pull")
	void ReleasePull();

	UFUNCTION(BlueprintPure, Category="Pull")
	bool IsBeingPulled() const { return bIsPulled; }

	/** 끌려가는 상태가 바뀔 때 호출 — 버티는/끌려가는 애니메이션은 BP에서 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category="Pull")
	void OnPullVisualChanged(bool bPulled);

	/** 넉다운·경직·기절·끌려가는 중 하나라도 걸려 있으면 이동/스킬 입력이 막혀야 하는 상태인지 — 컨트롤러의 입력 차단 체크에서 사용 */
	UFUNCTION(BlueprintPure, Category="Stagger")
	bool IsActionLocked() const { return bIsKnockedDown || bIsStaggered || bIsStunned || bIsPulled || bIsHeld; }

	/** 경직 상태가 바뀔 때 호출 — 짧은 피격 리액션 애니메이션은 BP에서 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category="Stagger")
	void OnStaggerVisualChanged(bool bStaggered);

	/** 기절 상태가 바뀔 때 호출 — 제자리에 얼어붙는 리액션 애니메이션은 BP에서 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category="Stun")
	void OnStunVisualChanged(bool bStunned);

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

