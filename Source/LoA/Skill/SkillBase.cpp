// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillBase.h"

void USkillBase::OnKeyDown(AActor* Owner)
{
}

void USkillBase::OnKeyHeld(AActor* Owner, float DeltaTime)
{
}

void USkillBase::OnKeyUp(AActor* Owner)
{
}


void USkillBase::ForceCancel(AActor* Owner)
{
    bIsActive = false;
    ElapsedTime = 0.f;
}

void USkillBase::Execute_Implementation(AActor* Owner)
{
}
