// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillBase.h"
#include "SkillCharge.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class LOA_API USkillCharge : public USkillBase
{
	GENERATED_BODY()
	
public:
	virtual void OnKeyDown(AActor* Owner) override;
	virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
	virtual void OnKeyUp(AActor* Owner) override;
};
