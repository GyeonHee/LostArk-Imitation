// Fill out your copyright notice in the Description page of Project Settings.


#include "Skill/SkillBase.h"
#include "LoACharacter.h"
#include "LoAPlayerController.h"
#include "Character/LoACharacterAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "CoreGlobals.h"
#include "TimerManager.h"
#include "Stat/StatComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Character/MageCharacter.h"
#include "Character/SorceressIdentityComponent.h"
#include "Skill/SkillSound.h"
#include "Components/AudioComponent.h"

namespace
{
    /** 스킬 → 소서리스 원본 애니메이션(Model/Anims). 키 = 네이티브 스킬 클래스 이름, 기본공격은 BP라 SkillRowName.
     *  짝은 유저가 PIE에서 보고 정함. ReleaseStart = 스킬 효과가 즉시 나가므로 애니메이션의 발사 순간(원본 PSA에서 손 속도가
     *  가장 빠른 프레임) 0.1초 전부터 튼다 — 블레이즈 0.30 · 인페르노 0.60 · 아이스 에로우 1.20 · 돌풍 0.47 · 기본공격 0.07/0.27초.
     *  인페르노·아이스 에로우·돌풍은 유저 요청으로 반대 — 애니메이션을 처음부터 틀고 효과를 발사 순간까지 미룬다(ExecuteDelay) */
    struct FSkillAnimRow
    {
        const TCHAR* Key;
        const TCHAR* Cast;          // 캐스팅·홀드 시작 (없으면 nullptr)
        const TCHAR* CastLoop;      // 홀드 — Cast 다음 반복 (없으면 Cast를 반복)
        const TCHAR* HoldEnd;       // 홀드를 뗄 때 (없으면 그냥 끊음)
        bool bScaleCastToCastTime;  // 캐스팅 — 재생 속도를 캐스팅 시간(신속 반영)에 맞춰 캐스팅 바가 찰 때 끝나게
        const TCHAR* Release[2];    // 발동 순간 (번갈아)
        float ReleaseStart[2];
        float ExecuteDelay[2];      // Release별 — 애니메이션을 처음부터 틀고 효과를 발사 순간(이 시각)까지 미룸
        const TCHAR* ReleaseVFX;    // 발사 순간 캐릭터 앞에서 정면으로 뿜는 나이아가라 경로(없으면 생략 — 뒤 멤버라 안 쓰는 행은 0)
    };

    const FSkillAnimRow SkillAnim_Table[] = {
        { TEXT("SkillDoomsday"),  TEXT("sk_meteor_01"),      nullptr, nullptr, true,  { TEXT("sk_meteor_02"), nullptr },      { 0.f, 0.f }, { 0.f, 0.f } },
        { TEXT("SkillCheonbeol"), TEXT("sk_thunderbolt_01"), nullptr, nullptr, true,  { TEXT("sk_thunderbolt_02"), nullptr }, { 0.f, 0.f }, { 0.f, 0.f } },
        { TEXT("SkillExplosion"), TEXT("sk_greatfire_01"),   nullptr, nullptr, true,  { TEXT("sk_greatfire_02"), nullptr },   { 0.f, 0.f }, { 0.f, 0.f } },
        { TEXT("SkillBlaze"),     nullptr, nullptr, nullptr, false, { TEXT("sk_firewall"), nullptr },    { 0.f, 0.f }, { 0.30f, 0.f } },
        { TEXT("SkillInferno"),   nullptr, nullptr, nullptr, false, { TEXT("sk_flamestrike"), nullptr }, { 0.f, 0.f }, { 0.60f, 0.f } },
        { TEXT("SkillIceArrow"),  nullptr, nullptr, nullptr, false, { TEXT("sk_icearrow"), nullptr },    { 0.f, 0.f }, { 1.20f, 0.f } },
        { TEXT("SkillGust"),      nullptr, nullptr, nullptr, false, { TEXT("sk_windstrike"), nullptr },  { 0.f, 0.f }, { 0.47f, 0.f } },
        { TEXT("SkillFrostCall"), TEXT("sk_flashfrost_01"), TEXT("sk_flashfrost_02"), TEXT("sk_flashfrost_03"), false, { nullptr, nullptr }, { 0.f, 0.f }, { 0.f, 0.f } },
        { TEXT("BasicAttack"),    nullptr, nullptr, nullptr, false, { TEXT("att_battle_1_01"), TEXT("att_battle_1_02") }, { 0.f, 0.f }, { 0.07f, 0.27f },
          TEXT("/Game/LostArk/Character/Magician/Sorceress/VFX/NS_SorceressBasicAttackMist.NS_SorceressBasicAttackMist") },
    };

