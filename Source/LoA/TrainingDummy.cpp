// Fill out your copyright notice in the Description page of Project Settings.

#include "TrainingDummy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "Stat/LoADamageLibrary.h"
#include "UI/DamageNumberActor.h"
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FClassFinder<ADamageNumberActor> DamageNumberBP(TEXT("/Game/LostArk/UI/BP_DamageNumber"));
	if (DamageNumberBP.Succeeded())
	{
		DamageNumberClass = DamageNumberBP.Class;
	}
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
	// Super 반환값은 쓰지 않는다 — 반경 데미지 거리 계산이 실패하면 0이 되는 함정(AEchidnaBoss::TakeDamage 참고)
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 보스와 같은 피격 판정 — 계산이 달라지면 허수아비로 잰 DPS가 보스전과 안 맞는다
	bool bCritical = false;
	const float FinalDamage = ULoADamageLibrary::ResolveIncomingHit(DamageAmount, EventInstigator, false, bCritical);
	ULoADamageLibrary::ReportDamageDealt(EventInstigator, this, DamageCauser, FinalDamage, bCritical, true);
	ULoADamageLibrary::NotifyIdentityHit(EventInstigator, DamageCauser, DamageAmount);
	SpawnDamageNumber(FinalDamage, bCritical);

	if (!bInfiniteHP)
	{
		HP = FMath::Max(HP - FinalDamage, 0.f);
		OnHPChanged.Broadcast(HP, MaxHP);
	}
	return FinalDamage;
}

void ATrainingDummy::SpawnDamageNumber(float DamageAmount, bool bCritical)
{
	UWorld* World = GetWorld();
	if (!World || !DamageNumberClass || DamageAmount < 0.5f) return;

	FVector SpawnLocation = GetActorLocation() + FVector(0.f, 0.f, DamageNumberHeight);
	SpawnLocation += FVector(
		FMath::FRandRange(-DamageNumberJitter, DamageNumberJitter),
		FMath::FRandRange(-DamageNumberJitter, DamageNumberJitter),
		FMath::FRandRange(-DamageNumberJitter * 0.5f, DamageNumberJitter * 0.5f));

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADamageNumberActor* Number = World->SpawnActor<ADamageNumberActor>(DamageNumberClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
	{
		// 보스와 같은 표기 — 일반 흰색, 치명타 노랗고 크게. 나중에 맞은 숫자일수록 앞
		const FText Text = FText::FromString(FString::FormatAsNumber(FMath::RoundToInt(DamageAmount)));
		Number->ActivateLabel(Text,
			bCritical ? CritDamageNumberColor : NormalDamageNumberColor,
			bCritical ? CritDamageNumberScale : 1.f,
			++DamageNumberCounter);
	}
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
