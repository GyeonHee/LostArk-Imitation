// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillCharge.h"
#include "Engine/Engine.h"

void USkillCharge::OnKeyDown(AActor* Owner)
{
    bIsActive = true;
    ElapsedTime = 0.0f;
    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Orange, TEXT("[차징] 시작"));
}

void USkillCharge::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (!bIsActive) return;

    ElapsedTime += DeltaTime;

    // 최대 차징 도달 → 자동 발동
    if (SkillData.ChargeMaxTime > 0.0f && ElapsedTime >= SkillData.ChargeMaxTime)
    {
        bIsActive = false;
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("[차징] 최대 도달 — 자동 발동! (%.1f초)"), SkillData.ChargeMaxTime));
        if (IsValid(Owner)) Execute(Owner);
        ElapsedTime = 0.0f;
        return;
    }

    // 키 2번을 고정 슬롯으로 사용 → 매 프레임 덮어써서 한 줄만 유지
    GEngine->AddOnScreenDebugMessage(2, 0.1f, FColor::Orange,
        FString::Printf(TEXT("[차징] 차징 중... %.1f / %.1f초"), ElapsedTime, SkillData.ChargeMaxTime));
}

void USkillCharge::OnKeyUp(AActor* Owner)
{
    if (!bIsActive) return;

    bIsActive = false;
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
        FString::Printf(TEXT("[차징] 발동! (%.1f초 차징)"), ElapsedTime));
    if (IsValid(Owner)) Execute(Owner);
    ElapsedTime = 0.0f;
}