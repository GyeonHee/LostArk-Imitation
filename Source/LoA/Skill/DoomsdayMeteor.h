#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DoomsdayMeteor.generated.h"

class UNiagaraSystem;

/**
 * 종말의 날 — 시작 위치에서 VFX를 1회 재생하고, VFXTravelTime 후 착지 데미지
 * NS_Magma_Shot 자체 폭발 효과를 사용하므로 별도 ExplosionVFX 없음
 */
UCLASS(Blueprintable)
class LOA_API ADoomsdayMeteor : public AActor
{
    GENERATED_BODY()

public:
    ADoomsdayMeteor();

    void Activate(const FVector& InTargetLocation, float InFallDelay,
                  float InExplosionRadius, float InDamage,
                  AController* InInstigator);

    // 캐스팅 완료 후 표시되는 범위 원 VFX (NS_Free_Magic_Circle2 복제본 할당)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> CircleVFXSystem;

    // 원이 보여지는 시간 (초) — 이 시간 후 메테오 낙하 시작
    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    float CircleShowTime = 1.0f;

    // Scale=1.0일 때 원 VFX의 실제 반경 (cm) — 에디터에서 스케일 1.0으로 재생해 측정 후 입력
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float CircleBaseRadius = 100.f;

    // BP에서 NS_Magma_Shot 할당
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> MeteorVFXSystem;

    // 목표 지점 위 스폰 높이 (cm)
    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    float SpawnHeightOffset = 1200.f;

    /**
     * NS_Magma_Shot이 스폰 위치에서 바닥에 닿을 때까지 걸리는 시간 (초)
     * 에디터에서 VFX 재생 후 육안으로 착지 시점을 맞춰 설정
     * 이 시간에 맞춰 데미지가 들어감
     */
    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    float VFXTravelTime = 1.0f;

    // 착지 후 액터 생존 시간
    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    float LifeAfterLanding = 0.5f;

    // VFX 스케일 — 클수록 NS_Magma_Shot 이펙트가 크게 재생됨
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float VFXScale = 1.0f;

    /**
     * VFX 스폰 위치 월드 공간 보정값 (X, Y, 0)
     * 불꽃이 빨간 구체보다 월드 +X로 300cm 밀리면 X = -300 설정
     */
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    FVector VFXSpawnOffset = FVector::ZeroVector;

    UFUNCTION(BlueprintImplementableEvent, Category="VFX")
    void BP_OnLand(FVector LandLocation);

protected:
    virtual void BeginPlay() override;

private:
    FVector TargetLocation;
    float Damage = 0.f;
    float InternalExplosionRadius = 350.f;
    TWeakObjectPtr<AController> InstigatorController;

    FTimerHandle DelayTimerHandle;
    FTimerHandle LandTimerHandle;

    void StartFalling();
    void Land();
};
