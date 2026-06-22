// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SkillTypes.generated.h"

/**
 * 
 */

class USkillBase;

UENUM(BlueprintType)
enum class ESkillInputType : uint8
{
    Instant   UMETA(DisplayName = "즉발"),
    Cast      UMETA(DisplayName = "캐스팅"),
    Charge    UMETA(DisplayName = "차지"),
    Combo     UMETA(DisplayName = "콤보"),
    Hold      UMETA(DisplayName = "홀딩")
};

USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText SkillName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ESkillInputType InputType = ESkillInputType::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ManaCost = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Cooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float CastTime = 1.0f;          // Cast 전용

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ChargeMaxTime = 2.0f;     // Charge 전용

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float HoldMaxTime = 4.0f;       // Hold 전용 — 최대 홀딩 시간

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ComboMaxStep = 3;         // Combo 전용

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ComboWindow = 0.8f;       // 콤보 입력 허용 시간(초)

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<USkillBase> SkillClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DashImpulse = 2500.f;

    // 스킬 계수 — 캐릭터 공격력에 곱해서 최종 데미지 산출 (예: 3.5 = 공격력의 350%)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DamageCoefficient = 1.0f;

    // 스킬트리 관련
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText SkillTypeLabel;  // "일반", "지점", "홀딩" 등 UI 표시용

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=1))
    int32 MaxSkillLevel = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=1))
    int32 SkillPointCostPerLevel = 1;
};


