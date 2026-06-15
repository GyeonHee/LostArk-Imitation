#include "BurnStatusEffect.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UBurnStatusEffect::UBurnStatusEffect()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UBurnStatusEffect::Initialize(float InDamagePerTick, float InInterval, float InDuration,
                                    AController* InInstigator, AActor* InCauser)
{
    DamagePerTick = InDamagePerTick;
    TicksRemaining = FMath::Max(1, FMath::CeilToInt(InDuration / InInterval));
    InstigatorController = InInstigator;
    CauserActor = InCauser;
    bTracksProjectile = (InCauser != nullptr);

    if (UWorld* World = GetWorld())
    {
        // 첫 틱은 InInterval 후 시작 (즉시 타격은 BlazeProjectile의 InitialDamage가 담당)
        World->GetTimerManager().SetTimer(BurnTimer, this, &UBurnStatusEffect::OnBurnTick,
            InInterval, true, InInterval);
    }
}

void UBurnStatusEffect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(BurnTimer);
}

void UBurnStatusEffect::OnBurnTick()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) { DestroyComponent(); return; }

    // 투사체(CauserActor)가 소멸했으면 화상 즉시 종료
    if (bTracksProjectile && !CauserActor.IsValid())
    {
        DestroyComponent();
        return;
    }

    UGameplayStatics::ApplyDamage(Owner, DamagePerTick,
        InstigatorController.Get(), CauserActor.Get(), UDamageType::StaticClass());

    if (--TicksRemaining <= 0)
        DestroyComponent();
}
