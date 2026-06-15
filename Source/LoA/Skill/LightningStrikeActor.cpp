#include "Skill/LightningStrikeActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ALightningStrikeActor::ALightningStrikeActor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ALightningStrikeActor::BeginPlay()
{
    Super::BeginPlay();
}

void ALightningStrikeActor::Activate(const FVector& InTargetLocation, float InExplosionRadius,
                                      float InDamage, AController* InInstigator)
{
    TargetLocation         = InTargetLocation;
    InternalExplosionRadius = InExplosionRadius;
    Damage                 = InDamage;
    InstigatorController   = InInstigator;

    SetActorLocation(TargetLocation);

    UWorld* World = GetWorld();

    // 범위 원 VFX 즉시 표시 — 이미터별 스케일 파라미터 일괄 설정
    if (CircleVFXSystem && World)
    {
        if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, CircleVFXSystem, TargetLocation, FRotator::ZeroRotator, FVector(1.f)))
        {
            const float AutoScale = (CircleBaseRadius > 0.f)
                ? InternalExplosionRadius / CircleBaseRadius : 1.f;
            for (const FName& Param : {
                FName("User.Scale_Circle"), FName("User.Scale_Smoke"),
                FName("User.Scale_Sparks1"), FName("User.Scale_Spiral1") })
            {
                NC->SetFloatParameter(Param, AutoScale);
            }
        }
    }

    const float TotalTime = CircleShowTime + StrikeDelay + LifeAfterStrike;
    DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 12, FColor::Yellow, false, TotalTime);
    DrawDebugSphere(GetWorld(), TargetLocation, InternalExplosionRadius, 16, FColor::Cyan, false, TotalTime);

    // CircleShowTime 후 번개 VFX + 데미지 타이머 시작
    if (World)
    {
        World->GetTimerManager().SetTimer(
            StrikeTimerHandle, this, &ALightningStrikeActor::Strike, CircleShowTime, false);
    }
}

void ALightningStrikeActor::Strike()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 번개 VFX 재생
    if (LightningVFXSystem)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, LightningVFXSystem, TargetLocation,
            FRotator::ZeroRotator, FVector(VFXScale));
    }

    BP_OnStrike(TargetLocation);

    // StrikeDelay 후 데미지 적용
    World->GetTimerManager().SetTimer(
        DamageTimerHandle, this, &ALightningStrikeActor::ApplyDamage, StrikeDelay, false);
}

void ALightningStrikeActor::ApplyDamage()
{
    UWorld* World = GetWorld();
    if (!World) return;

    APawn* InstigatorPawn = InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr;
    TArray<AActor*> IgnoreActors;
    if (InstigatorPawn) IgnoreActors.Add(InstigatorPawn);

    UGameplayStatics::ApplyRadialDamage(
        World, Damage, TargetLocation, InternalExplosionRadius,
        UDamageType::StaticClass(), IgnoreActors,
        this, InstigatorController.Get(), true);

    SetLifeSpan(LifeAfterStrike);
}