    /** 스킬 → 소서리스 원본 사운드 번호(SORCERESS_F_<번호>). 키는 SkillAnim_Table과 같다. 짝은 유저가 원본을 듣고 정함.
     *  Delay = 시작 시점부터 초. SkillSound_AfterCastAnim = 캐스팅(홀드) 시작 애니메이션이 끝날 때(혹한 반복 구간) */
    constexpr float SkillSound_AfterCastAnim = -1.f;
    struct FSkillSoundCue
    {
        const TCHAR* Id;
        float Delay;
    };
    struct FSkillSoundRow
    {
        const TCHAR* Key;
        FSkillSoundCue Cast[2];     // 캐스팅·홀드 시작 — 취소·홀드 종료면 끊음
        FSkillSoundCue Release[5];  // 발동 순간
        bool bRandomRelease;        // Release 중 하나만 무작위(기본공격)
        bool bStopCastOnRelease;    // 발동하면 캐스팅 소리를 끊음(차징음 → 발사음). false면 끝까지(종말의 날 068)
    };

    const FSkillSoundRow SkillSound_Table[] = {
        // 종말의 날: 068 캐스팅부터 낙하까지 이어짐 + 장판이 깔릴 때 131(DoomsdayMeteor)
        { TEXT("SkillDoomsday"),  { { TEXT("068"), 0.12f } }, {}, false, false },  // 소리가 운석보다 살짝 빨라 늦춤(유저 PIE)
        // 천벌: 173 캐스팅, 174 내리칠 때(LightningStrikeActor)
        { TEXT("SkillCheonbeol"), { { TEXT("173"), 0.f } }, {}, false, true },
        // 익스플로전: 170 차징, 171 발사
        { TEXT("SkillExplosion"), { { TEXT("170"), 0.f } }, { { TEXT("171"), 0.f } }, false, true },
        { TEXT("SkillBlaze"),     {}, { { TEXT("044"), 0.f } }, false, false },
        // 인페르노: 049 바로 뒤 050(거의 동시), 053 폭발(InfernoZoneActor)
        { TEXT("SkillInferno"),   {}, { { TEXT("049"), 0.f }, { TEXT("050"), 0.15f } }, false, false },
        { TEXT("SkillIceArrow"),  {}, { { TEXT("074"), 0.f } }, false, false },
        // 돌풍: 227 기 모을 때(동작 시작), 229 장풍 발사(ExecuteDelay 0.47과 같게)
        { TEXT("SkillGust"),      {}, { { TEXT("227"), 0.f }, { TEXT("229"), 0.47f } }, false, false },
        // 혹한의 부름: 023 누를 때, 026 반복 구간부터 — 떼면 둘 다 끊음
        { TEXT("SkillFrostCall"), { { TEXT("023"), 0.f }, { TEXT("026"), SkillSound_AfterCastAnim } }, {}, false, false },
        { TEXT("BasicAttack"),    {}, { { TEXT("001"), 0.f }, { TEXT("002"), 0.f }, { TEXT("003"), 0.f }, { TEXT("004"), 0.f }, { TEXT("005"), 0.f } }, true, false },
    };

