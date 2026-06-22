#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IceArrowZoneActor.generated.h"

class UNiagaraSystem;

/**
 * 아이스 에로우 존 액터
 * Activate() 호출 시 첫 틱 즉시 발동 + 0.5s 간격 총 4틱
 * 고드름 VFX는 캐릭터→커서 방향 45도 각도로 스폰
 */
UCLASS(Blueprintable)
class LOA_API AIceArrowZoneActor : public AActor
{
    GENERATED_BODY()

public:
    AIceArrowZoneActor();

    // InTickDamage: 틱당 데미지 (총 데미지 / MaxTicks로 나눠서 전달)
    // InIcicleRotation: 고드름 낙하 방향 (캐릭터→커서, 45도 하향)
    void Activate(float InTickDamage, AController* InInstigator, FRotator InIcicleRotation);

    // 고드름 낙하 VFX
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> IcicleVFXSystem;

    // 고드름 스폰 높이 오프셋 (cm)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float IcicleSpawnHeight = 500.f;

    // 수평 드리프트 보정 (cm) — Niagara 수평 속도로 인한 착지점 오차 보정
    // 고드름이 캐릭터 쪽으로 흘러서 범위 밖에 착지할 때: 목표 방향으로 스폰 위치를 밀어 상쇄
    // IcicleSpawnHeight와 같은 값이 45도 기준 정확한 보정값
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float IcicleHorizontalDrift = 500.f;

    // 틱당 고드름 개수
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    int32 IciclesPerTick = 5;

    // 데미지 반경 (cm)
    UPROPERTY(EditDefaultsOnly, Category="IceArrow")
    float ZoneRadius = 350.f;

    // 틱 간격 (초) — 4틱 × 0.5s = 총 1.5초
    UPROPERTY(EditDefaultsOnly, Category="IceArrow")
    float TickInterval = 0.5f;

    // 총 데미지 틱 수
    UPROPERTY(EditDefaultsOnly, Category="IceArrow")
    int32 MaxTicks = 4;

    UPROPERTY(EditDefaultsOnly, Category="IceArrow")
    float LifeAfterDone = 0.5f;

protected:
    virtual void BeginPlay() override;

private:
    float TickDamage = 0.f;
    int32 CurrentTick = 0;
    FRotator IcicleRotation;
    TWeakObjectPtr<AController> InstigatorController;
    FTimerHandle TickTimerHandle;

    void ApplyTick();
};
