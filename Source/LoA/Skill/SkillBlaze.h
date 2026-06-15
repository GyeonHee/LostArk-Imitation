#pragma once

#include "CoreMinimal.h"
#include "SkillInstant.h"
#include "SkillBlaze.generated.h"

class ABlazeProjectile;

/**
 * 블레이즈 — 커서 위치에 화염 존을 소환하는 즉발 스킬
 * 존 안에 있는 적에게 ZoneDuration 동안 틱 데미지 적용
 */
UCLASS(Blueprintable)
class LOA_API USkillBlaze : public USkillInstant
{
    GENERATED_BODY()

public:
    // BP_BlazeProjectile 할당
    UPROPERTY(EditDefaultsOnly, Category="Blaze")
    TSubclassOf<ABlazeProjectile> ProjectileClass;

    // 박스 절반 크기 (X=앞뒤 길이, Y=좌우 폭, Z=높이)
    UPROPERTY(EditDefaultsOnly, Category="Blaze")
    FVector BoxHalfExtent = FVector(200.f, 80.f, 80.f);

    // 존 유지 시간 (초)
    UPROPERTY(EditDefaultsOnly, Category="Blaze")
    float ZoneDuration = 5.f;

    // 틱당 데미지 = AttackPower * BurnDamageRatio
    UPROPERTY(EditDefaultsOnly, Category="Blaze")
    float BurnDamageRatio = 0.3f;

    // 틱 간격 (초)
    UPROPERTY(EditDefaultsOnly, Category="Blaze")
    float BurnInterval = 1.0f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;
};
