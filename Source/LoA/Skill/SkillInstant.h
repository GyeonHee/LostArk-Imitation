// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillBase.h"
#include "SkillInstant.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class LOA_API USkillInstant : public USkillBase
{
	GENERATED_BODY()
	
public:
	virtual void OnKeyDown(AActor* Owner) override;
};
