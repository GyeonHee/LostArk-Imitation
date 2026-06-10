// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SkillTreeEntry_ViewModel.generated.h"

class USkillManagerComponent;
class USkillTree_ViewModel;

/**
 * 스킬트리 리스트의 행(row) 하나에 대응하는 ViewModel.
 * WBP_SkillTreeRow의 ListView 아이템으로 사용된다.
 */
UCLASS(BlueprintType)
class LOA_API USkillTreeEntry_ViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    FText SkillName;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    FText SkillTypeText;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    int32 CurrentLevel = 0;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    int32 MaxLevel = 10;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    bool bCanLevelUp = false;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category="SkillTree")
    bool bCanLevelDown = false;

    // SkillTree_ViewModel::Initialize 에서 호출
    void Initialize(FName InRowName, USkillManagerComponent* InManager, USkillTree_ViewModel* InOwner);

    // 스킬매니저 상태에서 FieldNotify 프로퍼티들을 동기화
    void Refresh();

    UFUNCTION(BlueprintCallable, Category="SkillTree")
    void LevelUp();

    UFUNCTION(BlueprintCallable, Category="SkillTree")
    void LevelDown();

    UFUNCTION(BlueprintPure, Category="SkillTree")
    FName GetRowName() const { return RowName; }

private:
    FName RowName;
    TWeakObjectPtr<USkillManagerComponent> SkillManagerWeak;
    TWeakObjectPtr<USkillTree_ViewModel> OwnerViewModelWeak;
};
