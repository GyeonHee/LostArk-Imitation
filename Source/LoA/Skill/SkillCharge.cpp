// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillCharge.h"
#include "Engine/Engine.h"
#include "Stat/StatComponent.h"

void USkillCharge::OnKeyDown(AActor* Owner)
{
    bIsActive = true;
    ElapsedTime = 0.0f;
}

void USkillCharge::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (!bIsActive) return;

    // 신속의 공격(캐스팅) 속도만큼 빨리 찬다
    ElapsedTime += DeltaTime * UStatComponent::GetChargeSpeedMultiplierOf(Owner);

    // 최대 차징 도달 → 자동 발동
    if (SkillData.ChargeMaxTime > 0.0f && ElapsedTime >= SkillData.ChargeMaxTime)
    {
        bIsActive = false;
        if (IsValid(Owner)) Execute(Owner);
        ElapsedTime = 0.0f;
        return;
    }

}

bool USkillCharge::GetCastProgress(float& OutElapsed, float& OutTotal) const
{
    if (!bIsActive || SkillData.ChargeMaxTime <= 0.f) return false;

    OutElapsed = FMath::Clamp(ElapsedTime, 0.f, SkillData.ChargeMaxTime);
    OutTotal = SkillData.ChargeMaxTime;
    return true;
}

void USkillCharge::OnKeyUp(AActor* Owner)
{
    if (!bIsActive) return;

    bIsActive = false;
    if (IsValid(Owner)) Execute(Owner);
    ElapsedTime = 0.0f;
}