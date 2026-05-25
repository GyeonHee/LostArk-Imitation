// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoACharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"

ALoACharacter::ALoACharacter()
{
	HP = 100000.f;
	MaxHP = 100000.f;
	MP = 5000.f;
	MaxMP = 5000.f;

	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	GetCharacterMovement()->MaxAcceleration = 99999.0f;

	// Create the skill manager component
	SkillManager = CreateDefaultSubobject<USkillManagerComponent>(TEXT("SkillManager"));

	// Create the camera boom component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	// Create the camera component
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));

	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ALoACharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (CharacterData)
	{
		MaxHP       = CharacterData->MaxHP;
		HP          = MaxHP;
		MaxMP       = CharacterData->MaxMP;
		MP          = MaxMP;
		MPRegenRate = CharacterData->MPRegenRate;
		AttackPower = CharacterData->AttackPower;
		GetCharacterMovement()->MaxWalkSpeed = CharacterData->MoveSpeed;
	}
}

void ALoACharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxAcceleration = 99999.0f;
}

void ALoACharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	MPRegenAccum += DeltaSeconds;
	if (MPRegenAccum >= 1.f)
	{
		RestoreMP(MaxMP * MPRegenRate);
		MPRegenAccum -= 1.f;
	}
}

void ALoACharacter::ExecuteDash_Implementation(const FVector& TargetLocation)
{
	const FVector Dir = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (!SkillManager || Dir.IsNearlyZero()) return;

	const FSkillData DashData = SkillManager->GetSlotSkillData(USkillManagerComponent::DashSlotIndex);
	LaunchCharacter(Dir * DashData.DashImpulse, true, false);
}

float ALoACharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ReceiveDamage(DamageAmount);
	return DamageAmount;
}

void ALoACharacter::ReceiveDamage(float DamageAmount)
{
	HP = FMath::Max(HP - DamageAmount, 0.f);
	OnHPChanged.Broadcast(HP);
}

bool ALoACharacter::ConsumeMP(float Amount)
{
	if (MP < Amount) return false;
	MP = FMath::Max(MP - Amount, 0.f);
	OnMPChanged.Broadcast(MP);
	return true;
}

void ALoACharacter::RestoreMP(float Amount)
{
	MP = FMath::Min(MP + Amount, MaxMP);
	OnMPChanged.Broadcast(MP);
}
