// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillTypes.h"
#include "Engine/TimerHandle.h"
#include "SkillBase.generated.h"

class UAnimSequence;

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

    /** 선딜 — 발동 애니메이션은 시작했고 효과(ExecuteDelay)는 아직 안 나간 남은 시간(초). 0 = 없음 */
    float GetPendingExecuteRemaining() const;

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

    // ── 캐릭터 애니메이션 (표 = SkillBase.cpp의 SkillAnim_Table, 원본 모델일 때만 재생) ──
    /** 캐스팅 시작(캐스팅 스킬) / 누르기 시작(홀드 스킬) — 커서 쪽을 보고, 캐스팅은 반복·홀드는 마지막 프레임 유지 */
    void PlaySkillCastAnim(AActor* Owner);
    /** 발동 순간 1회 — 여러 개면 번갈아(기본공격 1·2타). 같은 프레임에 두 번 불려도 한 번만.
     *  bFaceCursor = 커서 쪽으로 돈다(캐스팅 완료는 시작 때 이미 돌았으니 false, 목표가 따로 있으면 FaceSkillTarget 후 false) */
    void PlaySkillReleaseAnim(AActor* Owner, bool bFaceCursor = true);

    /** 발동 직전에 부른다 — 방향·발동 애니메이션을 처리하고, 표의 ExecuteDelay가 있으면 효과를 그만큼 미룬다.
     *  false면 지금은 효과를 내지 말 것(타이머가 나중에 Execute를 다시 부르고, 그때는 true). FaceTarget이 없으면 커서 쪽.
     *  USkillInstant::OnKeyDown이 부르므로 즉발 스킬은 신경 안 써도 됨 — OnKeyDown을 따로 둔 스킬(사거리 이동)만 Execute 첫 줄에서 */
    bool BeginSkillExecute(AActor* Owner, const FVector* FaceTarget);
    /** 발사 순간 정면으로 뿜는 VFX(SkillAnim_Table ReleaseVFX) — 없으면 아무것도 안 함 */
    void SpawnReleaseVFX(AActor* Owner);
    /** 지금 Execute가 미뤄졌다가 실행되는 중인지 — 그 사이 커서가 움직였을 수 있으니 누를 때 방향(액터 정면)을 쓸 것 */
    bool bLastExecuteDeferred = false;

    /** 이번 캐스팅에 걸리는 시간(신속 전) — DT CastTime에서 아이덴티티 감소(소서리스 마력 강화·해방 1초)를 뺀 값 */
    float GetEffectiveCastTime(const AActor* Owner) const;

    /** 캐릭터를 목표 지점 쪽(수평)으로 즉시 돌린다 — 마네킹이어도 동작 */
    static void FaceSkillTarget(AActor* Owner, const FVector& Target);
    static void FaceCursor(AActor* Owner);
    /** 캐스팅·홀드 취소·종료 — 캐스팅 애니메이션이 아직 재생 중일 때만 끊는다 */
    void StopSkillCastAnim(AActor* Owner);

    // 시전 진행 시간
    float ElapsedTime = 0.0f;

    // 현재 활성화 여부
    bool bIsActive = false;

private:
    void ResolveSkillAnims();

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CastAnim;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CastLoopAnim;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> HoldEndAnim;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAnimSequence>> ReleaseAnims;

    TArray<float> ReleaseStarts;
    TArray<float> ExecuteDelays;

    // 발사 순간 정면으로 뿜는 VFX(SkillAnim_Table ReleaseVFX — 기본공격 보라 입김). 위치 = 캡슐 중심 + 정면 × Forward + 위 × Height
    UPROPERTY(Transient)
    TObjectPtr<class UNiagaraSystem> ReleaseVFX;
    float ReleaseVFXForwardOffset = 60.f;
    float ReleaseVFXHeightOffset = 20.f;
    float PendingExecuteDelay = 0.f;
    float LastReleaseAnimRemaining = 0.f;  // 방금 튼 발동 애니메이션이 끝날 때까지(초)  // 방금 튼 발동 애니메이션의 ExecuteDelay
    bool bRunningDeferredExecute = false;
    FTimerHandle DeferredExecuteHandle;
    TWeakObjectPtr<AActor> DeferredExecuteOwner;
    void RunDeferredExecute();
    bool bSkillAnimsResolved = false;
    bool bScaleCastToCastTime = false;
    int32 NextReleaseAnim = 0;
    uint64 LastReleaseAnimFrame = 0;

    // ── 사운드 (표 = SkillBase.cpp의 SkillSound_Table, 마네킹이어도 재생) ──
    /** 캐스팅·홀드 시작 — 취소하면 끊는다 */
    void StartCastSounds(AActor* Owner);
    /** 발동 순간(즉발은 누를 때, 캐스팅은 완료 때) — 지연 있는 것은 타이머(CC·사망이면 취소) */
    void StartReleaseSounds(AActor* Owner);
    void StopCastSounds(AActor* Owner, float FadeOut);
    void ScheduleSkillSound(AActor* Owner, const TCHAR* Id, float Delay, bool bCastSound);
    int32 SoundRowIndex = INDEX_NONE;
    TArray<TWeakObjectPtr<class UAudioComponent>> CastSoundComps;
    TArray<FTimerHandle> CastSoundTimers;
    TArray<FTimerHandle> ReleaseSoundTimers;
};
