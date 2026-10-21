// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingDummy.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class ADamageNumberActor;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDummyHPChanged, float /*CurrentHP*/, float /*MaxHP*/);

/**
 * 허수아비 — 로비에서 DPS를 재는 대상. 보스와 같은 피격 판정(ULoADamageLibrary::ResolveIncomingHit)을 거치고
 * 데미지 미터에 기록되며(허수아비 세션: 5초 무피격 시 종료), 보스와 같은 흰색/노란색(치명) 데미지 숫자를 띄운다.
 * 기본은 HP 무한 — 스킬 한 방이 수천만이라 유한 HP면 바로 0이 된다.
 */
UCLASS()
class LOA_API ATrainingDummy : public AActor
{
	GENERATED_BODY()

public:
	ATrainingDummy();

protected:
	virtual void BeginPlay() override;

public:
	/** 켜면 맞아도 HP가 줄지 않는다 (DPS 측정용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	bool bInfiniteHP = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHP = 5000.f;

	/** 피격 데미지 숫자 — 기본값 BP_DamageNumber (보스와 같은 액터) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	TSubclassOf<ADamageNumberActor> DamageNumberClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	float DamageNumberHeight = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	float DamageNumberJitter = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	FLinearColor NormalDamageNumberColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	FLinearColor CritDamageNumberColor = FLinearColor(1.f, 0.82f, 0.15f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage")
	float CritDamageNumberScale = 1.3f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float HP;

	FOnDummyHPChanged OnHPChanged;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category="Stats")
	void ResetHP();

	UFUNCTION(BlueprintPure, Category="Stats")
	float GetHPPercent() const { return MaxHP > 0.f ? HP / MaxHP : 0.f; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> HPWidget;

	FTimerHandle RegenTimerHandle;
	void RegenTick();

	void SpawnDamageNumber(float DamageAmount, bool bCritical);
	int32 DamageNumberCounter = 0;
};
