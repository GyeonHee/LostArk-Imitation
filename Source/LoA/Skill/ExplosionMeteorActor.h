#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExplosionMeteorActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 익스플로전 — 캐릭터 정면에서 커서까지 직선 비행 후 범위 폭발
 * MeteorVFXComponent(Niagara)가 액터에 붙어 같이 이동
 * Tick에서 거리 체크 후 ArrivalThreshold 진입 시 Land() 호출
 */
UCLASS(Blueprintable)
class LOA_API AExplosionMeteorActor : public AActor
{
    GENERATED_BODY()

public:
    AExplosionMeteorActor();
    virtual void Tick(float DeltaTime) override;

    void Activate(const FVector& InCharWaistLoc, const FVector& InTargetLoc,
                  float InExplosionRadius, float InDamage, AController* InInstigator);

    // 캐스팅 완료 후 CircleShowTime 동안 커서 위치에 발판 원 표시
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> CircleVFXSystem;

    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float CircleBaseRadius = 100.f;

    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float CircleShowTime = 0.8f;

    // 액터에 부착되어 함께 이동하는 비행 VFX (중력 제거한 NS_Magma_Shot 복사본 할당)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    TObjectPtr<UNiagaraComponent> MeteorVFXComponent;

    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> MeteorVFXSystem;

    // 착지 시 커서 위치에 스폰할 폭발 VFX (NS_Magma_Shot 원본 또는 별도 폭발 이펙트 할당)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> ExplosionVFXSystem;

    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float VFXScale = 1.0f;

    // 캐릭터 앞 스폰 오프셋 (cm)
    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float SpawnForwardOffset = 80.f;

    // 허리 높이에서 추가 Z 오프셋 (cm)
    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float SpawnZOffset = 0.f;

    // 직선 이동 속도 (cm/s)
    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float MeteorSpeed = 1500.f;

    // 커서와 이 거리 이하가 되면 폭발 (cm)
    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float ArrivalThreshold = 80.f;

    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float LifeAfterLanding = 0.5f;

    // BP에서 폭발 VFX/사운드 재생
    UFUNCTION(BlueprintImplementableEvent, Category="VFX")
    void BP_OnLand(FVector LandLocation);

protected:
    virtual void BeginPlay() override;

private:
    FVector TargetLocation;
    float Damage = 0.f;
    float InternalExplosionRadius = 300.f;
    bool bFlying = false;
    bool bLanded = false;
    TWeakObjectPtr<AController> InstigatorController;

    FTimerHandle CircleTimerHandle;

    void StartFlying();
    void Land();
};
