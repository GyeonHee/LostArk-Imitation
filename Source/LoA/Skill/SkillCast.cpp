// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillCast.h"
#include "Engine/Engine.h"
#include "Stat/StatComponent.h"

void USkillCast::OnKeyDown(AActor* Owner)
{
    bIsActive = true;
    ElapsedTime = 0.0f;
    CurrentCastTime = GetEffectiveCastTime(Owner);
    PlaySkillCastAnim(Owner);
}

void USkillCast::OnKeyHeld(AActor* Owner, float DeltaTime)
{
    if (!bIsActive) return;

    // 신속의 공격(캐스팅) 속도만큼 빨리 찬다
    ElapsedTime += DeltaTime * UStatComponent::GetCastSpeedMultiplierOf(Owner);


    if (ElapsedTime >= CurrentCastTime)
    {
        bIsActive = false;
        PlaySkillReleaseAnim(Owner, false);  // 방향은 캐스팅 시작 때 정했다
        if (IsValid(Owner)) Execute(Owner);
    }
}

bool USkillCast::GetCastProgress(float& OutElapsed, float& OutTotal) const
{
    // bIsActive가 false인 동안(사거리 이동 중 등)은 아직 캐스팅이 시작된 게 아니므로 바를 띄우지 않는다
    if (!bIsActive || CurrentCastTime <= 0.f) return false;

    OutElapsed = FMath::Clamp(ElapsedTime, 0.f, CurrentCastTime);
    OutTotal = CurrentCastTime;
    return true;
}

void USkillCast::OnKeyUp(AActor* Owner)
{
    if (!bIsActive) return;  // 이미 완료된 경우엔 취소 메시지 안 띄움

    bIsActive = false;
    ElapsedTime = 0.0f;
    StopSkillCastAnim(Owner);
}