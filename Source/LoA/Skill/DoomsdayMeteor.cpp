#include "Skill/DoomsdayMeteor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ADoomsdayMeteor::ADoomsdayMeteor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ADoomsdayMeteor::BeginPlay()
{
    Super::BeginPlay();
}

void ADoomsdayMeteor::Activate(const FVector& InTargetLocation, float InFallDelay,
                                float InExplosionRadius, float InDamage,
                                AController* InInstigator)
{
    TargetLocation = InTargetLocation;
    InternalExplosionRadius = InExplosionRadius;
    Damage = InDamage;
    InstigatorController = InInstigator;

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
                FName("User.Scale_Circle"), FName("User.Scale_Mesh1"),
                FName("User.Scale_Ray"), FName("User.Scale_Sparks1"),
                FName("User.Scale_Sparks2") })
            {
                NC->SetFloatParameter(Param, AutoScale);
            }
        }
    }

    // CircleShowTime 후 낙하 시작 (InFallDelay는 추가 외부 딜레이)
    const float TotalDelay = InFallDelay + CircleShowTime;
    if (TotalDelay > 0.f && World)
    {
        World->GetTimerManager().SetTimer(DelayTimerHandle, this,
            &ADoomsdayMeteor::StartFalling, TotalDelay, false);
    }
    else
    {
        StartFalling();
    }

    // 디버그: 정확한 목표(빨강) + 폭발 반경(보라)
    const float TotalTime = TotalDelay + VFXTravelTime + LifeAfterLanding;
    DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 12, FColor::Red, false, TotalTime);
    DrawDebugSphere(GetWorld(), TargetLocation, InternalExplosionRadius, 16, FColor::Purple, false, TotalTime);
}

void ADoomsdayMeteor::StartFalling()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (MeteorVFXSystem)
    {
        const FVector StartLoc = TargetLocation + FVector(0.f, 0.f, SpawnHeightOffset) + VFXSpawnOffset;
        const FRotator DownRotation = FRotator(-90.f, 0.f, 0.f);
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, MeteorVFXSystem, StartLoc, DownRotation,
            FVector(VFXScale));

        DrawDebugSphere(World, StartLoc, 60.f, 12, FColor::Orange, false, VFXTravelTime + 0.5f);
        DrawDebugLine(World, StartLoc, TargetLocation, FColor::Yellow, false, VFXTravelTime + 0.5f, 0, 3.f);
    }

    // NS_Magma_Shot 재생 시간에 맞춰 데미지 — VFXTravelTime 직접 지정
    World->GetTimerManager().SetTimer(LandTimerHandle, this,
        &ADoomsdayMeteor::Land, VFXTravelTime, false);
}

void ADoomsdayMeteor::Land()
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

    BP_OnLand(TargetLocation);
    SetLifeSpan(LifeAfterLanding);
}
