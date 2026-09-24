#include "Skill/GustTornadoActor.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Raid/CounterableInterface.h"

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

void AGustTornadoActor::Activate(float InDamage, AController* InInstigator, bool bInCanCounter)
{
    Damage               = InDamage;
    bCanCounter          = bInCanCounter;
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

    // 카운터 판정은 데미지보다 먼저, 그리고 액터가 아니라 **컴포넌트 단위** — 거울 벽은 한 액터에 거울 7개가 있고
    // 그중 파란 거울 하나만 카운터 대상이라 어느 컴포넌트를 맞았는지가 중요하다.
    // 정면 판정은 토네이도 위치가 아니라 "시전자(플레이어) 위치" 기준 (로아의 헤드어택과 동일)
    if (bCanCounter && InstigatorPawn)
    {
        TSet<AActor*> CounteredActors;
        for (const FOverlapResult& Hit : Overlaps)
        {
            AActor* HitActor = Hit.GetActor();
            ICounterable* Counterable = Cast<ICounterable>(HitActor);
            if (!Counterable || CounteredActors.Contains(HitActor)) continue;

            if (Counterable->TryCounterHit(InstigatorPawn, Hit.GetComponent()))
            {
                CounteredActors.Add(HitActor);
            }
        }
    }

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
