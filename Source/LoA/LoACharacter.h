// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Skill/SkillManagerComponent.h"
#include "Data/CharacterDataAsset.h"
#include "Stat/LoADamageTypes.h"
#include "LoACharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UWidgetComponent;
class UBattleItemComponent;
class UInventoryComponent;
class UStatComponent;
class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;
class ULoACharacterAnimInstance;
struct FCombatStats;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHPChanged, float /*NewHP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMPChanged, float /*NewMP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharmGaugeChanged, int32 /*NewCharmGauge*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnockdownChanged, bool /*bKnockedDown*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharmedChanged, bool /*bCharmed*/);
DECLARE_MULTICAST_DELEGATE(FOnPlayerDied);

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

	/** UStatComponent 재계산 결과를 캐릭터에 반영 — 공격력, 최대 HP(현재 HP 비율 유지), 이동속도 */
	void HandleStatsChanged(const FCombatStats& NewStats);

	// DA의 MoveSpeed — 신속 배율을 곱할 기준값
	float BaseMoveSpeed = 400.f;

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

	/** BodyMeshOverride가 있으면 BP의 마네킹 메시·AnimBP를 원본 모델 + ULoACharacterAnimInstance로 바꾼다 */
	void ApplyBodyMeshOverride();

	// 저스트 가드 자세 — BeginJustGuardStance가 켜고 타이머가 끈다
	bool bInJustGuardStance = false;

protected:
	// 애니메이션에 맞춰 코드가 캐릭터를 미는 동안(소서리스 대시) — 입력 잠금(IsActionLocked)
	bool bScriptedMoveLock = false;

	// SetVisualLift — 메시 기준 높이(BeginPlay 때 상대 Z) 위로 Current를 Target까지 보간
	float VisualLiftTarget = 0.f;
	float VisualLiftCurrent = 0.f;
	float BodyMeshBaseZ = 0.f;
	bool bBodyMeshBaseZValid = false;

