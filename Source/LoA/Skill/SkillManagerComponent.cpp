// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillCombo.h"
#include "Engine/Engine.h"
#include "LoACharacter.h"

// Sets default values for this component's properties
USkillManagerComponent::USkillManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SlotClasses.SetNum(8);

}


// Called when the game starts
void USkillManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
    SlotInstances.SetNum(19);
    CooldownEndTimes.Init(0.0f, 19);
    CooldownDurations.Init(1.0f, 19);

    for (int32 i = 0; i < 8; i++)
    {
        if (!SlotClasses[i]) continue;

        SlotInstances[i] = NewObject<USkillBase>(GetOwner(), SlotClasses[i]);

        // SkillRowName이 있으면 DataTable에서 데이터 로드
        if (SkillDataTable && !SlotInstances[i]->SkillRowName.IsNone())
        {
            if (FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SlotInstances[i]->SkillRowName, TEXT("")))
            {
                SlotInstances[i]->SkillData = *Row;
                UE_LOG(LogTemp, Log, TEXT("[SkillManager] Slot %d: '%s' 로드 성공 (쿨타임: %.1f초)"),
                    i, *SlotInstances[i]->SkillRowName.ToString(), SlotInstances[i]->SkillData.Cooldown);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[SkillManager] Slot %d: RowName '%s' 를 DT에서 찾지 못함"),
                    i, *SlotInstances[i]->SkillRowName.ToString());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SkillManager] Slot %d: DataTable 없음 또는 SkillRowName 비어있음 (RowName: '%s')"),
                i, *SlotInstances[i]->SkillRowName.ToString());
        }
    }

    // 기본공격 슬롯 (인덱스 8) 생성
    if (BasicAttackClass)
    {
        SlotInstances[BasicAttackSlotIndex] = NewObject<USkillBase>(GetOwner(), BasicAttackClass);
        if (SkillDataTable && !SlotInstances[BasicAttackSlotIndex]->SkillRowName.IsNone())
        {
            if (FSkillData* Row = SkillDataTable->FindRow<FSkillData>(SlotInstances[BasicAttackSlotIndex]->SkillRowName, TEXT("")))
            {
                SlotInstances[BasicAttackSlotIndex]->SkillData = *Row;
                UE_LOG(LogTemp, Log, TEXT("[SkillManager] 기본공격 로드 성공 (쿨타임: %.1f초)"),
                    SlotInstances[BasicAttackSlotIndex]->SkillData.Cooldown);
            }
        }
    }

    // 대시 슬롯 (인덱스 18) - DashClass 없어도 DT에서 전체 데이터 로드
    if (SkillDataTable && !DashRowName.IsNone())
    {
        if (FSkillData* Row = SkillDataTable->FindRow<FSkillData>(DashRowName, TEXT("")))
        {
            CooldownDurations[DashSlotIndex] = Row->Cooldown;
            if (DashClass)
            {
                SlotInstances[DashSlotIndex] = NewObject<USkillBase>(GetOwner(), DashClass);
                SlotInstances[DashSlotIndex]->SkillData = *Row;
            }
            UE_LOG(LogTemp, Log, TEXT("[SkillManager] 대시 DT 로드 성공 (쿨타임: %.1f초, 임펄스: %.0f)"),
                Row->Cooldown, Row->DashImpulse);
        }
    }
    else if (DashClass)
    {
        SlotInstances[DashSlotIndex] = NewObject<USkillBase>(GetOwner(), DashClass);
    }
}

void USkillManagerComponent::HandleKeyDown(int32 SlotIndex)
{
    if (!IsValidSlot(SlotIndex)) return;
    if (IsOnCooldown(SlotIndex)) return;

    USkillBase* Skill = SlotInstances[SlotIndex];

    if (ALoACharacter* Char = Cast<ALoACharacter>(GetOwner()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] 슬롯%d 마나소모 시도: %.0f / 현재MP: %.0f"),
            SlotIndex, Skill->SkillData.ManaCost, Char->GetMP());
        if (!Char->ConsumeMP(Skill->SkillData.ManaCost))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Skill] 마나 부족으로 스킬 취소"));
            return;
        }
    }

    if (USkillCombo* ComboSkill = Cast<USkillCombo>(Skill))
    {
        HandleComboInput(SlotIndex);
        return;
    }

    Skill->OnKeyDown(GetOwner());

    // Charge, Cast는 완료/취소 시점에 쿨타임 시작
    const bool bDeferCooldown = Skill->SkillData.InputType == ESkillInputType::Charge
                             || Skill->SkillData.InputType == ESkillInputType::Cast;
    if (!bDeferCooldown)
    {
        StartCooldown(SlotIndex);
    }
}

