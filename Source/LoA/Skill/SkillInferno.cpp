#include "Skill/SkillInferno.h"
#include "Skill/InfernoZoneActor.h"
#include "LoACharacter.h"
#include "Stat/LoADamageLibrary.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"

void USkillInferno::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    // 바닥을 못 맞히면(맵 밖·하늘) 조준 방향 사거리 끝으로 — 예전엔 그냥 return해서 스킬이 씹혔다
    if (!PC->GetCursorGroundTarget(Owner, MaxCastRange, PendingTarget)) return;
    const float Dist = FVector::Dist2D(Owner->GetActorLocation(), PendingTarget);

    if (Dist <= MaxCastRange)
    {
        Execute(Owner);
    }
    else
    {
        bMovingToRange = true;
        PC->ForceMoveTo(PendingTarget);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("[인페르노] 사거리 초과 — 이동 중..."));
    }
}

void USkillInferno::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (!bMovingToRange) return;
    if (!IsValid(Owner)) return;

    const float Dist = FVector::Dist2D(Owner->GetActorLocation(), PendingTarget);
    if (Dist <= MaxCastRange)
    {
        bMovingToRange = false;
        CancelMovement(Owner);
        Execute(Owner);
    }
}

void USkillInferno::OnKeyUp(AActor* Owner)
{
    // 자동이동 중 키 해제는 무시 — 틱이 계속 처리
}

void USkillInferno::CancelRangeMove(AActor* Owner)
{
    bMovingToRange = false;
    CancelMovement(Owner);
}

void USkillInferno::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !ZoneClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    FVector TargetLoc = PendingTarget;
    if (TargetLoc.IsZero() && !PC->GetCursorGroundTarget(Owner, MaxCastRange, TargetLoc)) return;

    FVector OwnerLoc   = Owner->GetActorLocation();
    FVector Horizontal = TargetLoc - OwnerLoc;
    Horizontal.Z = 0.f;
    if (Horizontal.Size() > MaxCastRange)
    {
        const float SavedZ = TargetLoc.Z;
        TargetLoc   = OwnerLoc + Horizontal.GetSafeNormal() * MaxCastRange;
        TargetLoc.Z = SavedZ;
    }

    UWorld* World = Owner->GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AInfernoZoneActor* Zone = World->SpawnActor<AInfernoZoneActor>(
        ZoneClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);
    // 데미지 미터가 이 액터의 피해를 어느 스킬로 집계할지 알 수 있게
    ULoADamageLibrary::TagDamageSource(Zone, SkillRowName);

    if (Zone)
    {
        const float FinalDamage = ULoADamageLibrary::CalcOutgoingDamage(Character, SkillData.DamageCoefficient, SkillRowName);
        Zone->Activate(FinalDamage, Character ? Character->GetController() : nullptr);
    }

    PendingTarget = FVector::ZeroVector;
}

void USkillInferno::CancelMovement(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
    {
        if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Char->GetController()))
            PC->CancelAutoMove();
    }
}
