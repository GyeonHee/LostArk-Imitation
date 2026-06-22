#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GustTornadoActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 돌풍 토네이도 존 액터
 * Activate() 시 VFX 시작 + 전방 박스 범위 1회 데미지 후 LifeAfterDone 뒤 소멸
 */
UCLASS(Blueprintable)
class LOA_API AGustTornadoActor : public AActor
{
    GENERATED_BODY()

public:
    AGustTornadoActor();

    void Activate(float InDamage, AController* InInstigator);

    // 토네이도 VFX
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> TornadoVFXSystem;

    // 박스 전방 절반 길이 (전방 X축, cm)
    UPROPERTY(EditDefaultsOnly, Category="Gust")
    float BoxHalfLength = 300.f;

    // 박스 측면 절반 너비 (Y축, cm)
    UPROPERTY(EditDefaultsOnly, Category="Gust")
    float BoxHalfWidth = 150.f;

    // 박스 절반 높이 (Z축, cm)
    UPROPERTY(EditDefaultsOnly, Category="Gust")
    float BoxHalfHeight = 150.f;

    UPROPERTY(EditDefaultsOnly, Category="Gust")
    float LifeAfterDone = 1.0f;

protected:
    virtual void BeginPlay() override;

private:
    float Damage = 0.f;
    TWeakObjectPtr<AController> InstigatorController;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> TornadoVFXComp;

    void ApplyDamage();
};
