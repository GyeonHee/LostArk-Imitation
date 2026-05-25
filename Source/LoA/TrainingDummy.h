// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingDummy.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDummyHPChanged, float /*CurrentHP*/, float /*MaxHP*/);

UCLASS()
class LOA_API ATrainingDummy : public AActor
{
	GENERATED_BODY()

public:
	ATrainingDummy();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHP = 5000.f;

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
};
