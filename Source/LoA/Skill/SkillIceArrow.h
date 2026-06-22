#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillInstant.h"
#include "SkillIceArrow.generated.h"

class AIceArrowZoneActor;

/**
 * 아이스 에로우 — 즉발 스킬
 * 커서 위치에 원형 범위, 캐릭터→커서 방향 45도로 고드름 낙하
 * 1.5초 동안 4틱 (틱당 총 데미지의 25%)
 */
UCLASS(Blueprintable)
class LOA_API USkillIceArrow : public USkillInstant
{
    GENERATED_BODY()

public:
    virtual void OnKeyDown(AActor* Owner) override;
    virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
    virtual void OnKeyUp(AActor* Owner) override;
    virtual bool IsMovingToRange() const override { return bMovingToRange; }
    virtual void CancelRangeMove(AActor* Owner) override;

    UPROPERTY(EditDefaultsOnly, Category="IceArrow")
    TSubclassOf<AIceArrowZoneActor> ZoneClass;

    UPROPERTY(EditDefaultsOnly, Category="IceArrow")
    float MaxCastRange = 900.f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;

private:
    bool bMovingToRange = false;
    FVector PendingTarget;

    void CancelMovement(AActor* Owner);
};
