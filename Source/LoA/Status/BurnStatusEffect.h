#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BurnStatusEffect.generated.h"

/**
 * 화상 DoT 컴포넌트 — 적에게 동적으로 추가되어 틱 데미지를 적용
 * BlazeProjectile이 RegisterComponent() 이후 Initialize()를 호출해 타이머 시작
 */
UCLASS(ClassGroup=(Status))
class LOA_API UBurnStatusEffect : public UActorComponent
{
    GENERATED_BODY()

public:
    UBurnStatusEffect();

    void Initialize(float InDamagePerTick, float InInterval, float InDuration,
                    AController* InInstigator, AActor* InCauser);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    float DamagePerTick = 0.f;
    int32 TicksRemaining = 0;

    TWeakObjectPtr<AController> InstigatorController;
    TWeakObjectPtr<AActor> CauserActor;
    bool bTracksProjectile = false;

    FTimerHandle BurnTimer;
    void OnBurnTick();
};
