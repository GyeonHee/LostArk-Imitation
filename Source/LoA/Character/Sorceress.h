#pragma once

#include "CoreMinimal.h"
#include "Character/MageCharacter.h"
#include "Sorceress.generated.h"

class USorceressIdentityComponent;

/**
 * 소서리스 — 아이덴티티 = 신비한 마력 게이지 + 마력 방출(Z) / 점멸(X). 규칙은 USorceressIdentityComponent
 * 스페이스 대시는 짧은 순간이동(DT 대시 행 DashImpulse 거리) — 이동기라 추진력만 발동, 아크 패시브의 "점멸"은 X
 */
UCLASS()
class LOA_API ASorceress : public AMageCharacter
{
	GENERATED_BODY()

public:
	ASorceress();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
	TObjectPtr<USorceressIdentityComponent> Identity;

	/** 점멸 이동 — 커서 쪽으로 최대 15m. 벽(WorldStatic)에서 멈추고 보스는 넘는다. 바닥이 없는 곳이면 앞으로 당긴다 */
	bool BlinkTo(const FVector& TargetLocation);

	virtual void ExecuteDash_Implementation(const FVector& TargetLocation) override;

	/** 마력 해방 슈퍼아머 — 넉다운·경직 무시 */
	virtual void ApplyKnockdown(const FVector& SourceLocation) override;
	virtual void ApplyStagger() override;

	/** 마력 방출 (MageCharacter 공통 진입점) */
	virtual void ActivateIdentity() override;
	virtual float GetIdentityGaugePercent() const override;

	virtual void BeginPlay() override;
};
