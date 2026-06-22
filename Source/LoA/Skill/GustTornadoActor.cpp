#include "Skill/GustTornadoActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

AGustTornadoActor::AGustTornadoActor()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void AGustTornadoActor::BeginPlay()
{
    Super::BeginPlay();
}

void AGustTornadoActor::Activate(float InDamage, AController* InInstigator)
{
    Damage               = InDamage;
    InstigatorController = InInstigator;

    UWorld* World = GetWorld();
    if (!World) return;

    // VFX — 액터 소멸(LifeAfterDone)까지 자동 재생
    if (TornadoVFXSystem)
    {
        TornadoVFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
            TornadoVFXSystem, GetRootComponent(), NAME_None,
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset, false);
    }

    // 데미지 즉시 적용
    ApplyDamage();

    // VFX 재생 후 소멸
    SetLifeSpan(LifeAfterDone);
}

void AGustTornadoActor::ApplyDamage()
{
    UWorld* World = GetWorld();
    if (!World) return;

    APawn* InstigatorPawn = InstigatorController.IsValid()
        ? InstigatorController->GetPawn() : nullptr;

    // 전방 방향(액터 회전 기준) 박스 오버랩
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    if (InstigatorPawn) QueryParams.AddIgnoredActor(InstigatorPawn);

    // 박스 중심을 전방으로 BoxHalfLength만큼 밀어서
    // 스폰 위치(ForwardSpawnOffset)가 박스의 뒤쪽 끝이 되도록 배치
    const FVector BoxCenter = GetActorLocation()
        + GetActorForwardVector() * BoxHalfLength;

    TArray<FOverlapResult> Overlaps;
    const FCollisionShape Box = FCollisionShape::MakeBox(
        FVector(BoxHalfLength, BoxHalfWidth, BoxHalfHeight));

    World->OverlapMultiByObjectType(
        Overlaps, BoxCenter, GetActorQuat(),
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
        Box, QueryParams);

    DrawDebugBox(World, BoxCenter,
        FVector(BoxHalfLength, BoxHalfWidth, BoxHalfHeight),
        GetActorQuat(), FColor::Green, false, LifeAfterDone);

    TSet<AActor*> DamagedActors;
    for (const FOverlapResult& Hit : Overlaps)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || DamagedActors.Contains(HitActor)) continue;
        DamagedActors.Add(HitActor);
        UGameplayStatics::ApplyDamage(
            HitActor, Damage, InstigatorController.Get(),
            this, UDamageType::StaticClass());
    }
}
