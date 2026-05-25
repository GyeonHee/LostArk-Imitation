#pragma once

#include "CoreMinimal.h"
#include "Character/MageCharacter.h"
#include "Sorceress.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnIdentityOrbsChanged, int32 /*NewOrbs*/);

/**
 * 소서리스: Identity = 마법 구슬 (Arcane Torrent, 최대 3개)
 * 스킬 시전 시 구슬 충전 → ActivateIdentity()로 소모하여 효과 발동
 */
UCLASS()
class LOA_API ASorceress : public AMageCharacter
{
	GENERATED_BODY()

public:
	ASorceress();

	static constexpr int32 MaxIdentityOrbs = 3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity")
	int32 IdentityOrbs = 0;

	/** 구슬 변경 시 브로드캐스트 (UI 바인딩용) */
	FOnIdentityOrbsChanged OnIdentityOrbsChanged;

	/** 스킬 시전 후 스킬 클래스에서 호출하여 구슬 충전 */
	void AddIdentityOrb(int32 Count = 1);

	virtual void ExecuteDash_Implementation(const FVector& TargetLocation) override;

	virtual void ActivateIdentity() override;
	virtual float GetIdentityGaugePercent() const override;
};
