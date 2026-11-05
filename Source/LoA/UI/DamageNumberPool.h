#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DamageNumberPool.generated.h"

class AActor;
class ADamageNumberActor;

/**
 * 데미지 숫자 액터 풀 — 타격마다 액터·위젯을 만들고 부수는 비용을 없앤다.
 * 수명이 끝난 숫자는 파괴하지 않고 숨겨 두었다가 다음 타격에 다시 쓴다. 풀 크기는 동시에 떠 있던 최대 개수까지만 자란다.
 * 월드마다 하나라 레벨 이동 때 액터와 함께 정리된다. 켜고 끄기는 `LoA.DamageNumber.Pool`.
 */
UCLASS()
class LOA_API UDamageNumberPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** 쉬고 있는 숫자를 꺼내 Location에 보이게 한다. 없으면 새로 만든다 */
	ADamageNumberActor* Acquire(TSubclassOf<ADamageNumberActor> Class, const FVector& Location, AActor* Owner);

	/** 수명이 끝난 숫자를 숨기고 돌려받는다 */
	void Release(ADamageNumberActor* Number);

	/** 레벨 로딩 중에 미리 만들어 첫 타격 때 스폰 비용이 들지 않게 한다 */
	void Prewarm(TSubclassOf<ADamageNumberActor> Class, int32 Count, AActor* Owner);

private:
	ADamageNumberActor* SpawnPooled(TSubclassOf<ADamageNumberActor> Class, const FVector& Location, AActor* Owner);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADamageNumberActor>> FreeNumbers;
};
