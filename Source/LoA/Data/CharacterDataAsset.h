#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterDataAsset.generated.h"

/**
 * 캐릭터별 기본 스탯 데이터
 * 에디터에서 DA_Sorceress, DA_Summoner 등으로 생성하여 할당
 */
UCLASS(BlueprintType)
class LOA_API UCharacterDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MaxHP = 100000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MaxMP = 5000.f;

	// 초당 MaxMP의 몇 배를 회복할지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MPRegenRate = 0.044f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float MoveSpeed = 600.f;

	// 스킬 계수 곱셈 기준값 (스킬 데미지 = AttackPower * DamageCoefficient)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
	float AttackPower = 100.f;
};
