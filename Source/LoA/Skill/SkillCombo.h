// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillBase.h"
#include "SkillCombo.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class LOA_API USkillCombo : public USkillBase
{
	GENERATED_BODY()
	
public:
    virtual void OnKeyDown(AActor* Owner) override;

    // 현재 콤보 단계 (SkillManagerComponent가 외부에서 세팅)
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentComboStep = 0;
};
