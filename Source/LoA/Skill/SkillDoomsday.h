#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillCast.h"
#include "SkillDoomsday.generated.h"

class ADoomsdayMeteor;

/**
 * 종말의 날 — 마우스 커서 위치에 거대 메테오가 낙하하는 캐스팅 스킬
 * 사거리 밖: 캐릭터 이동 → 사거리 진입 시 캐스팅 시작 → 완료 시 발동
 * 이동/캐스팅 중 키 해제 → 취소 + 쿨타임 없음
 */
UCLASS(Blueprintable)
class LOA_API USkillDoomsday : public USkillCast
{
    GENERATED_BODY()

public:
    virtual void OnKeyDown(AActor* Owner) override;
    virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
    virtual void OnKeyUp(AActor* Owner) override;
    virtual void ForceCancel(AActor* Owner) override;

    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    TSubclassOf<ADoomsdayMeteor> MeteorClass;

    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    float MaxCastRange = 800.f;

    UPROPERTY(EditDefaultsOnly, Category="Doomsday")
    float ExplosionRadius = 350.f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;

private:
    bool bMovingToRange = false;
    FVector PendingCastTarget;

    void SpawnMeteorAt(AActor* Owner, const FVector& TargetPos, float Damage);
    void CancelMovement(AActor* Owner);
};
