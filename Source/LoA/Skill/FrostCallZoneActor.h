#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrostCallZoneActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 혹한의 부름 장판 액터
 * 데미지 타이머(DamageTickInterval, MaxDamageTicks)와
 * VFX 타이머(VFXTickInterval, IciclesPerTick)가 독립적으로 동작
 */
UCLASS(Blueprintable)
class LOA_API AFrostCallZoneActor : public AActor
{
    GENERATED_BODY()

public:
    AFrostCallZoneActor();

    void Activate(float InTickDamage, AController* InInstigator);
    void StopZone();

    // 루핑 장판 원 VFX
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> CircleVFXSystem;

    // Scale=1.0일 때 원 VFX 실측 반경 (cm)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float CircleBaseRadius = 100.f;

    // 고드름 낙하 VFX
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> IcicleVFXSystem;

    // 고드름 스폰 높이 오프셋 (cm)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float IcicleSpawnHeight = 400.f;

    // 틱당 스폰할 고드름 개수 (데미지와 무관한 순수 연출)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    int32 IciclesPerTick = 3;

    // 고드름 VFX 간격 (초) — 데미지와 독립
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float VFXTickInterval = 0.2f;

    // 데미지 적용 반경 (cm)
    UPROPERTY(EditDefaultsOnly, Category="FrostCall")
    float ZoneRadius = 400.f;

    // 데미지 틱 간격 (초)
    UPROPERTY(EditDefaultsOnly, Category="FrostCall")
    float DamageTickInterval = 0.5f;

    // 총 데미지 횟수 (즉시 1회 포함) — 기본 9 = 즉시1 + 0.5s×8
    UPROPERTY(EditDefaultsOnly, Category="FrostCall")
    int32 MaxDamageTicks = 9;

    // 종료 후 액터 수명
    UPROPERTY(EditDefaultsOnly, Category="FrostCall")
    float LifeAfterStop = 0.5f;

protected:
    virtual void BeginPlay() override;

private:
    float TickDamage = 0.f;
    int32 CurrentDamageTick = 0;
    bool bStopped = false;
    TWeakObjectPtr<AController> InstigatorController;

    FTimerHandle DamageTimerHandle;
    FTimerHandle VFXTimerHandle;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> CircleVFXComp;

    void ApplyDamageTick();
    void SpawnIcicleVFX();
};
