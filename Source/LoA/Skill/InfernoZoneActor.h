#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InfernoZoneActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 인페르노 장판 액터
 * 1) 장판 원 VFX 즉시 표시 (CircleShowTime 동안)
 * 2) CircleShowTime 후 위로 솟구치는 폭발 VFX 1회 스폰 + 데미지
 */
UCLASS(Blueprintable)
class LOA_API AInfernoZoneActor : public AActor
{
    GENERATED_BODY()

public:
    AInfernoZoneActor();

    void Activate(float InDamage, AController* InInstigator);

    // 장판 범위 표시용 원 VFX (NS_MeteorCircle 계열 또는 루핑 원)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> CircleVFXSystem;

    // Scale=1.0일 때 원 VFX의 실측 반경 (cm)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float CircleBaseRadius = 100.f;

    // 원 표시 후 폭발까지 대기 시간 (초)
    UPROPERTY(EditDefaultsOnly, Category="Inferno")
    float CircleShowTime = 1.0f;

    // 위로 솟구치는 폭발 VFX (NS_Inferno_Impact 할당)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> ExplosionVFXSystem;

    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float ExplosionVFXScale = 1.0f;

    // 데미지 적용 반경 (cm)
    UPROPERTY(EditDefaultsOnly, Category="Inferno")
    float ZoneRadius = 300.f;

    UPROPERTY(EditDefaultsOnly, Category="Inferno")
    float LifeAfterExplosion = 1.0f;

    UFUNCTION(BlueprintImplementableEvent, Category="VFX")
    void BP_OnExplode(FVector ExplodeLocation);

protected:
    virtual void BeginPlay() override;

private:
    float Damage = 0.f;
    float InternalZoneRadius = 300.f;
    TWeakObjectPtr<AController> InstigatorController;
    FTimerHandle ExplodeTimerHandle;

    void Explode();
};
