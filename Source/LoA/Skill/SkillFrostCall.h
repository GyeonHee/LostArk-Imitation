#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillBase.h"
#include "SkillFrostCall.generated.h"

class AFrostCallZoneActor;

/**
 * 혹한의 부름 — 홀딩 스킬
 * 키 누르는 중 커서 위치에 장판 소환 + 고드름 틱 데미지 (0.5초 간격, 최대 4초)
 * 키 해제 또는 이동 입력 시 즉시 중단
 */
UCLASS(Blueprintable)
class LOA_API USkillFrostCall : public USkillBase
{
    GENERATED_BODY()

public:
    virtual void OnKeyDown(AActor* Owner) override;
    virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
    virtual void OnKeyUp(AActor* Owner) override;
    virtual void ForceCancel(AActor* Owner) override;

    UPROPERTY(EditDefaultsOnly, Category="FrostCall")
    TSubclassOf<AFrostCallZoneActor> ZoneClass;

    // 최대 시전 사거리 (이 거리 이내에 커서가 있어야 함 — 초과 시 사거리 경계에 스폰)
    UPROPERTY(EditDefaultsOnly, Category="FrostCall")
    float MaxCastRange = 800.f;

protected:
    virtual void Execute_Implementation(AActor* Owner) override;

private:
    TWeakObjectPtr<AFrostCallZoneActor> ActiveZone;

    void StopActiveZone();
};
