#include "Skill/ExplosionMeteorActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AExplosionMeteorActor::AExplosionMeteorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    MeteorVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MeteorVFX"));
    MeteorVFXComponent->SetupAttachment(RootComponent);
    MeteorVFXComponent->SetAutoActivate(false);
}

void AExplosionMeteorActor::BeginPlay()
{
    Super::BeginPlay();
}

void AExplosionMeteorActor::Activate(const FVector& InCharWaistLoc, const FVector& InTargetLoc,
                                      float InExplosionRadius, float InDamage, AController* InInstigator)
{
    TargetLocation          = InTargetLoc;
    InternalExplosionRadius = InExplosionRadius;
    Damage                  = InDamage;
    InstigatorController    = InInstigator;

    // 스폰 위치: 캐릭터 허리에서 커서 방향으로 SpawnForwardOffset 앞
    FVector HorizDir = (InTargetLoc - InCharWaistLoc);
    HorizDir.Z = 0.f;
    HorizDir.Normalize();

    const FVector SpawnLoc = InCharWaistLoc
        + HorizDir * SpawnForwardOffset
        + FVector(0.f, 0.f, SpawnZOffset);

    SetActorLocation(SpawnLoc);
    // 액터 회전을 커서 방향으로 — 부착된 VFX 방향 기준
    SetActorRotation((TargetLocation - SpawnLoc).GetSafeNormal().Rotation());

    UWorld* World = GetWorld();

    // 발판 원 VFX — 이미터별 스케일 파라미터
    if (CircleVFXSystem && World)
    {
        if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, CircleVFXSystem, TargetLocation, FRotator::ZeroRotator, FVector(1.f)))
        {
            const float AutoScale = (CircleBaseRadius > 0.f)
                ? InternalExplosionRadius / CircleBaseRadius : 1.f;
            for (const FName& Param : {
                FName("User.Scale_Circle"), FName("User.Scale_Mesh1"),
                FName("User.Scale_Ray"),    FName("User.Scale_Sparks1"),
                FName("User.Scale_Sparks2") })
            {
                NC->SetFloatParameter(Param, AutoScale);
            }
        }
    }

    if (CircleShowTime > 0.f && World)
    {
        World->GetTimerManager().SetTimer(CircleTimerHandle, this,
            &AExplosionMeteorActor::StartFlying, CircleShowTime, false);
    }
    else
    {
        StartFlying();
    }

    // 디버그
    const float EstTravel = (MeteorSpeed > 0.f) ? FVector::Dist(SpawnLoc, TargetLocation) / MeteorSpeed : 1.f;
    const float TotalTime = CircleShowTime + EstTravel + LifeAfterLanding;
    DrawDebugSphere(World, TargetLocation, 50.f, 12, FColor::Red, false, TotalTime);
    DrawDebugSphere(World, TargetLocation, InternalExplosionRadius, 16, FColor::Purple, false, TotalTime);
    DrawDebugSphere(World, SpawnLoc, 40.f, 12, FColor::Orange, false, CircleShowTime + 0.5f);
    DrawDebugLine(World, SpawnLoc, TargetLocation, FColor::Yellow, false, TotalTime, 0, 3.f);
}

void AExplosionMeteorActor::StartFlying()
{
    if (MeteorVFXSystem && MeteorVFXComponent)
    {
        MeteorVFXComponent->SetAsset(MeteorVFXSystem);
        MeteorVFXComponent->SetRelativeScale3D(FVector(VFXScale));
        MeteorVFXComponent->Activate(true);
    }

    bFlying = true;
    SetActorTickEnabled(true);
}

void AExplosionMeteorActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bFlying || bLanded) return;

    const FVector Current   = GetActorLocation();
    const FVector ToTarget  = TargetLocation - Current;
    const float   DistSq    = ToTarget.SizeSquared();

    if (DistSq <= ArrivalThreshold * ArrivalThreshold)
    {
        Land();
        return;
    }

    const float MoveAmount = MeteorSpeed * DeltaTime;
    SetActorLocation(Current + ToTarget.GetSafeNormal() * MoveAmount);
}

void AExplosionMeteorActor::Land()
{
    if (bLanded) return;
    bLanded = true;
    bFlying = false;
    SetActorTickEnabled(false);

    // 비행 VFX 즉시 종료
    if (MeteorVFXComponent) MeteorVFXComponent->DeactivateImmediate();

    UWorld* World = GetWorld();
    if (!World) return;

    // 폭발 VFX를 정확한 커서 위치에 별도 스폰
    if (ExplosionVFXSystem)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World, ExplosionVFXSystem, TargetLocation,
            FRotator(-90.f, 0.f, 0.f), FVector(VFXScale));
    }

    APawn* InstigatorPawn = InstigatorController.IsValid()
        ? InstigatorController->GetPawn() : nullptr;
    TArray<AActor*> IgnoreActors;
    if (InstigatorPawn) IgnoreActors.Add(InstigatorPawn);

    UGameplayStatics::ApplyRadialDamage(
        World, Damage, TargetLocation, InternalExplosionRadius,
        UDamageType::StaticClass(), IgnoreActors,
        this, InstigatorController.Get(), true);

    BP_OnLand(TargetLocation);
    SetLifeSpan(LifeAfterLanding);
}
