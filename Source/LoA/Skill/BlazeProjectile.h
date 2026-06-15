#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlazeProjectile.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 블레이즈 화염 존
 * - SpreadDuration 동안 콜리전 박스가 앞으로 점진적으로 확장
 * - 박스에 새로 겹친 적 → 즉시 1타 + CurrentActors에 추가
 * - 이후 DamageTimer가 1초마다 CurrentActors 전체에 틱 데미지
 */
UCLASS(Blueprintable)
class LOA_API ABlazeProjectile : public AActor
{
    GENERATED_BODY()

public:
    ABlazeProjectile();
    virtual void Tick(float DeltaTime) override;

    void Activate(const FVector& InBoxExtent, float InTickDamage,
                  float InTickInterval, float InDuration, AController* InInstigator);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    TObjectPtr<UNiagaraComponent> ZoneVFX;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TObjectPtr<UNiagaraSystem> HitVFX;

    // 콜리전이 앞으로 퍼지는 시간 (BP에서 조정 가능)
    UPROPERTY(EditDefaultsOnly, Category="Blaze")
    float SpreadDuration = 0.7f;

    // Niagara 에셋의 X 방향 자연 반길이 (스케일 1일 때 절반 길이) — BP에서 에셋에 맞게 조정
    UPROPERTY(EditDefaultsOnly, Category="VFX")
    float VFXHalfLengthX = 200.f;

    UFUNCTION(BlueprintImplementableEvent, Category="VFX")
    void BP_OnHit(AActor* HitActor, FVector HitLocation);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    TObjectPtr<UBoxComponent> CollisionBox;

    TSet<AActor*> CurrentActors;
    FTimerHandle DamageTimer;
    float TickDamage = 0.f;
    TWeakObjectPtr<AController> InstigatorController;

    // 스프레드 상태
    float SpreadElapsed = 0.f;
    float TargetHalfExtentX = 200.f;
    float TargetHalfExtentY = 80.f;
    float TargetHalfExtentZ = 80.f;
    bool bSpreadComplete = false;

    bool IsValidTarget(AActor* Target) const;
    void ApplyInitialHit(AActor* Target);
    void OnDamageTick();
};
