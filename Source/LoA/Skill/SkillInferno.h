#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillInstant.h"
#include "SkillInferno.generated.h"

class AInfernoZoneActor;

/**
 * 인페르노 — 즉발 스킬, 사거리 밖이면 경계까지 자동이동 후 즉시 발동
 */
UCLASS(Blueprintable)
class LOA_API USkillInferno : public USkillInstant
{
    GENERATED_BODY()

public:
    virtual void OnKeyDown(AActor* Owner) override;
    virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
    virtual void OnKeyUp(AActor* Owner) override;
    virtual bool IsMovingToRange() const override { return bMovingToRange; }
    virtual void CancelRangeMove(AActor* Owner) override;

    UPROPERTY(EditDefaultsOnly, Category="Inferno")
    TSubclassOf<AInfernoZoneActor> ZoneClass;

    UPROPERTY(EditDefaultsOnly, Category="Inferno")
    float MaxCastRange = 800.f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;

private:
    bool bMovingToRange = false;
    FVector PendingTarget;

    void CancelMovement(AActor* Owner);
};