    UAnimSequence* SkillAnim_Load(const TCHAR* Name)
    {
        if (!Name) return nullptr;
        const FString Path = FString::Printf(TEXT("/Game/LostArk/Character/Magician/Sorceress/Model/Anims/%s.%s"), Name, Name);
        return LoadObject<UAnimSequence>(nullptr, *Path);
    }

    ULoACharacterAnimInstance* SkillAnim_GetAnim(AActor* Owner)
    {
        const ALoACharacter* Char = Cast<ALoACharacter>(Owner);
        return Char ? Char->GetLoAAnim() : nullptr;
    }
}

void USkillBase::ResolveSkillAnims()
{
    if (bSkillAnimsResolved) return;
    bSkillAnimsResolved = true;

    // 키: 기본공격(BP, SkillInstant 그대로)은 행 이름, 나머지는 BP가 상속한 네이티브 스킬 클래스
    FString Key = SkillRowName.ToString();
    for (const UClass* Class = GetClass(); Class; Class = Class->GetSuperClass())
    {
        if (Class->HasAnyClassFlags(CLASS_Native))
        {
            if (Key != TEXT("BasicAttack")) Key = Class->GetName();
            break;
        }
    }
    for (int32 i = 0; i < UE_ARRAY_COUNT(SkillSound_Table); ++i)
    {
        if (Key == SkillSound_Table[i].Key) { SoundRowIndex = i; break; }
    }
    for (const FSkillAnimRow& Row : SkillAnim_Table)
    {
        if (Key != Row.Key) continue;
        CastAnim = SkillAnim_Load(Row.Cast);
        CastLoopAnim = SkillAnim_Load(Row.CastLoop);
        HoldEndAnim = SkillAnim_Load(Row.HoldEnd);
        bScaleCastToCastTime = Row.bScaleCastToCastTime;
        for (int32 i = 0; i < UE_ARRAY_COUNT(Row.Release); ++i)
        {
            if (UAnimSequence* Anim = SkillAnim_Load(Row.Release[i]))
            {
                ReleaseAnims.Add(Anim);
                ReleaseStarts.Add(Row.ReleaseStart[i]);
                ExecuteDelays.Add(Row.ExecuteDelay[i]);
            }
        }
        if (Row.ReleaseVFX) ReleaseVFX = Cast<UNiagaraSystem>(FSoftObjectPath(Row.ReleaseVFX).TryLoad());
        break;
    }
}

float USkillBase::GetEffectiveCastTime(const AActor* Owner) const
{
    const AMageCharacter* Mage = Cast<AMageCharacter>(Owner);
    const float Reduction = Mage ? Mage->GetCastTimeReduction() : 0.f;
    if (Reduction <= 0.f || SkillData.CastTime <= 0.f) return SkillData.CastTime;
    return FMath::Max(LoASorceressIdentity::MinCastTime, SkillData.CastTime - Reduction);
}

bool USkillBase::BeginSkillExecute(AActor* Owner, const FVector* FaceTarget)
{
    // 타이머가 다시 부른 실제 발동 — 방향·애니메이션은 누를 때 이미 처리했다
    if (bRunningDeferredExecute) return true;
    bLastExecuteDeferred = false;

    // 앞서 미뤄 둔 발동이 아직 안 나갔으면(기본공격 연타 등) 지금 바로 내보낸다 — 새 타이머가 덮어써 사라지지 않게
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (World && World->GetTimerManager().IsTimerActive(DeferredExecuteHandle))
    {
        World->GetTimerManager().ClearTimer(DeferredExecuteHandle);
        RunDeferredExecute();
        bLastExecuteDeferred = false;
    }

    PendingExecuteDelay = 0.f;
    LastReleaseAnimRemaining = 0.f;
    if (FaceTarget)
    {
        FaceSkillTarget(Owner, *FaceTarget);
        PlaySkillReleaseAnim(Owner, false);
    }
    else
    {
        PlaySkillReleaseAnim(Owner, true);
    }

    // 즉발 스킬·기본공격 — 발동 애니메이션이 끝날 때까지 클릭 이동 금지
    if (ALoACharacter* Character = Cast<ALoACharacter>(Owner))
    {
        Character->LockMoveForSkillAnim(LastReleaseAnimRemaining);
    }

    // 애니메이션이 실제로 나왔을 때만 미룬다(마네킹 등 애니메이션이 없으면 예전처럼 즉시)
    if (PendingExecuteDelay <= 0.f || !World) return true;

    DeferredExecuteOwner = Owner;
    World->GetTimerManager().SetTimer(DeferredExecuteHandle,
        FTimerDelegate::CreateUObject(this, &USkillBase::RunDeferredExecute), PendingExecuteDelay, false);
    return false;
}