void USkillManagerComponent::HandleKeyHeld(int32 SlotIndex, float DeltaTime)
{
    if (!IsValidSlot(SlotIndex)) return;

    USkillBase* Skill = SlotInstances[SlotIndex];
    const bool bWasActive = Skill->IsActive();
    Skill->OnKeyHeld(GetOwner(), DeltaTime);

    // Charge/Cast가 자동 완료된 경우 즉시 쿨타임 시작
    const bool bAutoCompleted = bWasActive && !Skill->IsActive();
    if (bAutoCompleted &&
        (Skill->SkillData.InputType == ESkillInputType::Charge ||
         Skill->SkillData.InputType == ESkillInputType::Cast))
    {
        StartCooldown(SlotIndex);
    }
}

void USkillManagerComponent::HandleKeyUp(int32 SlotIndex)
{
    if (!IsValidSlot(SlotIndex)) return;

    USkillBase* Skill = SlotInstances[SlotIndex];
    const bool bWasActive = Skill->IsActive();
    Skill->OnKeyUp(GetOwner());

    // 키를 수동으로 뗄 때 쿨타임 시작 (Charge: 차징 발동, Cast: 취소/성공 모두)
    if (bWasActive &&
        (Skill->SkillData.InputType == ESkillInputType::Charge ||
         Skill->SkillData.InputType == ESkillInputType::Cast))
    {
        StartCooldown(SlotIndex);
    }
}

void USkillManagerComponent::HandleComboInput(int32 SlotIndex)
{
    USkillCombo* ComboSkill = Cast<USkillCombo>(SlotInstances[SlotIndex]);
    if (!ComboSkill) return;

    // 다른 슬롯 누름 → 콤보 리셋 후 새로 시작
    if (ComboSlotIndex != SlotIndex)
    {
        ResetCombo();
        ComboSlotIndex = SlotIndex;
    }

    ComboSkill->CurrentComboStep = ComboStep;
    ComboSkill->OnKeyDown(GetOwner());

    ComboStep++;

    if (ComboStep >= ComboSkill->SkillData.ComboMaxStep)
    {
        StartCooldown(SlotIndex);
        ResetCombo();
    }
    else
    {
        // 콤보 창 타이머 재시작
        GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimer);
        GetWorld()->GetTimerManager().SetTimer(
            ComboWindowTimer,
            this, &USkillManagerComponent::ResetCombo,
            ComboSkill->SkillData.ComboWindow,
            false
        );
    }
}

float USkillManagerComponent::GetCooldownRatio(int32 SlotIndex) const
{
    UWorld* World = GetWorld();
    if (!World || !CooldownEndTimes.IsValidIndex(SlotIndex)) return 0.0f;
    if (CooldownDurations[SlotIndex] <= 0.0f) return 0.0f;

    float Remaining = CooldownEndTimes[SlotIndex] - World->GetTimeSeconds();
    return FMath::Clamp(Remaining / CooldownDurations[SlotIndex], 0.0f, 1.0f);
}

bool USkillManagerComponent::IsValidSlot(int32 SlotIndex) const
{
    return SlotInstances.IsValidIndex(SlotIndex) && SlotInstances[SlotIndex] != nullptr;
}

bool USkillManagerComponent::IsOnCooldown(int32 SlotIndex) const
{
    UWorld* World = GetWorld();
    if (!World || !CooldownEndTimes.IsValidIndex(SlotIndex)) return false;
    return World->GetTimeSeconds() < CooldownEndTimes[SlotIndex];
}

void USkillManagerComponent::StartCooldown(int32 SlotIndex)
{
    UWorld* World = GetWorld();
    if (!World || !CooldownEndTimes.IsValidIndex(SlotIndex)) return;
    if (SlotInstances.IsValidIndex(SlotIndex) && SlotInstances[SlotIndex])
    {
        CooldownDurations[SlotIndex] = SlotInstances[SlotIndex]->SkillData.Cooldown;
    }
    CooldownEndTimes[SlotIndex] = World->GetTimeSeconds() + CooldownDurations[SlotIndex];
}

float USkillManagerComponent::GetRemainingCooldown(int32 SlotIndex) const
{
    UWorld* World = GetWorld();
    if (!World || !CooldownEndTimes.IsValidIndex(SlotIndex)) return 0.0f;
    return FMath::Max(0.0f, CooldownEndTimes[SlotIndex] - World->GetTimeSeconds());
}

FSkillData USkillManagerComponent::GetSlotSkillData(int32 SlotIndex) const
{
    if (SlotInstances.IsValidIndex(SlotIndex) && SlotInstances[SlotIndex])
        return SlotInstances[SlotIndex]->SkillData;

    // 인스턴스 없이 DT만 쓰는 슬롯 (대시 등) 처리
    if (SlotIndex == DashSlotIndex && SkillDataTable && !DashRowName.IsNone())
    {
        if (FSkillData* Row = SkillDataTable->FindRow<FSkillData>(DashRowName, TEXT("")))
            return *Row;
    }

    return FSkillData{};
}

