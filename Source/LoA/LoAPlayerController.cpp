// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoAPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "LoACharacter.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "LoA.h"
#include "Skill/SkillManagerComponent.h"
#include "UI/HUD_ViewModel.h"
#include "Blueprint/UserWidget.h"
#include "View/MVVMView.h"

ALoAPlayerController::ALoAPlayerController()
{
	bIsTouch = false;
	bMoveToMouseCursor = false;
	ShortPressThreshold = 0.5f;

	// create the path following comp
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));

	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;

	PrimaryActorTick.bCanEverTick = true;
}

void ALoAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	HUDViewModel = NewObject<UHUD_ViewModel>(this);

	if (HUDWidgetClass && IsLocalController())
	{
		UUserWidget* Widget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
		if (Widget)
		{
			Widget->AddToViewport();
			if (UMVVMView* View = Widget->GetExtension<UMVVMView>())
			{
				bool bOk = View->SetViewModel(FName("HUD_ViewModel"), HUDViewModel);
				UE_LOG(LogTemp, Warning, TEXT("[HUD] SetViewModel: %s"), bOk ? TEXT("성공") : TEXT("실패 - 이름 불일치?"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[HUD] UMVVMView extension 없음 - WBP_HUD에 ViewModel 미등록?"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[HUD] Widget 생성 실패 - HUDWidgetClass 확인"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUD] 위젯 생성 건너뜀 - HUDWidgetClass: %s, IsLocal: %d"),
			HUDWidgetClass ? TEXT("설정됨") : TEXT("미설정"), IsLocalController());
	}

	// OnPossess가 BeginPlay 전에 이미 호출된 경우를 대비해 여기서도 바인딩 시도
	if (ALoACharacter* Char = GetPawn<ALoACharacter>())
	{
		BindCharacterEvents(Char);
	}
}

void ALoAPlayerController::StopMovement()
{
	bWasAutoMovingBeforeDash = bAutoMoving;
	bAutoMoving = false;
	bHoldMoving = false;
	bDashSuppressed = true;
	DashSuppressFrames = 2;
	Super::StopMovement();
}

void ALoAPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Controller Tick이 CMC Tick보다 먼저 실행되어야 RequestDirectMove가 같은 프레임에 적용됨
	if (UCharacterMovementComponent* CMC = InPawn ? InPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr)
	{
		CMC->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}

	if (HUDViewModel)
	{
		if (ALoACharacter* Char = Cast<ALoACharacter>(InPawn))
		{
			BindCharacterEvents(Char);
		}
	}
}

void ALoAPlayerController::BindCharacterEvents(ALoACharacter* InCharacter)
{
	InCharacter->OnHPChanged.RemoveAll(this);
	InCharacter->OnMPChanged.RemoveAll(this);

	HUDViewModel->SetMaxHP(InCharacter->GetMaxHP());
	HUDViewModel->SetHP(InCharacter->GetHP());
	HUDViewModel->SetMaxMP(InCharacter->GetMaxMP());
	HUDViewModel->SetMP(InCharacter->GetMP());

	InCharacter->OnHPChanged.AddUObject(this, &ALoAPlayerController::OnPlayerHPChanged);
	InCharacter->OnMPChanged.AddUObject(this, &ALoAPlayerController::OnPlayerMPChanged);
}

void ALoAPlayerController::OnPlayerHPChanged(float NewHP)
{
	if (HUDViewModel)
	{
		HUDViewModel->SetHP(NewHP);
	}
}

void ALoAPlayerController::OnPlayerMPChanged(float NewMP)
{
	if (HUDViewModel)
	{
		HUDViewModel->SetMP(NewMP);
	}
}

void ALoAPlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Only set up input on local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);

			if (SkillMappingContext)
			{
				Subsystem->AddMappingContext(SkillMappingContext, 1);
			}
		}

		// Set up action bindings
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// Setup mouse input events
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &ALoAPlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &ALoAPlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &ALoAPlayerController::OnSetDestinationReleased);

			// Setup touch input events
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &ALoAPlayerController::OnTouchTriggered);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &ALoAPlayerController::OnTouchReleased);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &ALoAPlayerController::OnTouchReleased);

			// 스킬 슬롯 바인딩 (루프로 8개 한꺼번에)
			for (int32 i = 0; i < SkillSlotActions.Num(); i++)
			{
				if (!SkillSlotActions[i]) continue;
				EnhancedInputComponent->BindAction(SkillSlotActions[i], ETriggerEvent::Started,   this, &ALoAPlayerController::OnSkillKeyDown, i);
				EnhancedInputComponent->BindAction(SkillSlotActions[i], ETriggerEvent::Triggered, this, &ALoAPlayerController::OnSkillKeyHeld, i);
				EnhancedInputComponent->BindAction(SkillSlotActions[i], ETriggerEvent::Completed, this, &ALoAPlayerController::OnSkillKeyUp,   i);
			}

			// 대쉬 바인딩
			if (DashAction)
			{
				EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &ALoAPlayerController::OnDashInput);
			}

			// 기본공격 바인딩 (슬롯 인덱스 8)
			if (BasicAttackAction)
			{
				EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started,   this, &ALoAPlayerController::OnSkillKeyDown, USkillManagerComponent::BasicAttackSlotIndex);
				EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Triggered, this, &ALoAPlayerController::OnSkillKeyHeld, USkillManagerComponent::BasicAttackSlotIndex);
				EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Completed, this, &ALoAPlayerController::OnSkillKeyUp,   USkillManagerComponent::BasicAttackSlotIndex);
			}

		}
		else
		{
			UE_LOG(LogLoA, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
		}
	}
}

void ALoAPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent());
	if (!CMC) return;

	if (bDashSuppressed)
	{
		if (DashSuppressFrames > 0)
		{
			--DashSuppressFrames;
			return;
		}
		if (CMC->Velocity.Size2D() <= CMC->MaxWalkSpeed + 1.0f)
		{
			bDashSuppressed = false;
			if (bWasAutoMovingBeforeDash && !bHoldMoving)
			{
				bAutoMoving = true;
				bWasAutoMovingBeforeDash = false;
			}
		}
		else
		{
			return;
		}
	}

	if (bAutoMoving)
	{
		FVector ToDestination = CachedDestination - ControlledPawn->GetActorLocation();
		ToDestination.Z = 0.f;

		const float FrameMoveDist = CMC->MaxWalkSpeed * DeltaSeconds + 1.0f;

		if (ToDestination.SizeSquared() <= FMath::Square(FrameMoveDist))
		{
			bAutoMoving = false;
			FVector SnapLocation(CachedDestination.X, CachedDestination.Y, ControlledPawn->GetActorLocation().Z);
			ControlledPawn->SetActorLocation(SnapLocation, false, nullptr, ETeleportType::TeleportPhysics);
			CMC->Velocity = FVector::ZeroVector;
			CMC->ClearAccumulatedForces();
		}
		else
		{
			const FVector Dir = ToDestination.GetSafeNormal();
			const float Speed = CMC->MaxWalkSpeed;
			CMC->Velocity = FVector(Dir.X * Speed, Dir.Y * Speed, CMC->Velocity.Z);
			CMC->RequestDirectMove(Dir * Speed, true);
			ControlledPawn->AddMovementInput(Dir, 1.0f, true);
		}
	}
	else if (bHoldMoving)
	{
		FVector ToDestination = CachedDestination - ControlledPawn->GetActorLocation();
		ToDestination.Z = 0.f;

		if (!ToDestination.IsNearlyZero())
		{
			const FVector Dir = ToDestination.GetSafeNormal();
			const float Speed = CMC->MaxWalkSpeed;
			CMC->Velocity = FVector(Dir.X * Speed, Dir.Y * Speed, CMC->Velocity.Z);
			CMC->RequestDirectMove(Dir * Speed, true);
			ControlledPawn->AddMovementInput(Dir, 1.0f, true);
		}
		else
		{
			CMC->Velocity = FVector(0.f, 0.f, CMC->Velocity.Z);
		}
	}
}

