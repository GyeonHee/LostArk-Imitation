// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillCombo.h"
#include "Engine/Engine.h"

void USkillCombo::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    Execute(Owner);
}