float USkillBase::GetPendingExecuteRemaining() const
{
    const UWorld* World = GetWorld();
    if (!World && DeferredExecuteOwner.IsValid()) World = DeferredExecuteOwner->GetWorld();
    if (!World || !World->GetTimerManager().IsTimerActive(DeferredExecuteHandle)) return 0.f;
    return World->GetTimerManager().GetTimerRemaining(DeferredExecuteHandle);
}

void USkillBase::RunDeferredExecute()
{
    AActor* Caster = DeferredExecuteOwner.Get();
    DeferredExecuteOwner.Reset();
    if (!Caster) return;
    bRunningDeferredExecute = true;
    bLastExecuteDeferred = true;
    SpawnReleaseVFX(Caster);
    Execute(Caster);
    bRunningDeferredExecute = false;
}

void USkillBase::SpawnReleaseVFX(AActor* Owner)
{
    ResolveSkillAnims();
    if (!ReleaseVFX || !IsValid(Owner)) return;
    // 캐릭터가 바라보는 쪽으로(발동 때 커서 쪽으로 돌았다) — 가슴 높이, 몸 바로 앞
    const FRotator Facing(0.f, Owner->GetActorRotation().Yaw, 0.f);
    const FVector Location = Owner->GetActorLocation() + Facing.Vector() * ReleaseVFXForwardOffset + FVector(0.f, 0.f, ReleaseVFXHeightOffset);
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(Owner, ReleaseVFX, Location, Facing);
}

void USkillBase::FaceSkillTarget(AActor* Owner, const FVector& Target)
{
    if (!IsValid(Owner)) return;
    FVector Dir = Target - Owner->GetActorLocation();
    Dir.Z = 0.f;
    if (Dir.IsNearlyZero()) return;
    Owner->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
}

void USkillBase::FaceCursor(AActor* Owner)
{
    const APawn* Pawn = Cast<APawn>(Owner);
    const ALoAPlayerController* PC = Pawn ? Cast<ALoAPlayerController>(Pawn->GetController()) : nullptr;
    FVector Aim;
    if (PC && PC->GetCursorAimLocation(Aim)) FaceSkillTarget(Owner, Aim);
}

void USkillBase::ScheduleSkillSound(AActor* Owner, const TCHAR* Id, float Delay, bool bCastSound)
{
    if (!Id || !IsValid(Owner)) return;
    auto Play = [this, Owner, Id, bCastSound]()
    {
        UAudioComponent* Comp = LoASkillSound::PlayAttached(Owner, Id);
        if (Comp && bCastSound) CastSoundComps.Add(Comp);
    };
    UWorld* World = Owner->GetWorld();
    if (Delay <= 0.f || !World)
    {
        Play();
        return;
    }
    FTimerHandle& Handle = (bCastSound ? CastSoundTimers : ReleaseSoundTimers).AddDefaulted_GetRef();
    TWeakObjectPtr<USkillBase> WeakThis(this);
    TWeakObjectPtr<AActor> WeakOwner(Owner);
    World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakThis, WeakOwner, Id, bCastSound]()
    {
        if (USkillBase* Self = WeakThis.Get())
        {
            if (AActor* O = WeakOwner.Get()) Self->ScheduleSkillSound(O, Id, 0.f, bCastSound);
        }
    }), Delay, false);
}