void ALoAPlayerController::OnInputStarted()
{
	bAutoMoving = false;
	bHoldMoving = false;
	bDashSuppressed = false;
	bWasAutoMovingBeforeDash = false;

	if (APawn* ControlledPawn = GetPawn())
	{
		if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
		{
			CMC->Velocity = FVector::ZeroVector;
			CMC->ClearAccumulatedForces();
		}
	}

	UpdateCachedDestination();
}

void ALoAPlayerController::OnSetDestinationTriggered()
{
	bAutoMoving = false;
	bHoldMoving = true;

	FollowTime += GetWorld()->GetDeltaSeconds();

	// 커서 위치 업데이트만 - 실제 이동은 Tick에서 처리
	UpdateCachedDestination();
}

void ALoAPlayerController::OnSetDestinationReleased()
{
	bHoldMoving = false;

	if (FollowTime <= ShortPressThreshold)
	{
		bAutoMoving = true;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}
	else
	{
		// 꾹 누르다 뗐을 때 즉시 정지
		if (APawn* P = GetPawn())
		{
			if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(P->GetMovementComponent()))
			{
				CMC->Velocity = FVector::ZeroVector;
				CMC->ClearAccumulatedForces();
			}
		}
	}

	FollowTime = 0.f;
}

// Triggered every frame when the input is held down
void ALoAPlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void ALoAPlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void ALoAPlayerController::UpdateCachedDestination()
{
	if (bIsTouch)
	{
		FHitResult Hit;
		if (GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit))
		{
			CachedDestination = Hit.Location;
		}
		return;
	}

	// 커서 레이를 캐릭터 높이의 수평면에 교차 - 캡슐/메시 충돌 완전 무시
	float MouseX, MouseY;
	if (!GetMousePosition(MouseX, MouseY)) return;

	FVector RayOrigin, RayDir;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, RayOrigin, RayDir)) return;

	if (FMath::Abs(RayDir.Z) > KINDA_SMALL_NUMBER)
	{
		const float GroundZ = GetPawn() ? GetPawn()->GetActorLocation().Z : 0.f;
		const float T = (GroundZ - RayOrigin.Z) / RayDir.Z;
		if (T > 0.f)
		{
			CachedDestination = RayOrigin + RayDir * T;
		}
	}
}

void ALoAPlayerController::OnDashInput()
{
	ALoACharacter* Char = GetPawn<ALoACharacter>();
	if (!Char || !Char->SkillManager) return;
	if (Char->SkillManager->IsSlotOnCooldown(USkillManagerComponent::DashSlotIndex)) return;

	FHitResult HitResult;
	FVector TargetLocation;
	if (GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		TargetLocation = HitResult.Location;
	}
	else
	{
		TargetLocation = Char->GetActorLocation() + Char->GetActorForwardVector() * 1000.f;
	}

	Char->ExecuteDash(TargetLocation);
	Char->SkillManager->TriggerCooldown(USkillManagerComponent::DashSlotIndex);
	StopMovement();
}

void ALoAPlayerController::OnSkillKeyDown(int32 SlotIndex)
{
	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyDown(SlotIndex);
	}
}

void ALoAPlayerController::OnSkillKeyHeld(int32 SlotIndex)
{
	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyHeld(SlotIndex, GetWorld()->GetDeltaSeconds());
	}
}

void ALoAPlayerController::OnSkillKeyUp(int32 SlotIndex)
{
	if (USkillManagerComponent* SM = GetSkillManager())
	{
		SM->HandleKeyUp(SlotIndex);
	}
}

USkillManagerComponent* ALoAPlayerController::GetSkillManager() const
{
	if (ALoACharacter* Char = Cast<ALoACharacter>(GetPawn()))
	{
		return Char->SkillManager;
	}
	return nullptr;
}
