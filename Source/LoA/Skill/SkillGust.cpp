#include "Skill/SkillGust.h"
#include "Skill/GustTornadoActor.h"
#include "LoACharacter.h"
#include "Stat/LoADamageLibrary.h"
#include "LoAPlayerController.h"
#include "Engine/Engine.h"

void USkillGust::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !ZoneClass) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    ALoAPlayerController* PC = Character
        ? Cast<ALoAPlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    // 조준 지점 — 바닥 트레이스가 빗나가도(거울 카운터의 비스듬한 시점에서 맵 밖·하늘을 가리킬 때) 방향은 나온다.
    // 예전엔 여기서 return해서 토네이도 없이 마나·쿨타임만 빠졌다(TryActivateSkill이 Execute 결과와 무관하게 StartCooldown)
    FVector AimLocation;
    if (!PC->GetCursorAimLocation(AimLocation)) return;

    // 캐릭터 → 커서 수평 방향
    const FVector OwnerLoc = Owner->GetActorLocation();
    FVector HorizDir = AimLocation - OwnerLoc;
    HorizDir.Z = 0.f;
    if (HorizDir.IsNearlyZero()) HorizDir = Owner->GetActorForwardVector();  // 커서가 캐릭터 위 — 바라보는 방향으로
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
    // 데미지 미터가 이 액터의 피해를 어느 스킬로 집계할지 알 수 있게
    ULoADamageLibrary::TagDamageSource(Zone, SkillRowName);

    if (Zone)
    {
        const float TotalDamage = ULoADamageLibrary::CalcOutgoingDamage(Character, SkillData.DamageCoefficient, SkillRowName);
        Zone->Activate(TotalDamage, Character ? Character->GetController() : nullptr, SkillData.bCanCounter);

        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green,
            FString::Printf(TEXT("[돌풍] 발동 — 총 데미지 %.0f"), TotalDamage));
    }
}
