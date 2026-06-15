#include "BlazeProjectile.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ABlazeProjectile::ABlazeProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    // 루트는 빈 SceneComponent로 고정 — 액터 위치가 절대 바뀌지 않음
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // CollisionBox는 루트의 자식 → SetRelativeLocation이 액터 기준 상대 좌표로 동작
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(Root);
    CollisionBox->SetBoxExtent(FVector(1.f, 80.f, 80.f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

    ZoneVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ZoneVFX"));
    ZoneVFX->SetupAttachment(Root); // CollisionBox가 아닌 Root에 부착 — 위치/스케일 독립 제어
    ZoneVFX->SetAutoActivate(true);
}

void ABlazeProjectile::BeginPlay()
{
    Super::BeginPlay();
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ABlazeProjectile::OnBoxBeginOverlap);
    CollisionBox->OnComponentEndOverlap.AddDynamic(this, &ABlazeProjectile::OnBoxEndOverlap);
}

void ABlazeProjectile::Activate(const FVector& InBoxExtent, float InTickDamage,
                                 float InTickInterval, float InDuration, AController* InInstigator)
{
    TargetHalfExtentX = InBoxExtent.X;
    TargetHalfExtentY = InBoxExtent.Y;
    TargetHalfExtentZ = InBoxExtent.Z;
    TickDamage = InTickDamage;
    InstigatorController = InInstigator;

    // 스프레드 시작: X는 거의 0에서 출발
    CollisionBox->SetBoxExtent(FVector(1.f, TargetHalfExtentY, TargetHalfExtentZ));
    CollisionBox->SetRelativeLocation(FVector::ZeroVector);

    if (ZoneVFX)
        ZoneVFX->SetRelativeLocation(FVector::ZeroVector);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(DamageTimer, this, &ABlazeProjectile::OnDamageTick,
            InTickInterval, true, InTickInterval);
    }

    SetLifeSpan(InDuration);
}

void ABlazeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(DamageTimer);
}

void ABlazeProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (bSpreadComplete) return;

    SpreadElapsed += DeltaTime;
    const float Alpha = FMath::Clamp(SpreadElapsed / SpreadDuration, 0.f, 1.f);
    const float CurrentExtentX = FMath::Max(TargetHalfExtentX * Alpha, 1.f);

    // 뒷면을 액터 기준점(캐릭터 앞)에 고정하고 앞면만 전진
    // 박스 중심을 CurrentExtentX만큼 앞으로 이동 → 뒷면은 항상 원점
    CollisionBox->SetBoxExtent(FVector(CurrentExtentX, TargetHalfExtentY, TargetHalfExtentZ));
    CollisionBox->SetRelativeLocation(FVector(CurrentExtentX, 0.f, 0.f));

    // VFX를 콜리전 박스 앞면(front face) 위치로 이동 — 스케일 없이 위치만 변경
    if (ZoneVFX)
        ZoneVFX->SetRelativeLocation(FVector(CurrentExtentX * 2.f, 0.f, 0.f));

    // OnBoxBeginOverlap은 박스가 커질 때 보장되지 않으므로 수동으로 체크
    TArray<AActor*> Overlapping;
    CollisionBox->GetOverlappingActors(Overlapping);
    for (AActor* Actor : Overlapping)
    {
        if (!IsValidTarget(Actor) || CurrentActors.Contains(Actor)) continue;
        CurrentActors.Add(Actor);
        ApplyInitialHit(Actor);
    }

    if (Alpha >= 1.f)
    {
        bSpreadComplete = true;
        SetActorTickEnabled(false);

        if (ZoneVFX)
            ZoneVFX->SetRelativeLocation(FVector(TargetHalfExtentX * 2.f, 0.f, 0.f));
    }
}

bool ABlazeProjectile::IsValidTarget(AActor* Target) const
{
    if (!IsValid(Target) || Target == this) return false;
    if (InstigatorController.IsValid() && Target == InstigatorController->GetPawn()) return false;
    return true;
}

void ABlazeProjectile::ApplyInitialHit(AActor* Target)
{
    UGameplayStatics::ApplyDamage(Target, TickDamage,
        InstigatorController.Get(), this, UDamageType::StaticClass());

    if (HitVFX)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitVFX, Target->GetActorLocation());

    BP_OnHit(Target, Target->GetActorLocation());
}

void ABlazeProjectile::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 스프레드 완료 후 새로 걸어 들어온 적 처리
    if (!IsValidTarget(OtherActor) || CurrentActors.Contains(OtherActor)) return;
    CurrentActors.Add(OtherActor);
    ApplyInitialHit(OtherActor);
}

void ABlazeProjectile::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    CurrentActors.Remove(OtherActor);
}

void ABlazeProjectile::OnDamageTick()
{
    for (auto It = CurrentActors.CreateIterator(); It; ++It)
    {
        AActor* Target = *It;
        if (!IsValid(Target)) { It.RemoveCurrent(); continue; }

        UGameplayStatics::ApplyDamage(Target, TickDamage,
            InstigatorController.Get(), this, UDamageType::StaticClass());

        if (HitVFX)
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitVFX, Target->GetActorLocation());

        BP_OnHit(Target, Target->GetActorLocation());
    }
}
