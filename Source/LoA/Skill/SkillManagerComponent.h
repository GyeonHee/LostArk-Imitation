// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Skill/SkillBase.h"
#include "SkillManagerComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillSlotChanged, int32, SlotIndex, FName, RowName);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LOA_API USkillManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USkillManagerComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
    // 슬롯 인덱스 상수 (0~17 = 스킬 18개, 18 = 대시)
    static constexpr int32 BasicAttackSlotIndex = 8;
    static constexpr int32 DashSlotIndex = 18;

    // 스킬 데이터 테이블 (FSkillData 기반)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skills")
    TObjectPtr<UDataTable> SkillDataTable;

    // 에디터에서 슬롯에 스킬 클래스 배치 (0=Q, 1=W, 2=E, 3=R, 4=A, 5=S, 6=D, 7=F)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills")
    TArray<TSubclassOf<USkillBase>> SlotClasses;

    // 기본공격 클래스 (좌클릭 / C키)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skills")
    TSubclassOf<USkillBase> BasicAttackClass;

    // 대시 클래스 (스페이스바, 없어도 쿨타임은 동작)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skills")
    TSubclassOf<USkillBase> DashClass;

    // DT_Skills에서 대시 행 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skills")
    FName DashRowName = FName("Dash");

    // 컨트롤러가 호출하는 함수
    void HandleKeyDown(int32 SlotIndex);
    void HandleKeyHeld(int32 SlotIndex, float DeltaTime);
    void HandleKeyUp(int32 SlotIndex);

    // UI에서 쿨타임 비율 조회용 (0.0 ~ 1.0)
    UFUNCTION(BlueprintCallable, Category="Skills")
    float GetCooldownRatio(int32 SlotIndex) const;

    // 남은 쿨타임 초 반환 (텍스트 표시용)
    UFUNCTION(BlueprintCallable, Category="Skills")
    float GetRemainingCooldown(int32 SlotIndex) const;

    // 슬롯의 스킬 데이터 전체 반환 (슬롯 비어있으면 기본값)
    UFUNCTION(BlueprintCallable, Category="Skills")
    FSkillData GetSlotSkillData(int32 SlotIndex) const;

    // 대시 전용: 쿨타임 중인지 확인 (Blueprint에서 사용)
    UFUNCTION(BlueprintCallable, Category="Skills")
    bool IsSlotOnCooldown(int32 SlotIndex) const { return IsOnCooldown(SlotIndex); }

    // 대시 전용: 외부에서 쿨타임 시작 (Blueprint에서 대시 실행 후 호출)
    UFUNCTION(BlueprintCallable, Category="Skills")
    void TriggerCooldown(int32 SlotIndex) { StartCooldown(SlotIndex); }

    // 슬롯 아이콘 텍스처 반환 (없으면 nullptr)
    UFUNCTION(BlueprintCallable, Category="Skills")
    UTexture2D* GetSlotIcon(int32 SlotIndex) const;

    // 슬롯에 스킬이 배정되어 있는지 확인
    UFUNCTION(BlueprintPure, Category="Skills")
    bool IsSlotAssigned(int32 SlotIndex) const;

    // 슬롯에 배정된 스킬 RowName 반환 (비어있으면 NAME_None)
    UFUNCTION(BlueprintPure, Category="Skills")
    FName GetSlotRowName(int32 SlotIndex) const;

    // 비어있는 첫 번째 슬롯(0~7) 인덱스 반환. 모두 찼으면 -1
    UFUNCTION(BlueprintCallable, Category="Skills")
    int32 FindFirstEmptySlot() const;

    // ─── 스킬트리 ───────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SkillTree")
    int32 AvailableSkillPoints = 10;

    UPROPERTY(BlueprintReadOnly, Category="SkillTree")
    TMap<FName, int32> SkillLevels;

    UFUNCTION(BlueprintCallable, Category="SkillTree")
    int32 GetSkillLevel(FName RowName) const;

    UFUNCTION(BlueprintCallable, Category="SkillTree")
    bool LevelUpSkill(FName RowName);

    UFUNCTION(BlueprintCallable, Category="SkillTree")
    bool LevelDownSkill(FName RowName);

    // DataTable의 모든 Row 이름 반환 (스킬트리 목록 구성용)
    UFUNCTION(BlueprintCallable, Category="SkillTree")
    TArray<FName> GetAllSkillRowNames() const;

    // 스킬트리에서 드래그앤드랍/우클릭으로 슬롯(0~7)에 스킬 배정
    UFUNCTION(BlueprintCallable, Category="SkillTree")
    bool AssignSkillToSlot(FName RowName, int32 SlotIndex);

    // 슬롯이 변경될 때마다 브로드캐스트 — WBP_HUD가 바인딩해서 아이콘 갱신
    UPROPERTY(BlueprintAssignable, Category="SkillTree")
    FOnSkillSlotChanged OnSkillSlotChanged;

private:
    UPROPERTY()
    TArray<TObjectPtr<USkillBase>> SlotInstances;

    // 쿨타임 종료 시각 (WorldTime 기준)
    TArray<float> CooldownEndTimes;
    TArray<float> CooldownDurations;

    // 콤보 상태
    int32 ComboSlotIndex = -1;
    int32 ComboStep = 0;
    FTimerHandle ComboWindowTimer;

    bool IsValidSlot(int32 SlotIndex) const;
    bool IsOnCooldown(int32 SlotIndex) const;
    void StartCooldown(int32 SlotIndex);
    void HandleComboInput(int32 SlotIndex);
    void ResetCombo();
};
