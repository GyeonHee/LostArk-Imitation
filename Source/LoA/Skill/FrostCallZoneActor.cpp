#include "Skill/FrostCallZoneActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AFrostCallZoneActor::AFrostCallZoneActor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void AFrostCallZoneActor::BeginPlay()
{
    Super::BeginPlay();
}

void AFrostCallZoneActor::Activate(float InTickDamage, AController* InInstigator)
{
    TickDamage           = InTickDamage;
    InstigatorController = InInstigator;
    CurrentDamageTick    = 0;
    bStopped             = false;

    UWorld* World = GetWorld();
    if (!World) return;

    // 루핑 장판 원 VFX
    if (CircleVFXSystem)
    {
        CircleVFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
            CircleVFXSystem, GetRootComponent(), NAME_None,
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset, false);

        if (CircleVFXComp && CircleBaseRadius > 0.f)
        {
            const float AutoScale = ZoneRadius / CircleBaseRadius;
            for (const FName& Param : {
                FName("User.Scale_Circle"), FName("User.Scale_Mesh1"),
                FName("User.Scale_Ray"),    FName("User.Scale_Sparks1"),
                FName("User.Scale_Sparks2")})
            {
                CircleVFXComp->SetFloatParameter(Param, AutoScale);
            }
        }
    }

    DrawDebugCylinder(World, GetActorLocation(), GetActorLocation() + FVector(0, 0, 10),
        ZoneRadius, 32, FColor::Cyan, false,
        (MaxDamageTicks - 1) * DamageTickInterval + LifeAfterStop);

    // 데미지: 즉시 1회 + 반복
    ApplyDamageTick();
    if (!bStopped)
    {
        World->GetTimerManager().SetTimer(
            DamageTimerHandle, this,
            &AFrostCallZoneActor::ApplyDamageTick,
            DamageTickInterval, true);
    }

    // VFX: 즉시 1회 + 반복 (데미지와 독립)
    SpawnIcicleVFX();
    if (VFXTickInterval > 0.f)
    {
        World->GetTimerManager().SetTimer(
            VFXTimerHandle, this,
            &AFrostCallZoneActor::SpawnIcicleVFX,
            VFXTickInterval, true);
    }
}

void AFrostCallZoneActor::StopZone()
{
    if (bStopped) return;
    bStopped = true;

    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(DamageTimerHandle);
        World->GetTimerManager().ClearTimer(VFXTimerHandle);
    }

    if (CircleVFXComp)
    {
        CircleVFXComp->DeactivateImmediate();
        CircleVFXComp = nullptr;
    }

    SetLifeSpan(LifeAfterStop);
}

void AFrostCallZoneActor::ApplyDamageTick()
{
    if (bStopped) return;

    CurrentDamageTick++;

    UWorld* World = GetWorld();
    if (!World) return;

    APawn* InstigatorPawn = InstigatorController.IsValid()
        ? InstigatorController->GetPawn() : nullptr;
    TArray<AActor*> IgnoreActors;
    if (InstigatorPawn) IgnoreActors.Add(InstigatorPawn);

    UGameplayStatics::ApplyRadialDamage(
        World, TickDamage, GetActorLocation(), ZoneRadius,
        UDamageType::StaticClass(), IgnoreActors,
        this, InstigatorController.Get(), true);

    if (CurrentDamageTick >= MaxDamageTicks)
        StopZone();
}

void AFrostCallZoneActor::SpawnIcicleVFX()
{
    if (bStopped || !IcicleVFXSystem) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Loc = GetActorLocation();
    for (int32 i = 0; i < IciclesPerTick; i++)
    {
        const FVector2D RandPoint = FMath::RandPointInCircle(ZoneRadius);
        const FVector SpawnLoc = Loc + FVector(RandPoint.X, RandPoint.Y, IcicleSpawnHeight);
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, IcicleVFXSystem, SpawnLoc);
    }
}
