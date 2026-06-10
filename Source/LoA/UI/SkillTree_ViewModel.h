// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UI/SkillTreeEntry_ViewModel.h"
#include "SkillTree_ViewModel.generated.h"

class USkillManagerComponent;

/**
 * 스킬트리 창 전체의 ViewModel.
 * LoAPlayerController가 생성하고 WBP_SkillTree에 주입한다.
 */
UCLASS(BlueprintType)
class LOA_API USkillTree_ViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    // 잔여 스킬포인트 — Text/Image 위젯에 직접 바인딩
    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    int32 AvailableSkillPoints = 0;

    // 스킬 목록 — Widget::NativeConstruct에서 ListView->SetListItems 에 전달
    UPROPERTY(BlueprintReadOnly, Category="SkillTree")
    TArray<TObjectPtr<USkillTreeEntry_ViewModel>> SkillEntries;

    // PlayerController가 스킬매니저 포함 Pawn을 Possess하면 호출
    void Initialize(USkillManagerComponent* InManager);

    // 스킬포인트 및 모든 Entry 상태 갱신 (Entry::LevelUp/Down 후 자동 호출)
    UFUNCTION(BlueprintCallable, Category="SkillTree")
    void Refresh();

    // 드래그앤드랍·우클릭으로 슬롯에 스킬 배정
    UFUNCTION(BlueprintCallable, Category="SkillTree")
    bool AssignSkillToSlot(FName RowName, int32 SlotIndex);

private:
    TWeakObjectPtr<USkillManagerComponent> SkillManagerWeak;
};
