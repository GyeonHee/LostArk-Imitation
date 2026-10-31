// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillBase.h"
#include "SkillCast.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class LOA_API USkillCast : public USkillBase
{
	GENERATED_BODY()
	
public:
	virtual void OnKeyDown(AActor* Owner) override;
	virtual void OnKeyHeld(AActor* Owner, float DeltaTime) override;
	virtual void OnKeyUp(AActor* Owner) override;

	virtual bool GetCastProgress(float& OutElapsed, float& OutTotal) const override;

private:
	// 시작할 때 정한 이번 캐스팅 시간 — 도중에 마력 강화가 끝나도 바 길이가 바뀌지 않게
	float CurrentCastTime = 0.f;
};
