// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillInstant.h"
#include "Engine/Engine.h"

void USkillInstant::OnKeyDown(AActor* Owner)
{
	if (!IsValid(Owner)) return;
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[즉발] 스킬 발동!"));
	Execute(Owner);
}
