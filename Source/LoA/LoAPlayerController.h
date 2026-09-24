// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "LoAPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UPathFollowingComponent;
class USkillManagerComponent;
class UHUD_ViewModel;
class USkillTree_ViewModel;
class ALoACharacter;
class UBossHPWidget;
class UCastBarWidget;
class AEchidnaBoss;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Player controller for a top-down perspective game.
 *  Implements point and click based controls
 */
UCLASS(abstract)
class ALoAPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Component used for moving along a NavMesh path. */
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, Category="Input")
	float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** MappingContext */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 스킬 전용 MappingContext */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TObjectPtr<UInputMappingContext> SkillMappingContext;

	/** 스킬 슬롯 IA (인덱스 0=Q, 1=W, 2=E, 3=R, 4=A, 5=S, 6=D, 7=F) */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TArray<TObjectPtr<UInputAction>> SkillSlotActions;

	/** 기본공격 IA (좌클릭 / C키) */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TObjectPtr<UInputAction> BasicAttackAction;

	/** 대쉬 IA (스페이스바) */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TObjectPtr<UInputAction> DashAction;

	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UUserWidget> HUDWidget;

	/** 보스 HP 바 위젯 클래스 — BP_LoAPlayerController에서 WBP_BossHP 할당.
	 *  레벨에 AEchidnaBoss가 없으면 위젯 자체를 만들지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBossHPWidget> BossHPWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UBossHPWidget> BossHPWidget;

	/** HP 갱신 때마다 남은 줄 수를 다시 물어보기 위해 들고 있는 참조 */
	TWeakObjectPtr<AEchidnaBoss> TrackedBoss;

	/** AEchidnaBoss::OnHPChanged 구독 콜백 — 위젯에 HP 비율과 줄 수를 넘긴다 */
	void OnBossHPChanged(double NewHP, double NewMaxHP);

	/** 캐스팅/차지 진행바 위젯 클래스 — BP_LoAPlayerController에서 WBP_CastBar 할당 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UCastBarWidget> CastBarWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UCastBarWidget> CastBarWidget;

	/** Tick에서 매 프레임 호출 — SkillManager를 폴링해 진행바를 갱신하거나 숨긴다 */
	void UpdateCastBar();

	// ── 미니맵 ──
	UPROPERTY(Transient)
	TObjectPtr<class UMinimapWidget> MinimapWidget;

	// ── 화면 연기 (랜잡 패턴) ──
	UPROPERTY(Transient)
	TObjectPtr<class UScreenFogWidget> ScreenFogWidget;

	float FogCurrentOpacity = 0.f;
	float FogTargetOpacity = 0.f;
	float FogFadeSpeed = 1.f;

	// Tick에서 연기 불투명도를 목표값으로 보간 — UpdateCastBar와 같이 IsActionLocked 조기 return보다 앞에서 호출
	void UpdateScreenFog(float DeltaSeconds);

