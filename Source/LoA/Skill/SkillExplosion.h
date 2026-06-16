#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillCast.h"
#include "SkillExplosion.generated.h"

class AExplosionMeteorActor;

/**
 * 익스플로전 — 캐릭터 방향에서 45도 각도로 날아오는 메테오 캐스팅 스킬
 * 사거리 밖: 캐릭터 이동 → 사거리 진입 시 캐스팅 시작 → 완료 시 발동
 */
UCLASS(Blueprintable)
class LOA_API USkillExplosion : public USkillCast
{
    GENERATED_BODY()

public:
    virtual void OnKeyDown(AActor* Owner) override;
    virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
    virtual void OnKeyUp(AActor* Owner) override;
    virtual void ForceCancel(AActor* Owner) override;
    virtual bool IsMovingToRange() const override { return bMovingToRange; }
    virtual void CancelRangeMove(AActor* Owner) override;

    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    TSubclassOf<AExplosionMeteorActor> MeteorClass;

    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float MaxCastRange = 1000.f;

    UPROPERTY(EditDefaultsOnly, Category="Explosion")
    float ExplosionRadius = 300.f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;

private:
    bool bMovingToRange = false;
    FVector PendingCastTarget;

    void SpawnMeteorAt(AActor* Owner, const FVector& TargetPos, float Damage);
    void CancelMovement(AActor* Owner);
};
