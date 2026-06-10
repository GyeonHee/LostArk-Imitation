// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SkillTreeEntry_ViewModel.h"
#include "UI/SkillTree_ViewModel.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"

void USkillTreeEntry_ViewModel::Initialize(FName InRowName, USkillManagerComponent* InManager, USkillTree_ViewModel* InOwner)
{
    RowName = InRowName;
    SkillManagerWeak = InManager;
    OwnerViewModelWeak = InOwner;

    if (InManager && InManager->SkillDataTable)
    {
        if (FSkillData* Data = InManager->SkillDataTable->FindRow<FSkillData>(InRowName, TEXT("")))
        {
            UE_MVVM_SET_PROPERTY_VALUE(SkillName, Data->SkillName);
            UE_MVVM_SET_PROPERTY_VALUE(SkillTypeText, Data->SkillTypeLabel);
            UE_MVVM_SET_PROPERTY_VALUE(MaxLevel, Data->MaxSkillLevel);
            UE_MVVM_SET_PROPERTY_VALUE(Icon, Data->Icon.LoadSynchronous());
        }
    }

    Refresh();
}

void USkillTreeEntry_ViewModel::Refresh()
{
    USkillManagerComponent* SM = SkillManagerWeak.Get();
    if (!SM) return;

    const int32 Level = SM->GetSkillLevel(RowName);
    UE_MVVM_SET_PROPERTY_VALUE(CurrentLevel, Level);

    if (FSkillData* Data = SM->SkillDataTable ? SM->SkillDataTable->FindRow<FSkillData>(RowName, TEXT("")) : nullptr)
    {
        const bool bUp = Level < Data->MaxSkillLevel && SM->AvailableSkillPoints >= Data->SkillPointCostPerLevel;
        UE_MVVM_SET_PROPERTY_VALUE(bCanLevelUp, bUp);
    }
    UE_MVVM_SET_PROPERTY_VALUE(bCanLevelDown, Level > 0);
}

void USkillTreeEntry_ViewModel::LevelUp()
{
    USkillManagerComponent* SM = SkillManagerWeak.Get();
    if (!SM || !SM->LevelUpSkill(RowName)) return;

    if (USkillTree_ViewModel* Owner = OwnerViewModelWeak.Get())
        Owner->Refresh();
}

void USkillTreeEntry_ViewModel::LevelDown()
{
    USkillManagerComponent* SM = SkillManagerWeak.Get();
    if (!SM || !SM->LevelDownSkill(RowName)) return;

    if (USkillTree_ViewModel* Owner = OwnerViewModelWeak.Get())
        Owner->Refresh();
}
