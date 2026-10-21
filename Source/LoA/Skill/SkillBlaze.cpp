#include "SkillBlaze.h"
#include "BlazeProjectile.h"
#include "LoACharacter.h"
#include "Stat/LoADamageLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

void USkillBlaze::Execute_Implementation(AActor* Owner)
{
    if (!IsValid(Owner) || !ProjectileClass) return;

    UWorld* World = Owner->GetWorld();
    if (!World) return;

    ALoACharacter* Character = Cast<ALoACharacter>(Owner);
    APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
    if (!PC) return;

    // 마우스 커서가 가리키는 바닥 위치
    FHitResult CursorHit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit)) return;

    // 캐릭터 → 커서 방향으로 존 회전
    FVector Direction = CursorHit.Location - Owner->GetActorLocation();
    Direction.Z = 0.f;
    const FRotator SpawnRotation = Direction.IsNearlyZero()
        ? Owner->GetActorRotation()
        : Direction.GetSafeNormal().Rotation();

    // 캐릭터도 커서 방향으로 즉시 회전
    Owner->SetActorRotation(SpawnRotation);

    // 캡슐 바닥(발 위치) 기준으로 스폰 — GetActorLocation()은 캡슐 중심(허리)이므로 HalfHeight만큼 내림
    float CapsuleRadius = 42.f;
    float CapsuleHalfHeight = 88.f;
    if (const ACharacter* Char = Cast<ACharacter>(Owner))
    {
        CapsuleRadius     = Char->GetCapsuleComponent()->GetScaledCapsuleRadius();
        CapsuleHalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }

    const FVector FeetLocation = Owner->GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight);
    const FVector SpawnLocation = FeetLocation
        + Direction.GetSafeNormal() * CapsuleRadius
        + FVector(0.f, 0.f, 5.f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.Instigator = Cast<APawn>(Owner);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ABlazeProjectile* Zone = World->SpawnActor<ABlazeProjectile>(
        ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
    // 데미지 미터가 이 액터의 피해를 어느 스킬로 집계할지 알 수 있게
    ULoADamageLibrary::TagDamageSource(Zone, SkillRowName);
    if (!Zone) return;

    AController* Instigator = Character ? Character->GetController() : nullptr;

    // DT 계수 = 존 안에 계속 있는 대상이 받는 총 피해. 맞는 횟수 = 첫 타 1번 + BurnInterval마다(존이 ZoneDuration 뒤 사라짐)
    // 예전엔 DT 계수(2.0)를 안 쓰고 BurnDamageRatio(0.3) × 타수로 따로 정해져 있었다
    const int32 BurnHits = 1 + FMath::FloorToInt((ZoneDuration - KINDA_SMALL_NUMBER) / FMath::Max(0.01f, BurnInterval));
    const float BurnTickDamage = ULoADamageLibrary::CalcOutgoingDamage(Character, SkillData.DamageCoefficient, SkillRowName) / BurnHits;

    Zone->Activate(
        BoxHalfExtent,
        BurnTickDamage,
        BurnInterval,
        ZoneDuration,
        Instigator
    );
}
