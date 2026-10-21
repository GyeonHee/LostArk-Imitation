#include "Skill/SkillFrostCall.h"
#include "Skill/FrostCallZoneActor.h"
#include "LoACharacter.h"
#include "Stat/LoADamageLibrary.h"
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

bool USkillFrostCall::GetCastProgress(float& OutElapsed, float& OutTotal) const
{
    if (!bIsActive) return false;

    // OnKeyHeld의 자동 종료 판정과 같은 폴백을 써야 바가 실제 지속시간과 어긋나지 않는다
    const float MaxHold = SkillData.HoldMaxTime > 0.f ? SkillData.HoldMaxTime : 4.f;
    OutElapsed = FMath::Clamp(ElapsedTime, 0.f, MaxHold);
    OutTotal = MaxHold;
    return true;
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
    // 데미지 미터가 이 액터의 피해를 어느 스킬로 집계할지 알 수 있게
    ULoADamageLibrary::TagDamageSource(Zone, SkillRowName);

    if (Zone)
    {
        ActiveZone = Zone;
        // DT 계수 = 끝까지 홀드했을 때의 총 피해 → 최대 틱 수로 나눠 틱당 피해 (아이스 에로우와 같은 방식).
        // 예전엔 계수를 틱마다 통째로 줘서 9틱이면 계수 72가 됐다 — 데미지 미터에서 딜 비중 45%로 드러남
        const float TotalDamage = ULoADamageLibrary::CalcOutgoingDamage(Character, SkillData.DamageCoefficient, SkillRowName);
        const AFrostCallZoneActor* ZoneCDO = ZoneClass->GetDefaultObject<AFrostCallZoneActor>();
        const int32 Ticks = ZoneCDO ? FMath::Max(1, ZoneCDO->MaxDamageTicks) : 1;
        const float TickDamage = TotalDamage / Ticks;
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
