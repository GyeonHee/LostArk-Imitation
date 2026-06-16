#include "Skill/SkillDoomsday.h"
#include "Skill/DoomsdayMeteor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

void USkillDoomsday::OnKeyDown(AActor* Owner)
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
        PC->ForceMoveTo(PendingCastTarget);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("[종말의 날] 사거리 초과 — 이동 중..."));
    }
}

void USkillDoomsday::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (bMovingToRange)
    {
        if (!IsValid(Owner)) return;

        const float Dist = FVector::Dist2D(Owner->GetActorLocation(), PendingCastTarget);
        if (Dist <= MaxCastRange)
        {
            bMovingToRange = false;
            CancelMovement(Owner);
            Super::OnKeyDown(Owner);  // 캐스팅 타이머 시작 (홀드 유지 필요)
        }
        return;
    }

    Super::OnKeyHeld(Owner, DeltaTime);
}

void USkillDoomsday::OnKeyUp(AActor* Owner)
{
    if (bMovingToRange)
    {
        bMovingToRange = false;
        CancelMovement(Owner);
        return;  // 쿨타임 없음 (bIsActive=false 이므로 HandleKeyUp도 미발동)
    }

    Super::OnKeyUp(Owner);  // 캐스팅 중 취소
}

void USkillDoomsday::CancelRangeMove(AActor* Owner)
{
    bMovingToRange = false;
    CancelMovement(Owner);
}

void USkillDoomsday::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !MeteorClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    const float FinalDamage = (Character ? Character->AttackPower : 100.f)
                            * SkillData.DamageCoefficient;

    SpawnMeteorAt(Owner, PendingCastTarget, FinalDamage);
}

void USkillDoomsday::ForceCancel(AActor* Owner)
{
    if (bMovingToRange)
    {
        bMovingToRange = false;
        CancelMovement(Owner);
    }
    Super::ForceCancel(Owner);
}

void USkillDoomsday::SpawnMeteorAt(AActor* Owner, const FVector& TargetPos, float Damage)
{
    UWorld* World = Owner->GetWorld();
    if (!World || !MeteorClass) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADoomsdayMeteor* Meteor = World->SpawnActor<ADoomsdayMeteor>(
        MeteorClass, TargetPos, FRotator::ZeroRotator, SpawnParams);

    if (Meteor)
    {
        AController* InstigatorCtrl = nullptr;
        if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
            InstigatorCtrl = Char->GetController();
        Meteor->Activate(TargetPos, 0.f, ExplosionRadius, Damage, InstigatorCtrl);
    }
}

void USkillDoomsday::CancelMovement(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
    {
        if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Char->GetController()))
            PC->CancelAutoMove();
    }
}
