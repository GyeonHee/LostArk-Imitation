#include "Skill/SkillCheonbeol.h"
#include "Skill/LightningStrikeActor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

void USkillCheonbeol::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    FHitResult CursorHit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)) return;

    PendingCastTarget = CursorHit.Location;
    const float Dist = FVector::Dist2D(Owner->GetActorLocation(), PendingCastTarget);

    DrawDebugCircle(Owner->GetWorld(), Owner->GetActorLocation(), MaxCastRange,
        36, FColor::Cyan, false, 3.0f, 0, 3.f, FVector(1,0,0), FVector(0,1,0));

    if (Dist <= MaxCastRange)
    {
        CancelMovement(Owner);  // 캐스팅 시작 시 이동 멈춤
        Super::OnKeyDown(Owner);
    }
    else
    {
        bMovingToRange = true;
        bIsActive = true;
        PC->ForceMoveTo(PendingCastTarget);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("[천벌] 사거리 초과 — 이동 중..."));
    }
}

void USkillCheonbeol::OnKeyHeld(AActor* Owner, float DeltaTime)
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

void USkillCheonbeol::OnKeyUp(AActor* Owner)
{
    if (bMovingToRange)
    {
        bMovingToRange = false;
        bIsActive = false;
        CancelMovement(Owner);
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[천벌] 이동 취소"));
        return;
    }

    Super::OnKeyUp(Owner);
}

void USkillCheonbeol::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !LightningClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    const float FinalDamage = (Character ? Character->AttackPower : 100.f)
                            * SkillData.DamageCoefficient;

    SpawnLightningAt(Owner, PendingCastTarget, FinalDamage);
}

void USkillCheonbeol::ForceCancel(AActor* Owner)
{
    if (bMovingToRange)
    {
        bMovingToRange = false;
        CancelMovement(Owner);
    }
    Super::ForceCancel(Owner);
}

void USkillCheonbeol::SpawnLightningAt(AActor* Owner, const FVector& TargetPos, float Damage)
{
    UWorld* World = Owner->GetWorld();
    if (!World || !LightningClass) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ALightningStrikeActor* Lightning = World->SpawnActor<ALightningStrikeActor>(
        LightningClass, TargetPos, FRotator::ZeroRotator, SpawnParams);

    if (Lightning)
    {
        AController* InstigatorCtrl = nullptr;
        if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
            InstigatorCtrl = Char->GetController();
        Lightning->Activate(TargetPos, ExplosionRadius, Damage, InstigatorCtrl);
    }
}

void USkillCheonbeol::CancelMovement(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
    {
        if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Char->GetController()))
            PC->CancelAutoMove();
    }
}