private:
	FTimerHandle JustGuardStanceTimerHandle;
	void EndJustGuardStance();

	// BeginPlay 시점의 SpringArm 길이 (BP에서 바꿔둔 값 포함) — 줌 오버라이드 해제 시 여기로 돌아간다
	float DefaultCameraArmLength = 0.f;

	// TryConsumeTickDamage — 종류별 마지막으로 틱 데미지를 받은 월드 시각
	TMap<FName, double> LastTickDamageTimes;

	// BeginPlay 시점의 SpringArm 회전 (BP에서 바꾼 값도 존중) / 패턴 오버라이드
	FRotator DefaultCameraRotation = FRotator::ZeroRotator;
	bool bCameraRotationOverride = false;
	FRotator CameraRotationOverride = FRotator::ZeroRotator;
	float CameraArmOverride = -1.f;

	void Die();

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

	// 배틀아이템 칸 1~4 (회복약 등) — 인벤토리에서 드래그해 등록
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Items")
	TObjectPtr<UBattleItemComponent> BattleItems;

	// 소지품 10x10 (I키)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Items")
	TObjectPtr<UInventoryComponent> Inventory;

	// 장착 장비 6부위 (P키 캐릭터 정보 창) — 지능·무기공격력을 StatComponent에 더한다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Items")
	TObjectPtr<class UEquipmentComponent> Equipment;

	// 보석 11칸 (7단계)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gem")
	TObjectPtr<class UGemComponent> Gems;

	// 각인 5칸 (7단계)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Engraving")
	TObjectPtr<class UEngravingComponent> Engravings;

	// 아크 패시브 — 진화·깨달음·도약 (2026-10-24)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ArkPassive")
	TObjectPtr<class UArkPassiveComponent> ArkPassive;

	// 스탯(지능·무기공격력·체력 + 전투 특성) 계산 — 공격력·최대 HP·이동속도는 여기서 나온다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	TObjectPtr<UStatComponent> StatComponent;

	// 에디터에서 DA_Sorceress 등 할당 — BeginPlay에서 스탯 초기화에 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
	TObjectPtr<UCharacterDataAsset> CharacterData;

	// StatComponent가 재계산 때마다 써넣는 값 — 기존 BP 참조(ABP_BasicAttack 등)를 깨지 않으려고 남겨둠.
	// 새 코드는 StatComponent->GetStats().AttackPower를 읽을 것
	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float AttackPower = 7911200.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float HP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHP;

	// BeginPlay가 끝나 HP를 한 번 채운 뒤 true — 그 뒤 최대 HP가 바뀌어도 현재 HP는 유지(장비 착용·해제)
	bool bHPSettled = false;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Death")
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Death")
	bool bInvulnerable = false;
	// 대시·점멸 이동 중 — 모든 피해·CC·매혹 면역(IsImmune)
	bool bMoveImmune = false;
	// 다음 기상이 스페이스 즉시 기상인지 — GetUpFromKnockdown이 애니메이션을 고른다
	bool bInstantGetUpPending = false;
	// 기상 동작 중 — 이동·스킬 입력 잠금(마우스를 누르고 있으면 바로 걸어가 애니메이션이 끊겼다). 루트 이동(구르기)은 허용
	bool bGetUpLock = false;
	bool bSkillAnimMoveLock = false;
	FTimerHandle SkillAnimMoveLockTimerHandle;
	FTimerHandle GetUpLockTimerHandle;

	// ── 각인 피격 효과 (7단계) ──
	double DivineReadyTime = 0.0;          // 여신의 가호 다음 발동 가능 시각
	double CrisisReadyTime = 0.0;          // 위기 모면
	bool bCrisisActive = false;            // 위기 모면 무적 중 — 받는 피해를 모아 끝날 때 40% 회복
	float CrisisAccumulated = 0.f;
	FTimerHandle CrisisTimer;
	void EndCrisisEvasion();
	double RescueReadyTime = 0.0;          // 긴급구조
	float ShieldHP = 0.f;
	FTimerHandle RescueTimer;
	void EndEmergencyRescue();
	float BaseMaxMP = 0.f;                 // DA 값 — 최대 마나 증가 각인이 여기에 곱한다

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

	/** 원본 모델 애니메이션(액션 레이어) — 몸 메시가 ULoACharacterAnimInstance가 아니면(마네킹) nullptr·무시. 스킬·상태가 쓴다 */
	ULoACharacterAnimInstance* GetLoAAnim() const;
	void PlayStateAnim(UAnimSequence* Sequence, bool bLoop, float BlendTime = 0.15f, bool bStopOnMove = false);
	/** Only = 지금 재생 중인 액션이 그것일 때만 끊는다(다른 상태가 덮어쓴 뒤 늦게 오는 해제 무시) */
	void StopStateAnim(const UAnimSequence* Only, float BlendTime = 0.2f);

	// ── 원본 모델 (ULoACharacterAnimInstance — AnimBP 없이 C++가 포즈 계산) ──
	// 비어 있으면 BP에 설정된 메시·AnimBP(마네킹) 그대로. 값은 캐릭터 클래스 생성자(ASorceress 등)가 채운다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim|Mesh")
	TObjectPtr<USkeletalMesh> BodyMeshOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim|Mesh")
	float BodyMeshScale = 1.f;

	// 메시 정면이 +X면 0 (마네킹은 +Y라 -90)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim|Mesh")
	float BodyMeshYaw = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim|Mesh")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	// 무기를 붙일 본/소켓
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim|Mesh")
	FName WeaponSocket;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Anim|Mesh")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> RunAnim;

	// 이 속도(cm/s) 이상이면 달리기 100%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	float RunAnimFullSpeed = 250.f;

	// 넉다운 — 첫 타에 동작 전체를 체인으로 틀고 마지막 프레임(누운 자세)을 기상까지 유지. 정착(SettleKnockdown)은 애니메이션을 안 건드림
	//  보통: KnockdownLoopAnim(down — 선 자세에서 뒤로 넘어져 눕기까지 한 번에)
	//  멀리 날아감(장풍, bBlown): Air → Land 또는 TwistAir → TwistLand 랜덤
	//  공중에서 또 맞음(에어본): Bound → BoundLand / 누운 채 또 맞음: BoundHitLand(맞아서 한 번 들썩)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> KnockdownAirAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> KnockdownLandAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> TwistKnockdownAirAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> TwistKnockdownLandAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> KnockdownLoopAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> BoundAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> BoundLandAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> BoundHitLandAnim;

	// 누워 있다가 저절로 일어남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> StandUpAnim;

	// 스페이스 즉시 기상(기상기) — 앞으로 구르며 일어남, 루트 이동 적용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> InstantGetUpAnim;

	// 기상 동작 동안 조작 잠금(초) — resurrect_1은 0.55초에 다 일어서고, sk_standup_1은 1.07초에 구르기가 끝난다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	float StandUpLockTime = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	float InstantGetUpLockTime = 1.07f;

	// 매혹 중 (반복)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> CharmAnim;

	// 기절·붙잡힘 (반복)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> StunAnim;

	// 경직 — 둘 중 랜덤
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> StaggerAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> StaggerAltAnim;

	// 사망 → 사망 반복
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> DeathAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> DeathLoopAnim;

	// 저스트 가드 자세 (반복)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anim")
	TObjectPtr<UAnimSequence> JustGuardAnim;

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

	// 오버라이드 없는 평소 카메라 각도(BeginPlay 값) — 미니맵 방향 고정용
	FRotator GetDefaultCameraRotation() const { return DefaultCameraRotation; }

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

	/** 피해 적용 — 물리/마법이면 보스 패턴 배율 × (1 - 그 방어력 감소율), True면 그대로. 광폭화 배율은 전부 */
	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void ReceiveDamage(float DamageAmount, ELoADamageKind Kind = ELoADamageKind::Magical);

	/** 같은 종류(Source)의 장판 틱 데미지가 여러 개 겹쳐도 Interval(실제 시간)당 한 번만 받게 — 이번 틱에 맞아도 되면 true.
	 *  예: 거울 카운터의 불길이 여러 줄 겹친 곳에 서 있어도 한 줄분만 들어온다 */
	bool TryConsumeTickDamage(FName Source, float Interval);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual bool ConsumeMP(float Amount);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void RestoreMP(float Amount);

	// HP·MP를 최대치로 — 대기 지역 정비소가 호출
	UFUNCTION(BlueprintCallable, Category="Stats")
	void RestoreFullStatus();

	// HP 회복 (최대치에서 클램프) — 사망 중엔 무시. 회복약이 호출
	UFUNCTION(BlueprintCallable, Category="Stats")
	void Heal(float Amount);

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
	 * 완전히 누운 뒤에는 재호출해도 무시됨.
	 * HopScale — 튕겨나가는 수평 힘 배율(장풍처럼 멀리 날리는 패턴용). 위로 뜨는 힘·정착 시간은 √배로 같이 늘어 포물선이 자연스럽게 길어짐
	 * bBlown — 멀리 날아가는 동작(장풍): knockdown·twistknockdown 중 랜덤. 아니면 down */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual void ApplyKnockdown(const FVector& SourceLocation, float HopScale = 1.f, bool bBlown = false);

	/** 끌어당기기 (에키드나 "끌고간후 장판터지는"의 줄기 촉수) — 맞는 즉시 PullHoldDuration만큼 제자리에
	 * 멈췄다가(발이 묶인 연출) TargetLocation 쪽으로 PullSpeed(cm/s)로 강제로 끌려간다.
	 * 물리 임펄스가 아니라 Tick에서 위치를 직접 보간하므로 지형/속도와 무관하게 항상 목표 근처까지 확실히
	 * 도달한다. 끌려가는 내내 IsActionLocked()가 true라 조작 불가 */
	UFUNCTION(BlueprintCallable, Category="Pull")
	virtual void ApplyPull(const FVector& TargetLocation, float PullSpeed);

	/** 넉다운 즉시 해제 (자동 기상/즉시 기상 공통 진입점) */
	UFUNCTION(BlueprintCallable, Category="Knockdown")
	virtual void GetUpFromKnockdown();

	/** 넉다운 동작 — 첫 타(보통 down / 날아감 랜덤) 또는 공중 재히트(bound) */
	void PlayKnockdownAnim(bool bFirstHit, bool bBlown);

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
	 *  시간 제한이 없으므로 건 쪽(패턴 Task의 ExitState)이 반드시 false로 풀어줘야 한다.
	 *  bShowStunAnim — 기절 모션. 입장 이동·인트로·NPC 메뉴·연출처럼 "조작만 막는" 용도는 false(로비 → 보스맵에서 기절 모션이 나왔던 원인) */
	UFUNCTION(BlueprintCallable, Category="Held")
	void SetHeldByPattern(bool bHeld, bool bShowStunAnim = true);

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

	/** 넉다운·경직·기절·끌려가는 중·저스트 가드 자세 중 하나라도 걸려 있으면 이동/스킬 입력이 막혀야 하는 상태인지 — 컨트롤러의 입력 차단 체크에서 사용 */
	UFUNCTION(BlueprintPure, Category="Stagger")
	bool IsActionLocked() const { return bIsDead || bIsKnockedDown || bIsStaggered || bIsStunned || bIsPulled || bIsHeld || bInJustGuardStance || bScriptedMoveLock || bGetUpLock; }

	/** 몸(메시)만 이만큼 띄운다 — 캡슐·판정은 그대로, Tick에서 부드럽게 오르내림. 혹한의 부름처럼 공중에 떠서 시전하는 동작용. 0 = 내림 */
	void SetVisualLift(float Height) { VisualLiftTarget = Height; }

	/** 스킬 발동 애니메이션 동안 클릭 이동만 막는다(스킬·대시는 됨) — 즉발 스킬을 쓰고 바로 다른 쪽을 클릭하면 캐릭터가 돌아서
	 *  미뤄진 효과가 그쪽으로 나가던 문제. 다시 부르면 더 긴 쪽으로 연장, CC(ForceCancel)면 SkillBase가 풀어 준다 */
	void LockMoveForSkillAnim(float Duration);
	void ClearSkillAnimMoveLock();
	bool IsSkillAnimMoveLocked() const { return bSkillAnimMoveLock; }

	/** 저스트 가드 자세 — Duration 동안 제자리(이동·스킬 잠금, 피격·CC 판정은 그대로). 진행 중인 저스트 가드 장판이 G를 받았을 때만 */
	void BeginJustGuardStance(float Duration);

	UFUNCTION(BlueprintPure, Category="JustGuard")
	bool IsInJustGuardStance() const { return bInJustGuardStance; }

	/** 저스트 가드 자세 시작/끝 — 막는 자세 애니메이션은 BP에서 */
	UFUNCTION(BlueprintImplementableEvent, Category="JustGuard")
	void OnJustGuardStanceChanged(bool bInStance);

	// ── 사망 ─────────────────────────────────────────────────
	// HP가 0 이하가 되면 ReceiveDamage가 Die()를 부른다. 이후 데미지·CC·매혹을 전부 무시하고 조작 불가(IsActionLocked).
	// 패배 연출(흑백 화면 + "공략에 실패하였습니다." + 정비소 복귀)은 OnDied를 구독한 ALoAPlayerController가 담당
	UFUNCTION(BlueprintPure, Category="Death")
	bool IsDead() const { return bIsDead; }

	FOnPlayerDied OnDied;

	// 무적 — 데미지·CC·매혹을 전부 무시한다. 레이드 클리어 순간 켠다(남은 장판에 맞아 클리어 후 죽지 않게)
	UFUNCTION(BlueprintCallable, Category="Death")
	void SetInvulnerable(bool bInInvulnerable) { bInvulnerable = bInInvulnerable; }

	// 사망했거나 무적이면 데미지·CC를 받지 않는다
	UFUNCTION(BlueprintPure, Category="Death")
	bool IsImmune() const { return bIsDead || bInvulnerable || bMoveImmune; }

	/** 긴급구조 보호막 남은 양 (0 = 없음) */
	float GetShieldHP() const { return ShieldHP; }

	// 쓰러지는 애니메이션 등 연출 훅
	UFUNCTION(BlueprintImplementableEvent, Category="Death")
	void OnDeathVisualChanged(bool bDead);

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

