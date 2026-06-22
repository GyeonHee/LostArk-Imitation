#include "Skill/SkillGust.h"
#include "Skill/GustTornadoActor.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"

void USkillGust::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !ZoneClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    FHitResult CursorHit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)) return;

    // 캐릭터 → 커서 수평 방향
    const FVector OwnerLoc = Owner->GetActorLocation();
    FVector HorizDir = CursorHit.Location - OwnerLoc;
    HorizDir.Z = 0.f;
    if (HorizDir.IsNearlyZero()) return;
    HorizDir.Normalize();

    // 커서 방향으로 캐릭터 회전 + 이동 즉시 정지
    const FRotator FaceRot(0.f, HorizDir.Rotation().Yaw, 0.f);
    Owner->SetActorRotation(FaceRot);
    PC->CancelAutoMove();

    // 캐릭터 앞 스폰 위치 (바닥 높이 유지)
    FVector SpawnLoc = OwnerLoc + HorizDir * ForwardSpawnOffset;
    SpawnLoc.Z = OwnerLoc.Z;

    UWorld* World = Owner->GetWorld();
    if (!World) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGustTornadoActor* Zone = World->SpawnActor<AGustTornadoActor>(
        ZoneClass, SpawnLoc, FaceRot, SpawnParams);

    if (Zone)
    {
        const float TotalDamage = (Character ? Character->AttackPower : 100.f)
                                * SkillData.DamageCoefficient;
        Zone->Activate(TotalDamage, Character ? Character->GetController() : nullptr);

        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green,
            FString::Printf(TEXT("[돌풍] 발동 — 총 데미지 %.0f"), TotalDamage));
    }
}
