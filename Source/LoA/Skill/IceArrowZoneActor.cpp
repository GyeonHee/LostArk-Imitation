#include "Skill/IceArrowZoneActor.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AIceArrowZoneActor::AIceArrowZoneActor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void AIceArrowZoneActor::BeginPlay()
{
    Super::BeginPlay();
}

void AIceArrowZoneActor::Activate(float InTickDamage, AController* InInstigator, FRotator InIcicleRotation)
{
    TickDamage           = InTickDamage;
    InstigatorController = InInstigator;
    IcicleRotation       = InIcicleRotation;
    CurrentTick          = 0;

    UWorld* World = GetWorld();
    if (!World) return;

    DrawDebugCylinder(World, GetActorLocation(), GetActorLocation() + FVector(0, 0, 10),
        ZoneRadius, 32, FColor::Blue, false,
        (MaxTicks - 1) * TickInterval + LifeAfterDone);

    // 첫 틱 즉시
    ApplyTick();

    if (CurrentTick < MaxTicks)
    {
        World->GetTimerManager().SetTimer(
            TickTimerHandle, this,
            &AIceArrowZoneActor::ApplyTick,
            TickInterval, true);
    }
}

void AIceArrowZoneActor::ApplyTick()
{
    CurrentTick++;

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Loc = GetActorLocation();

    // 고드름 VFX
    // IcicleHorizontalDrift: Niagara 수평 드리프트 상쇄용 오프셋
    // 목표 방향(IcicleRotation 수평)으로 미리 밀어두면, 드리프트 후 착지점이 원 안에 들어옴
    if (IcicleVFXSystem)
    {
        const FVector RotHorizDir = FVector(
            IcicleRotation.Vector().X, IcicleRotation.Vector().Y, 0.f).GetSafeNormal();

        for (int32 i = 0; i < IciclesPerTick; i++)
        {
            const FVector2D RandXY = FMath::RandPointInCircle(ZoneRadius);
            const FVector SpawnLoc = Loc
                + FVector(RandXY.X, RandXY.Y, IcicleSpawnHeight)
                + RotHorizDir * IcicleHorizontalDrift;
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                World, IcicleVFXSystem, SpawnLoc, IcicleRotation);
        }
    }

    // 범위 데미지
    APawn* InstigatorPawn = InstigatorController.IsValid()
        ? InstigatorController->GetPawn() : nullptr;
    TArray<AActor*> IgnoreActors;
    if (InstigatorPawn) IgnoreActors.Add(InstigatorPawn);

    UGameplayStatics::ApplyRadialDamage(
        World, TickDamage, Loc, ZoneRadius,
        UDamageType::StaticClass(), IgnoreActors,
        this, InstigatorController.Get(), true);

    if (CurrentTick >= MaxTicks)
    {
        World->GetTimerManager().ClearTimer(TickTimerHandle);
        SetLifeSpan(LifeAfterDone);
    }
}
