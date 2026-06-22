#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillInstant.h"
#include "SkillGust.generated.h"

class AGustTornadoActor;

/**
 * 돌풍 — 즉발 스킬
 * 커서 방향으로 캐릭터를 회전시킨 뒤 캐릭터 앞 ForwardSpawnOffset 거리에 토네이도 존 스폰
 */
UCLASS(Blueprintable)
class LOA_API USkillGust : public USkillInstant
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category="Gust")
    TSubclassOf<AGustTornadoActor> ZoneClass;

    // 캐릭터 앞 스폰 거리 (cm)
    UPROPERTY(EditDefaultsOnly, Category="Gust")
    float ForwardSpawnOffset = 200.f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;
};
