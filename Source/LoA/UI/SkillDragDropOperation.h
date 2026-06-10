// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "UI/SkillTreeEntry_ViewModel.h"
#include "SkillDragDropOperation.generated.h"

/**
 * 스킬트리 행을 스킬슬롯으로 드래그할 때 생성되는 오퍼레이션.
 * WBP_SkillTreeRow::NativeOnMouseButtonDown 에서 생성한다.
 */
UCLASS(BlueprintType)
class LOA_API USkillDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillTree")
    FName SkillRowName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillTree")
    TObjectPtr<UTexture2D> SkillIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillTree")
    TObjectPtr<USkillTreeEntry_ViewModel> EntryViewModel;
};
