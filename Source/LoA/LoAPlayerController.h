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
	TObjectPtr<UHUD_ViewModel> HUDViewModel;

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

	/** Distance to destination at which movement stops */
	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin=0, Units="cm"))
	float AutoMoveAcceptanceRadius = 20.0f;

public:

	/** Constructor */
	ALoAPlayerController();

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

	void OnPlayerHPChanged(float NewHP);
	void OnPlayerMPChanged(float NewMP);

	/** 캐릭터의 HP/MP 변경 델리게이트에 바인딩하고 ViewModel 초기화 */
	void BindCharacterEvents(ALoACharacter* InCharacter);
};


