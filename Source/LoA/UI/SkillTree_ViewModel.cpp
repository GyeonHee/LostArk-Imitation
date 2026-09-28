// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/SkillTree_ViewModel.h"
#include "Skill/SkillManagerComponent.h"

void USkillTree_ViewModel::Initialize(USkillManagerComponent* InManager)
{
    SkillManagerWeak = InManager;
    SkillEntries.Empty();

    if (!InManager) return;

    UE_MVVM_SET_PROPERTY_VALUE(AvailableSkillPoints, InManager->AvailableSkillPoints);

    for (const FName& RowName : InManager->GetAllSkillRowNames())
    {
        // 대시·즉시 기상·기본공격은 스킬트리에서 배우거나 슬롯에 넣는 스킬이 아니다
        if (!InManager->IsSkillTreeRow(RowName)) continue;

        USkillTreeEntry_ViewModel* Entry = NewObject<USkillTreeEntry_ViewModel>(this);
        Entry->Initialize(RowName, InManager, this);
        SkillEntries.Add(Entry);
    }
}

void USkillTree_ViewModel::Refresh()
{
    if (USkillManagerComponent* SM = SkillManagerWeak.Get())
        UE_MVVM_SET_PROPERTY_VALUE(AvailableSkillPoints, SM->AvailableSkillPoints);

    for (auto& Entry : SkillEntries)
        if (Entry) Entry->Refresh();
}

bool USkillTree_ViewModel::AssignSkillToSlot(FName RowName, int32 SlotIndex)
{
    if (USkillManagerComponent* SM = SkillManagerWeak.Get())
        return SM->AssignSkillToSlot(RowName, SlotIndex);
    return false;
}