bool USkillManagerComponent::IsSlotAssigned(int32 SlotIndex) const
{
    return SlotInstances.IsValidIndex(SlotIndex) && SlotInstances[SlotIndex] != nullptr;
}

FName USkillManagerComponent::GetSlotRowName(int32 SlotIndex) const
{
    if (SlotInstances.IsValidIndex(SlotIndex) && SlotInstances[SlotIndex])
        return SlotInstances[SlotIndex]->SkillRowName;
    return NAME_None;
}

int32 USkillManagerComponent::FindFirstEmptySlot() const
{
    for (int32 i = 0; i < 8; i++)
    {
        if (!SlotInstances.IsValidIndex(i) || !SlotInstances[i])
            return i;
    }
    return -1;
}

UTexture2D* USkillManagerComponent::GetSlotIcon(int32 SlotIndex) const
{
    if (SlotInstances.IsValidIndex(SlotIndex) && SlotInstances[SlotIndex])
        return SlotInstances[SlotIndex]->SkillData.Icon.LoadSynchronous();

    // 인스턴스 없이 DT만 쓰는 슬롯 (대시 등) 처리
    if (SlotIndex == DashSlotIndex && SkillDataTable && !DashRowName.IsNone())
    {
        if (FSkillData* Row = SkillDataTable->FindRow<FSkillData>(DashRowName, TEXT("")))
            return Row->Icon.LoadSynchronous();
    }

    return nullptr;
}

// ─── 스킬트리 ───────────────────────────────────────────────────────────────

int32 USkillManagerComponent::GetSkillLevel(FName RowName) const
{
    const int32* Level = SkillLevels.Find(RowName);
    return Level ? *Level : 0;
}

bool USkillManagerComponent::LevelUpSkill(FName RowName)
{
    if (!SkillDataTable) return false;
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, TEXT(""));
    if (!Data) return false;

    const int32 Current = GetSkillLevel(RowName);
    if (Current >= Data->MaxSkillLevel) return false;
    if (AvailableSkillPoints < Data->SkillPointCostPerLevel) return false;

    SkillLevels.FindOrAdd(RowName) = Current + 1;
    AvailableSkillPoints -= Data->SkillPointCostPerLevel;
    return true;
}

bool USkillManagerComponent::LevelDownSkill(FName RowName)
{
    if (!SkillDataTable) return false;
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, TEXT(""));
    if (!Data) return false;

    const int32 Current = GetSkillLevel(RowName);
    if (Current <= 0) return false;

    SkillLevels[RowName] = Current - 1;
    AvailableSkillPoints += Data->SkillPointCostPerLevel;
    return true;
}

TArray<FName> USkillManagerComponent::GetAllSkillRowNames() const
{
    if (SkillDataTable)
        return SkillDataTable->GetRowNames();
    return {};
}

bool USkillManagerComponent::AssignSkillToSlot(FName RowName, int32 SlotIndex)
{
    if (SlotIndex < 0 || SlotIndex >= 8) return false;
    if (!SkillDataTable) return false;

    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(RowName, TEXT(""));
    if (!Data || !Data->SkillClass) return false;

    if (!SlotClasses.IsValidIndex(SlotIndex))
        SlotClasses.SetNum(8);
    SlotClasses[SlotIndex] = Data->SkillClass;

    USkillBase* NewInstance = NewObject<USkillBase>(GetOwner(), Data->SkillClass);
    NewInstance->SkillData = *Data;
    NewInstance->SkillRowName = RowName;
    SlotInstances[SlotIndex] = NewInstance;

    CooldownEndTimes[SlotIndex] = 0.0f;
    CooldownDurations[SlotIndex] = Data->Cooldown;

    OnSkillSlotChanged.Broadcast(SlotIndex, RowName);
    return true;
}

// ────────────────────────────────────────────────────────────────────────────

void USkillManagerComponent::ResetCombo()
{
    if (ComboSlotIndex != -1 && ComboStep > 0)
    {
        FString Label = TEXT("콤보");
        if (SlotInstances.IsValidIndex(ComboSlotIndex) && SlotInstances[ComboSlotIndex])
        {
            const FText& Name = SlotInstances[ComboSlotIndex]->SkillData.SkillName;
            if (!Name.IsEmpty()) Label = Name.ToString();
        }
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
            FString::Printf(TEXT("[%s] 시간 초과 — %d단계에서 리셋"), *Label, ComboStep));
    }
    ComboSlotIndex = -1;
    ComboStep = 0;
}

