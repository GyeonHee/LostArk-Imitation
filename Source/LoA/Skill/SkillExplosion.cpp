#include "Skill/SkillExplosion.h"
#include "Skill/ExplosionMeteorActor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

void USkillExplosion::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    FHitResult CursorHit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)) return;

    PendingCastTarget = CursorHit.Location;
    const float Dist  = FVector::Dist2D(Owner->GetActorLocation(), PendingCastTarget);

    DrawDebugCircle(Owner->GetWorld(), Owner->GetActorLocation(), MaxCastRange,
        36, FColor::Cyan, false, 3.0f, 0, 3.f, FVector(1,0,0), FVector(0,1,0));

    if (Dist <= MaxCastRange)
    {
        CancelMovement(Owner);
        Super::OnKeyDown(Owner);
    }
    else
    {
        bMovingToRange = true;
        PC->ForceMoveTo(PendingCastTarget);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("[익스플로전] 사거리 초과 — 이동 중..."));
    }
}

void USkillExplosion::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (bMovingToRange)
    {
        if (!IsValid(Owner)) return;

        const float Dist = FVector::Dist2D(Owner->GetActorLocation(), PendingCastTarget);
        if (Dist <= MaxCastRange)
        {
            bMovingToRange = false;
            CancelMovement(Owner);
            Super::OnKeyDown(Owner);
        }
        return;
    }

    Super::OnKeyHeld(Owner, DeltaTime);
}

void USkillExplosion::OnKeyUp(AActor* Owner)
{
    if (bMovingToRange)
    {
        bMovingToRange = false;
        CancelMovement(Owner);
        return;
    }

    Super::OnKeyUp(Owner);
}

void USkillExplosion::CancelRangeMove(AActor* Owner)
{
    bMovingToRange = false;
    CancelMovement(Owner);
}

void USkillExplosion::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !MeteorClass) return;

    ALoACharacter* Character    = Cast<ALoACharacter>(Owner);
    const float    FinalDamage  = (Character ? Character->AttackPower : 100.f)
                                * SkillData.DamageCoefficient;

    SpawnMeteorAt(Owner, PendingCastTarget, FinalDamage);
}

void USkillExplosion::ForceCancel(AActor* Owner)
{
    if (bMovingToRange)
    {
        bMovingToRange = false;
        CancelMovement(Owner);
    }
    Super::ForceCancel(Owner);
}

void USkillExplosion::SpawnMeteorAt(AActor* Owner, const FVector& TargetPos, float Damage)
{
    UWorld* World = Owner->GetWorld();
    if (!World || !MeteorClass) return;

    // GetActorLocation()은 캡슐 중심(허리) — VFX 날아오는 방향 기준점으로 사용
    const FVector CharWaistLoc = Owner->GetActorLocation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AExplosionMeteorActor* Meteor = World->SpawnActor<AExplosionMeteorActor>(
        MeteorClass, TargetPos, FRotator::ZeroRotator, SpawnParams);

    if (Meteor)
    {
        AController* InstigatorCtrl = Cast<ALoACharacter>(Owner)
            ? Cast<ALoACharacter>(Owner)->GetController() : nullptr;
        Meteor->Activate(CharWaistLoc, TargetPos, ExplosionRadius, Damage, InstigatorCtrl);
    }
}

void USkillExplosion::CancelMovement(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
    {
        if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Char->GetController()))
            PC->CancelAutoMove();
    }
}
