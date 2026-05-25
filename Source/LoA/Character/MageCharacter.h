#pragma once

#include "CoreMinimal.h"
#include "LoACharacter.h"
#include "MageCharacter.generated.h"

/**
 * 마법사 계열 캐릭터 공통 베이스 (소서리스, 서머너 상속)
 * Identity 인터페이스 및 마법사 기본 스탯 정의
 */
UCLASS(abstract)
class LOA_API AMageCharacter : public ALoACharacter
{
	GENERATED_BODY()

public:
	AMageCharacter();

	/** Identity 게이지 활성화 (각 직업별 구현) */
	UFUNCTION(BlueprintCallable, Category="Identity")
	virtual void ActivateIdentity() {}

	/** Identity 게이지 비율 반환 0.0 ~ 1.0 (UI 바인딩용) */
	UFUNCTION(BlueprintPure, Category="Identity")
	virtual float GetIdentityGaugePercent() const { return 0.f; }
};
