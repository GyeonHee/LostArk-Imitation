// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "HUD_ViewModel.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class LOA_API UHUD_ViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
    // FieldNotify = 값 변경 시 View에 자동으로 알림
    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    float HP = 1000.f;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    float MaxHP = 1000.f;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    float HPPercent = 1.f;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    FText HPText = FText::FromString(TEXT("1000 / 1000"));

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    float MP = 500.f;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    float MaxMP = 500.f;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    float MPPercent = 1.f;

    UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "Stats")
    FText MPText = FText::FromString(TEXT("500 / 500"));

    UFUNCTION(BlueprintCallable, Category = "Stats")
    void SetHP(float NewHP)
    {
        UE_MVVM_SET_PROPERTY_VALUE(HP, NewHP);
        UE_MVVM_SET_PROPERTY_VALUE(HPPercent, MaxHP > 0.f ? NewHP / MaxHP : 0.f);
        UE_MVVM_SET_PROPERTY_VALUE(HPText, FText::FromString(FString::Printf(TEXT("%d / %d"), (int32)NewHP, (int32)MaxHP)));
    }

    UFUNCTION(BlueprintCallable, Category = "Stats")
    void SetMaxHP(float NewMaxHP)
    {
        UE_MVVM_SET_PROPERTY_VALUE(MaxHP, NewMaxHP);
        UE_MVVM_SET_PROPERTY_VALUE(HPPercent, NewMaxHP > 0.f ? HP / NewMaxHP : 0.f);
        UE_MVVM_SET_PROPERTY_VALUE(HPText, FText::FromString(FString::Printf(TEXT("%d / %d"), (int32)HP, (int32)NewMaxHP)));
    }

    UFUNCTION(BlueprintCallable, Category = "Stats")
    void SetMP(float NewMP)
    {
        UE_MVVM_SET_PROPERTY_VALUE(MP, NewMP);
        UE_MVVM_SET_PROPERTY_VALUE(MPPercent, MaxMP > 0.f ? NewMP / MaxMP : 0.f);
        UE_MVVM_SET_PROPERTY_VALUE(MPText, FText::FromString(FString::Printf(TEXT("%d / %d"), (int32)NewMP, (int32)MaxMP)));
    }

    UFUNCTION(BlueprintCallable, Category = "Stats")
    void SetMaxMP(float NewMaxMP)
    {
        UE_MVVM_SET_PROPERTY_VALUE(MaxMP, NewMaxMP);
        UE_MVVM_SET_PROPERTY_VALUE(MPPercent, NewMaxMP > 0.f ? MP / NewMaxMP : 0.f);
        UE_MVVM_SET_PROPERTY_VALUE(MPText, FText::FromString(FString::Printf(TEXT("%d / %d"), (int32)MP, (int32)NewMaxMP)));
    }
};
