#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightningStrikeActor.generated.h"

class UNiagaraSystem;

/**
 * 천벌 — 목표 위치에 번개가 내리꽂히는 액터
 * NS_Lightning_Strike VFX를 재생하고 StrikeDelay 후 범위 데미지
 */
UCLASS(Blueprintable)
class LOA_API ALightningStrikeActor : public AActor
{
    GENERATED_BODY()

public:
    ALightningStrikeActor();

    void Activate(const FVector& InTargetLocation, float InExplosionRadius,
                  float InDamage, AController* InInstigator);

    // 캐스팅 완료 후 표시되는 범위 원 VFX (NS_Free_Magic_Circle2 복제본 할당)
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> CircleVFXSystem;

    // 원이 보여지는 시간 (초) — 이 시간 후 번개 VFX 재생
    UPROPERTY(EditDefaultsOnly, Category="Lightning")
    float CircleShowTime = 1.0f;

    // Scale=1.0일 때 원 VFX의 실제 반경 (cm) — 에디터에서 스케일 1.0으로 재생해 측정 후 입력
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float CircleBaseRadius = 100.f;

    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> LightningVFXSystem;

    // 번개 VFX 스폰 후 데미지까지 딜레이 (번개가 내리꽂히는 시점에 맞춤)
    UPROPERTY(EditDefaultsOnly, Category="Lightning")
    float StrikeDelay = 0.3f;

    // 타격 후 액터 생존 시간
    UPROPERTY(EditDefaultsOnly, Category="Lightning")
    float LifeAfterStrike = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float VFXScale = 1.0f;

    UFUNCTION(BlueprintImplementableEvent, Category="VFX")
    void BP_OnStrike(FVector StrikeLocation);

protected:
    virtual void BeginPlay() override;

private:
    FVector TargetLocation;
    float Damage = 0.f;
    float InternalExplosionRadius = 300.f;
    TWeakObjectPtr<AController> InstigatorController;

    FTimerHandle StrikeTimerHandle;
    FTimerHandle DamageTimerHandle;

    void Strike();     // CircleShowTime 후 번개 VFX 재생
    void ApplyDamage(); // StrikeDelay 후 실제 데미지
};
