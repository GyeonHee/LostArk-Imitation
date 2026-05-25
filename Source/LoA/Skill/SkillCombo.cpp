// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillCombo.h"
#include "Engine/Engine.h"

void USkillCombo::OnKeyDown(AActor* Owner)
{
    if (!IsValid(Owner)) return;
    const FString Label = SkillData.SkillName.IsEmpty()
        ? TEXT("콤보")
        : SkillData.SkillName.ToString();
    GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Cyan,
        FString::Printf(TEXT("[%s] %d단계 발동!"), *Label, CurrentComboStep + 1));
    Execute(Owner);
}