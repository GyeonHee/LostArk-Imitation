// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillCast.h"
#include "Engine/Engine.h"

void USkillCast::OnKeyDown(AActor* Owner)
{
    bIsActive = true;
    ElapsedTime = 0.0f;
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
        FString::Printf(TEXT("[캐스팅] 시작 (%.1f초)"), SkillData.CastTime));
}

void USkillCast::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (!bIsActive) return;

    ElapsedTime += DeltaTime;

    // 키 1번을 고정 슬롯으로 사용 → 매 프레임 덮어써서 한 줄만 유지
    GEngine->AddOnScreenDebugMessage(1, 0.1f, FColor::Yellow,
        FString::Printf(TEXT("[캐스팅] 진행 중 %.1f / %.1f초"), ElapsedTime, SkillData.CastTime));

    if (ElapsedTime >= SkillData.CastTime)
    {
        bIsActive = false;
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[캐스팅] 성공!"));
        if (IsValid(Owner)) Execute(Owner);
    }
}

void USkillCast::OnKeyUp(AActor* Owner)
{
    if (!bIsActive) return;  // 이미 완료된 경우엔 취소 메시지 안 띄움

    bIsActive = false;
    ElapsedTime = 0.0f;
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[캐스팅] 취소"));
}