void USkillBase::StartCastSounds(AActor* Owner)
{
    ResolveSkillAnims();
    StopCastSounds(Owner, 0.1f);  // 앞 캐스팅 소리가 남아 있으면(연속 시전) 정리
    if (SoundRowIndex == INDEX_NONE) return;
    for (const FSkillSoundCue& Cue : SkillSound_Table[SoundRowIndex].Cast)
    {
        if (!Cue.Id) continue;
        float Delay = Cue.Delay;
        // 시작 동작이 끝나고 반복 구간에 들어갈 때 — 애니메이션이 없으면(마네킹) 0.5초
        if (Delay == SkillSound_AfterCastAnim) Delay = CastAnim ? CastAnim->GetPlayLength() : 0.5f;
        ScheduleSkillSound(Owner, Cue.Id, Delay, true);
    }
}

void USkillBase::StartReleaseSounds(AActor* Owner)
{
    ResolveSkillAnims();
    if (SoundRowIndex == INDEX_NONE) return;
    const FSkillSoundRow& Row = SkillSound_Table[SoundRowIndex];

    // 캐스팅이 끝났다 — 차징음은 끊고, 이어지는 소리(종말의 날)는 더는 취소 대상이 아니다(낙하 중 CC여도 계속)
    if (Row.bStopCastOnRelease)
    {
        StopCastSounds(Owner, 0.25f);
    }
    else
    {
        if (UWorld* World = Owner ? Owner->GetWorld() : nullptr)
        {
            for (FTimerHandle& Handle : CastSoundTimers) World->GetTimerManager().ClearTimer(Handle);
        }
        CastSoundTimers.Reset();
        CastSoundComps.Reset();
    }

    TArray<const FSkillSoundCue*, TInlineAllocator<5>> Cues;
    for (const FSkillSoundCue& Cue : Row.Release)
    {
        if (Cue.Id) Cues.Add(&Cue);
    }
    if (Cues.Num() == 0) return;
    if (Row.bRandomRelease)
    {
        const FSkillSoundCue* Pick = Cues[FMath::RandRange(0, Cues.Num() - 1)];
        ScheduleSkillSound(Owner, Pick->Id, Pick->Delay, false);
        return;
    }
    for (const FSkillSoundCue* Cue : Cues) ScheduleSkillSound(Owner, Cue->Id, Cue->Delay, false);
}

void USkillBase::StopCastSounds(AActor* Owner, float FadeOut)
{
    UWorld* World = Owner ? Owner->GetWorld() : GetWorld();
    for (const TWeakObjectPtr<UAudioComponent>& Comp : CastSoundComps)
    {
        if (!Comp.IsValid()) continue;
        if (!World) World = Comp->GetWorld();
        Comp->FadeOut(FadeOut, 0.f);
    }
    CastSoundComps.Reset();
    if (World)
    {
        for (FTimerHandle& Handle : CastSoundTimers) World->GetTimerManager().ClearTimer(Handle);
    }
    CastSoundTimers.Reset();
}

void USkillBase::PlaySkillCastAnim(AActor* Owner)
{
    FaceCursor(Owner);
    ResolveSkillAnims();
    StartCastSounds(Owner);
    ULoACharacterAnimInstance* Anim = SkillAnim_GetAnim(Owner);
    if (!Anim || !CastAnim) return;
    FLoAAnimStep Step;
    Step.Sequence = CastAnim;
    Step.bRootMotion = true;
    if (bScaleCastToCastTime && SkillData.CastTime > 0.f)
    {
        // 캐스팅 바가 다 차는 순간 끝나게 — 신속으로 캐스팅이 빨라지면 같이 빨라진다. 끝나면 마지막 프레임에서 발동 동작으로
        const float CastDuration = GetEffectiveCastTime(Owner) / FMath::Max(0.1f, UStatComponent::GetCastSpeedMultiplierOf(Owner));
        Step.PlayRate = CastAnim->GetPlayLength() / FMath::Max(0.05f, CastDuration);
        Step.bHoldLastFrame = true;
        Anim->PlayActionChain({ Step }, 0.12f);
        return;
    }
    if (CastLoopAnim)
    {
        // 홀드: 시작 동작 → 유지 동작 반복
        FLoAAnimStep Loop;
        Loop.Sequence = CastLoopAnim;
        Loop.bLoop = true;
        Loop.bRootMotion = true;
        Anim->PlayActionChain({ Step, Loop }, 0.12f);
        return;
    }
    Step.bLoop = true;
    Anim->PlayActionChain({ Step }, 0.12f);
}

