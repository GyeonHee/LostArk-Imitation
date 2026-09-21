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

	/** 매혹 중 무작위 행동(이동 목표 재설정 + 스킬 사용)을 반복하는 간격 (초) */
	UPROPERTY(EditAnywhere, Category="Charm")
	float CharmActionInterval = 1.5f;

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

	/** 매혹 중 CharmActionInterval마다 호출 — 무작위 지점으로 이동 목표 설정 + 무작위 스킬 슬롯 하나를 짧게 누름 */
	void PerformRandomCharmAction();

	/** 매혹 중 무작위로 눌렀던 스킬 슬롯을 짧은 홀드 시간 뒤에 떼는 콜백 */
	void ReleaseCharmSkill();

	/** 캐릭터의 HP/MP 변경 델리게이트에 바인딩하고 ViewModel 초기화 */
	void BindCharacterEvents(ALoACharacter* InCharacter);
};


