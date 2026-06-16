#include "Skill/InfernoZoneActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AInfernoZoneActor::AInfernoZoneActor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void AInfernoZoneActor::BeginPlay()
{
    Super::BeginPlay();
}

void AInfernoZoneActor::Activate(float InDamage, AController* InInstigator)
{
    Damage               = InDamage;
    InternalZoneRadius   = ZoneRadius;
    InstigatorController = InInstigator;

    UWorld* World = GetWorld();
    if (!World) return;

    // 장판 원 VFX 즉시 표시
    if (CircleVFXSystem)
    {
        if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, CircleVFXSystem, GetActorLocation(), FRotator::ZeroRotator, FVector(1.f)))
        {
            const float AutoScale = (CircleBaseRadius > 0.f)
                ? InternalZoneRadius / CircleBaseRadius : 1.f;
            for (const FName& Param : {
                FName("User.Scale_Circle"), FName("User.Scale_Mesh1"),
                FName("User.Scale_Ray"),    FName("User.Scale_Sparks1"),
                FName("User.Scale_Sparks2") })
            {
                NC->SetFloatParameter(Param, AutoScale);
            }
        }
    }

    // CircleShowTime 후 폭발
    World->GetTimerManager().SetTimer(
        ExplodeTimerHandle, this,
        &AInfernoZoneActor::Explode,
        CircleShowTime, false);

    const float TotalTime = CircleShowTime + LifeAfterExplosion;
    DrawDebugCylinder(World, GetActorLocation(), GetActorLocation() + FVector(0,0,10),
        InternalZoneRadius, 32, FColor::Orange, false, TotalTime);
}

void AInfernoZoneActor::Explode()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FVector ExplodePos = GetActorLocation();

    // 위로 솟구치는 폭발 VFX 1회 스폰
    if (ExplosionVFXSystem)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, ExplosionVFXSystem, ExplodePos,
            FRotator::ZeroRotator, FVector(ExplosionVFXScale));
    }

    // 범위 데미지
    APawn* InstigatorPawn = InstigatorController.IsValid()
        ? InstigatorController->GetPawn() : nullptr;
    TArray<AActor*> IgnoreActors;
    if (InstigatorPawn) IgnoreActors.Add(InstigatorPawn);

    UGameplayStatics::ApplyRadialDamage(
        World, Damage, ExplodePos, InternalZoneRadius,
        UDamageType::StaticClass(), IgnoreActors,
        this, InstigatorController.Get(), true);

    BP_OnExplode(ExplodePos);
    SetLifeSpan(LifeAfterExplosion);
}
