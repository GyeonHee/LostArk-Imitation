#include "Skill/SkillIceArrow.h"
#include "Skill/IceArrowZoneActor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

void USkillIceArrow::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    FHitResult CursorHit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)) return;

    PendingTarget = CursorHit.Location;
    const float Dist = FVector::Dist2D(Owner->GetActorLocation(), PendingTarget);

    DrawDebugCircle(Owner->GetWorld(), Owner->GetActorLocation(), MaxCastRange,
        36, FColor::Blue, false, 3.f, 0, 3.f, FVector(1, 0, 0), FVector(0, 1, 0));

    if (Dist <= MaxCastRange)
    {
        Execute(Owner);
    }
    else
    {
        bMovingToRange = true;
        PC->ForceMoveTo(PendingTarget);
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
            TEXT("[아이스 에로우] 사거리 초과 — 이동 중..."));
    }
}

void USkillIceArrow::OnKeyHeld(AActor* Owner, float DeltaTime)
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

void USkillIceArrow::OnKeyUp(AActor* Owner)
{
    // 자동이동 중 키 해제는 무시 — 틱이 계속 처리
}

void USkillIceArrow::CancelRangeMove(AActor* Owner)
{
    bMovingToRange = false;
    CancelMovement(Owner);
}

void USkillIceArrow::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !ZoneClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    // 목표 위치 확정
    FVector TargetLoc = PendingTarget;
    if (TargetLoc.IsZero())
    {
        FHitResult H;
        if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, H)) return;
        TargetLoc = H.Location;
    }

    // 사거리 클램프
    FVector OwnerLoc   = Owner->GetActorLocation();
    FVector Horizontal = TargetLoc - OwnerLoc;
    Horizontal.Z = 0.f;
    if (Horizontal.Size() > MaxCastRange)
    {
        const float SavedZ = TargetLoc.Z;
        TargetLoc   = OwnerLoc + Horizontal.GetSafeNormal() * MaxCastRange;
        TargetLoc.Z = SavedZ;
    }

    // 캐릭터→커서 수평 방향에서 45도 하향 각도 계산
    FVector HorizDir = (TargetLoc - OwnerLoc);
    HorizDir.Z = 0.f;
    HorizDir = HorizDir.GetSafeNormal();
    // 수평 + 수직 동일 크기 → 정확히 45도
    const FRotator IcicleRot = (-HorizDir + FVector(0.f, 0.f, -1.f)).GetSafeNormal().Rotation();

    UWorld* World = Owner->GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AIceArrowZoneActor* Zone = World->SpawnActor<AIceArrowZoneActor>(
        ZoneClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);

    if (Zone)
    {
        // 총 데미지를 MaxTicks로 나눠서 틱당 데미지 계산
        const float TotalDamage = (Character ? Character->AttackPower : 100.f)
                                * SkillData.DamageCoefficient;
        const AIceArrowZoneActor* ZoneCDO = ZoneClass->GetDefaultObject<AIceArrowZoneActor>();
        const int32 Ticks = ZoneCDO ? ZoneCDO->MaxTicks : 4;
        const float TickDmg = TotalDamage / FMath::Max(1, Ticks);

        Zone->Activate(TickDmg, Character ? Character->GetController() : nullptr, IcicleRot);

        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue,
            FString::Printf(TEXT("[아이스 에로우] 발동 — 틱당 %.0f 데미지 × %d회"), TickDmg, Ticks));
    }

    PendingTarget = FVector::ZeroVector;
}

void USkillIceArrow::CancelMovement(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    if (ALoACharacter* Char = Cast<ALoACharacter>(Owner))
        if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(Char->GetController()))
            PC->CancelAutoMove();
}
