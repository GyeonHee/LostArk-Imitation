// Fill out your copyright notice in the Description page of Project Settings.

#include "TrainingDummy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"

ATrainingDummy::ATrainingDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(40.f, 90.f);
	SetRootComponent(Capsule);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);

	HPWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPWidget"));
	HPWidget->SetupAttachment(Capsule);
	HPWidget->SetWidgetSpace(EWidgetSpace::World);
	HPWidget->SetDrawSize(FVector2D(200.f, 40.f));
	HPWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HPWidget->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

void ATrainingDummy::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	// 3초마다 MaxHP의 5% 자동 회복
	GetWorldTimerManager().SetTimer(RegenTimerHandle, this, &ATrainingDummy::RegenTick, 3.f, true);
}

float ATrainingDummy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	HP = FMath::Max(HP - DamageAmount, 0.f);
	OnHPChanged.Broadcast(HP, MaxHP);
	return DamageAmount;
}

void ATrainingDummy::ResetHP()
{
	HP = MaxHP;
	OnHPChanged.Broadcast(HP, MaxHP);
}

void ATrainingDummy::RegenTick()
{
	if (HP < MaxHP)
	{
		HP = FMath::Min(HP + MaxHP * 0.05f, MaxHP);
		OnHPChanged.Broadcast(HP, MaxHP);
	}
}