void USkillBase::PlaySkillReleaseAnim(AActor* Owner, bool bFaceCursor)
{
    // 즉발은 OnKeyDown과 Execute 양쪽에서 부를 수 있다 — 같은 프레임 두 번째는 무시(먼저 온 쪽의 방향이 맞다)
    if (LastReleaseAnimFrame == GFrameCounter) return;
    LastReleaseAnimFrame = GFrameCounter;
    StartReleaseSounds(Owner);
    if (bFaceCursor) FaceCursor(Owner);

    ResolveSkillAnims();
    ULoACharacterAnimInstance* Anim = SkillAnim_GetAnim(Owner);
    if (!Anim || ReleaseAnims.Num() == 0) return;
    const int32 Index = NextReleaseAnim % ReleaseAnims.Num();
    NextReleaseAnim = (NextReleaseAnim + 1) % ReleaseAnims.Num();
    // 발사 순간 직전부터 — 끝나면 대기·달리기로, 도중에 움직이면 바로 끊는다
    FLoAAnimStep Step;
    Step.Sequence = ReleaseAnims[Index];
    Step.StartTime = ReleaseStarts.IsValidIndex(Index) ? ReleaseStarts[Index] : 0.f;
    PendingExecuteDelay = ExecuteDelays.IsValidIndex(Index) ? ExecuteDelays[Index] : 0.f;
    LastReleaseAnimRemaining = FMath::Max(0.f, Step.Sequence->GetPlayLength() - Step.StartTime);
    Step.bRootMotion = true;
    Anim->PlayActionChain({ Step }, 0.06f, true);
}

void USkillBase::StopSkillCastAnim(AActor* Owner)
{
    StopCastSounds(Owner, 0.2f);
    ULoACharacterAnimInstance* Anim = SkillAnim_GetAnim(Owner);
    if (!Anim) return;
    const UAnimSequence* Current = Anim->GetCurrentActionSequence();
    if (!Current || (Current != CastAnim && Current != CastLoopAnim)) return;  // 이미 다른 동작(CC 등)이 덮었다
    if (HoldEndAnim)
    {
        // 홀드를 뗄 때 마무리 동작 — 움직이면 끊는다
        FLoAAnimStep End;
        End.Sequence = HoldEndAnim;
        End.bRootMotion = true;
        Anim->PlayActionChain({ End }, 0.1f, true);
        return;
    }
    Anim->StopAction(0.15f, Current);
}

void USkillBase::OnKeyDown(AActor* Owner)
{
}

void USkillBase::OnKeyHeld(AActor* Owner, float DeltaTime)
{
}

void USkillBase::OnKeyUp(AActor* Owner)
{
}


void USkillBase::ForceCancel(AActor* Owner)
{
    bIsActive = false;
    ElapsedTime = 0.f;
    StopSkillCastAnim(Owner);
    if (ALoACharacter* Character = Cast<ALoACharacter>(Owner)) Character->ClearSkillAnimMoveLock();
    // 미뤄 둔 효과도 취소(CC·사망)
    if (UWorld* World = Owner ? Owner->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(DeferredExecuteHandle);
        // 아직 안 나온 발동 소리(돌풍 229 등)도 — 효과가 취소됐다
        for (FTimerHandle& Handle : ReleaseSoundTimers) World->GetTimerManager().ClearTimer(Handle);
    }
    ReleaseSoundTimers.Reset();
}

void USkillBase::Execute_Implementation(AActor* Owner)
{
}
