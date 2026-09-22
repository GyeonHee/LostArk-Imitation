// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillTypes.h"
#include "SkillBase.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class LOA_API USkillBase : public UObject
{
	GENERATED_BODY()
	
public:

    // 이 스킬이 DataTable에서 참조할 행 이름 (각 스킬 BP에서 설정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
    FName SkillRowName;

    // SkillManagerComponent가 DataTable 데이터 읽어서 여기에 세팅
    UPROPERTY(BlueprintReadOnly)
    FSkillData SkillData;

    // SkillManagerComponent가 호출하는 함수들
    virtual void OnKeyDown(AActor* Owner);
    virtual void OnKeyHeld(AActor* Owner, float DeltaTime);
    virtual void OnKeyUp(AActor* Owner);

    bool IsActive() const { return bIsActive; }

    // 사거리 자동이동 중인지 (SkillManagerComponent 틱에서 사용)
    virtual bool IsMovingToRange() const { return false; }

    // 사거리 이동 취소 (쿨타임 없음 — 플레이어가 직접 이동 시 호출)
    virtual void CancelRangeMove(AActor* Owner) {}

    // SkillManagerComponent가 이동 입력 시 강제 취소할 때 호출
    virtual void ForceCancel(AActor* Owner);

    /** 캐스팅/차지 진행 상황 — 진행 중일 때만 true를 반환하고 경과/전체 시간을 채운다.
     *  즉발·홀딩처럼 "차오르는 진행도" 개념이 없는 타입은 기본 구현 그대로 false. 캐스트바 UI가 이걸로 폴링한다 */
    virtual bool GetCastProgress(float& OutElapsed, float& OutTotal) const { return false; }

protected:

    // 실제 스킬 효과 발동 (서브클래스에서 오버라이드)
    UFUNCTION(BlueprintNativeEvent)
    void Execute(AActor* Owner);
    virtual void Execute_Implementation(AActor* Owner);

    // 시전 진행 시간
    float ElapsedTime = 0.0f;

    // 현재 활성화 여부
    bool bIsActive = false;
};
