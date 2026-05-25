// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Skill/SkillManagerComponent.h"
#include "Data/CharacterDataAsset.h"
#include "LoACharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHPChanged, float /*NewHP*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMPChanged, float /*NewMP*/);

/**
 *  A controllable top-down perspective character
 */
UCLASS(abstract)
class ALoACharacter : public ACharacter
{
	GENERATED_BODY()

private:

	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	float MPRegenAccum = 0.f;

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skills")
	TObjectPtr<USkillManagerComponent> SkillManager;

	// 에디터에서 DA_Sorceress 등 할당 — BeginPlay에서 스탯 초기화에 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
	TObjectPtr<UCharacterDataAsset> CharacterData;

	UPROPERTY(BlueprintReadOnly, Category="Stats")
	float AttackPower = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float HP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxHP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
	float MP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MaxMP;

	FOnHPChanged OnHPChanged;
	FOnMPChanged OnMPChanged;

	/** Controller가 매 프레임 설정하는 원하는 이동 속도벡터 (ZeroVector = 이동 없음) */
	FVector ControllerMoveVelocity = FVector::ZeroVector;

protected:
	/** 초당 MaxMP의 몇 배를 회복할지 (서브클래스에서 오버라이드 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats")
	float MPRegenRate = 0.044f;

public:
	/** Constructor */
	ALoACharacter();

	virtual void PostInitializeComponents() override;

	/** Initialization */
	virtual void BeginPlay() override;

	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	float GetHP() const { return HP; }
	float GetMaxHP() const { return MaxHP; }
	float GetMP() const { return MP; }
	float GetMaxMP() const { return MaxMP; }

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** 대쉬 실행 — 서브클래스에서 오버라이드하여 직업별 이동 방식 구현 */
	UFUNCTION(BlueprintNativeEvent, Category="Movement")
	void ExecuteDash(const FVector& TargetLocation);
	virtual void ExecuteDash_Implementation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void ReceiveDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual bool ConsumeMP(float Amount);

	UFUNCTION(BlueprintCallable, Category="Stats")
	virtual void RestoreMP(float Amount);

	/** Returns the camera component **/
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** Returns the Camera Boom component **/
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

};

