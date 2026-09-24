#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CounterableInterface.generated.h"

class UPrimitiveComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCounterable : public UInterface
{
	GENERATED_BODY()
};

/**
 * [카운터 가능] 스킬에 맞았을 때 카운터 판정을 받는 대상 — 보스 본체(AEchidnaBoss)와 거울 카운터의 거울 벽(AEchidnaMirrorWallActor).
 * 스킬 액터(돌풍 토네이도)는 대상의 종류를 모르고 이 인터페이스만 부른다.
 */
class LOA_API ICounterable
{
	GENERATED_BODY()

public:
	/** Attacker = 스킬 시전자(헤드어택 판정 기준 위치), HitComponent = 실제로 맞은 컴포넌트(거울 7개 중 어느 것인지 구분용).
	 *  성공하면 true */
	virtual bool TryCounterHit(AActor* Attacker, UPrimitiveComponent* HitComponent) = 0;
};
