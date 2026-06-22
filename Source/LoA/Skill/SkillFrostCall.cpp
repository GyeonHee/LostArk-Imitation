#include "Skill/SkillFrostCall.h"
#include "Skill/FrostCallZoneActor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"

void USkillFrostCall::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;

    bIsActive   = true;
    ElapsedTime = 0.f;
    Execute(Owner);
}

void USkillFrostCall::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (!bIsActive) return;

    ElapsedTime += DeltaTime;

    const float MaxHold = SkillData.HoldMaxTime > 0.f ? SkillData.HoldMaxTime : 4.f;
    if (ElapsedTime >= MaxHold)
    {
        bIsActive = false;
        StopActiveZone();
        ElapsedTime = 0.f;
    }
}

void USkillFrostCall::OnKeyUp(AActor* Owner)
{
    if (!bIsActive) return;

    bIsActive = false;
    StopActiveZone();
    ElapsedTime = 0.f;
}

void USkillFrostCall::ForceCancel(AActor* Owner)
{
    Super::ForceCancel(Owner);
    StopActiveZone();
}

void USkillFrostCall::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !ZoneClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    FHitResult CursorHit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)) return;

    // 사거리 초과 시 경계로 클램프
    FVector TargetLoc   = CursorHit.Location;
    FVector OwnerLoc    = Owner->GetActorLocation();
    FVector Horizontal  = TargetLoc - OwnerLoc;
    Horizontal.Z = 0.f;
    if (Horizontal.Size() > MaxCastRange)
    {
        TargetLoc   = OwnerLoc + Horizontal.GetSafeNormal() * MaxCastRange;
        TargetLoc.Z = CursorHit.Location.Z;
    }

    UWorld* World = Owner->GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AFrostCallZoneActor* Zone = World->SpawnActor<AFrostCallZoneActor>(
        ZoneClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);

    if (Zone)
    {
        ActiveZone = Zone;
        const float TickDamage = (Character ? Character->AttackPower : 100.f)
                               * SkillData.DamageCoefficient;
        Zone->Activate(TickDamage, Character ? Character->GetController() : nullptr);

        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan,
            FString::Printf(TEXT("[혹한의 부름] 발동 — 틱당 %.0f 데미지"), TickDamage));
    }
}

void USkillFrostCall::StopActiveZone()
{
    if (ActiveZone.IsValid())
    {
        ActiveZone->StopZone();
        ActiveZone.Reset();
    }
}