public:
	/** 화면 전체 핑크 연기 켜기/끄기 — FadeTime초 동안 서서히. 위젯은 처음 켤 때 만들고 HUD 아래(ZOrder -1)에 깐다 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetScreenFog(bool bEnable, float FadeTime = 1.f);

protected:
	// 보스 HP 바 옆 광폭화 타이머 + 초상화 아래 정산 게이지 갱신 — UpdateCastBar와 같은 이유로 Tick의 IsActionLocked 조기 return보다 앞에서 호출
	void UpdateEnrageTimer();

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UHUD_ViewModel> HUDViewModel;

	/** K 키 — 스킬트리 토글 */
	UPROPERTY(EditAnywhere, Category="Input|UI")
	TObjectPtr<UInputAction> SkillTreeAction;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> SkillTreeWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> SkillTreeWidget;

	UPROPERTY(BlueprintReadOnly, Category="UI")
	TObjectPtr<USkillTree_ViewModel> SkillTreeViewModel;

	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	/** Set to true if we're using touch input */
	uint32 bIsTouch : 1;

	/** Saved location of the character movement destination */
	FVector CachedDestination;

	/** Time that the click input has been pressed */
	float FollowTime = 0.0f;

	/** True while auto-moving to a clicked destination */
	bool bAutoMoving = false;

	/** True while right-click is held down */
	bool bHoldMoving = false;

	/** True after StopMovement (dash) until velocity returns to normal */
	bool bDashSuppressed = false;

	/** Minimum frames to keep suppressed (prevents instant clear while PendingLaunchVelocity not yet applied) */
	int32 DashSuppressFrames = 0;

	/** Whether auto-move was active before the dash — restored after dash ends */
	bool bWasAutoMovingBeforeDash = false;

	/** 기본공격·스킬로 멈춘 뒤 새 이동 클릭(OnInputStarted)이 들어오기 전까지 true — 이동 버튼을 누른 채였어도 다시 걷지 않게 한다 */
	bool bMoveHaltedByAttack = false;

	/** 스킬트리 토글 중복 호출 방지용 타임스탬프 */
	float LastSkillTreeToggleTime = -1.f;

	/** Distance to destination at which movement stops */
	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin=0, Units="cm"))
	float AutoMoveAcceptanceRadius = 20.0f;

	/** 매혹(3스택) 상태 동안 무작위 이동/스킬 사용을 반복 실행하는 타이머 — 시작/정지는 OnPlayerCharmedChanged에서 관리 */
	FTimerHandle CharmConfusionTimerHandle;

	/** 매혹 중 무작위로 누른 스킬 슬롯을 짧게 뗄 때 쓰는 타이머 */
	FTimerHandle CharmSkillReleaseTimerHandle;

	/** 매혹 중 현재 붙잡고 있는 스킬 슬롯 (-1=없음) */
	int32 CharmActiveSkillSlot = -1;

	/** 매혹 중 무작위 행동(이동 목표 재설정 + 쓸 수 있는 스킬 소모)을 반복하는 간격 (초).
	 *  짧을수록 이동이 산만해지고 쿨이 도는 족족 스킬이 빠져나간다 */
	UPROPERTY(EditAnywhere, Category="Charm")
	float CharmActionInterval = 0.8f;

	/** 매혹 중 무작위 이동 목표 지점을 고를 반경 (cm) */
	UPROPERTY(EditAnywhere, Category="Charm")
	float CharmWanderRadius = 400.f;

public:

	/** Constructor */
	ALoAPlayerController();

	// 스킬 등 외부에서 강제 이동 목표 설정
	void ForceMoveTo(const FVector& Destination);
	void CancelAutoMove();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void OnPossess(APawn* InPawn) override;

	virtual void StopMovement() override;

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	
	/** Input handlers */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();

	/** Helper function to get the move destination */
	void UpdateCachedDestination();

	/** 스킬 입력 핸들러 */
	void OnSkillKeyDown(int32 SlotIndex);
	void OnSkillKeyHeld(int32 SlotIndex);
	void OnSkillKeyUp(int32 SlotIndex);

	/** 대쉬 입력 핸들러 */
	void OnDashInput();

	/** 현재 빙의된 캐릭터의 SkillManager 반환 */
	UFUNCTION(BlueprintCallable, Category="Skills")
	USkillManagerComponent* GetSkillManager() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	void OnSkillTreeToggle();

	void OnPlayerHPChanged(float NewHP);
	void OnPlayerMPChanged(float NewMP);

	/** 매혹 상태(3스택) 전환 시 호출 — 시작되면 무작위 이동/스킬 사용 타이머 시작, 끝나면 정지하고 붙잡고 있던 스킬 키를 뗌 */
	void OnPlayerCharmedChanged(bool bCharmed);

	/** 매혹 중 CharmActionInterval마다 호출 — 무작위 지점으로 이동 목표를 다시 잡고,
	 *  쿨타임이 돌아 쓸 수 있는 슬롯(0~7) 중 하나를 골라 타입별 필요 시간만큼 붙잡아 실제로 발동시킨다 */
	void PerformRandomCharmAction();

	/** 매혹 중 붙잡고 있던 스킬 슬롯을 떼는 콜백 — 떼는 순간 다음 틱에서 새 스킬을 고를 수 있게 된다 */
	void ReleaseCharmSkill();

	/** 캐릭터의 HP/MP 변경 델리게이트에 바인딩하고 ViewModel 초기화 */
	void BindCharacterEvents(ALoACharacter* InCharacter);
};


