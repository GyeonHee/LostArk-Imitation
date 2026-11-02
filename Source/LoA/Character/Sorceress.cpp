#include "Character/Sorceress.h"
#include "Character/SorceressIdentityComponent.h"
#include "Raid/EchidnaMirrorWallActor.h"
#include "Item/AvatarComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Stat/EngravingComponent.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	UAnimSequence* SorceressAnim(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/LostArk/Character/Magician/Sorceress/Model/Anims/%s.%s"), Name, Name);
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(*Path);
		return Finder.Succeeded() ? Finder.Object : nullptr;
	}
}

ASorceress::ASorceress()
{
	Identity = CreateDefaultSubobject<USorceressIdentityComponent>(TEXT("Identity"));
	Avatar = CreateDefaultSubobject<UAvatarComponent>(TEXT("Avatar"));

	// 원본 모델 — 키 약 1.32m라 1.3배(약 1.72m), 정면 +X
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyFinder(
		TEXT("/Game/LostArk/Character/Magician/Sorceress/Model/Mesh/SK_Sorceress.SK_Sorceress"));
	if (BodyFinder.Succeeded()) BodyMeshOverride = BodyFinder.Object;
	BodyMeshScale = 1.3f;
	BodyMeshYaw = 0.f;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> StaffFinder(
		TEXT("/Game/LostArk/Character/Magician/Sorceress/Model/Weapon/SK_Sorceress_Staff.SK_Sorceress_Staff"));
	if (StaffFinder.Succeeded()) WeaponMesh = StaffFinder.Object;
	WeaponSocket = TEXT("b_weapon_rhand");

	IdleAnim = SorceressAnim(TEXT("idle_battle_1"));
	RunAnim = SorceressAnim(TEXT("run_battle_1"));
	KnockdownAirAnim = SorceressAnim(TEXT("knockdown"));
	KnockdownLandAnim = SorceressAnim(TEXT("knockdown_land"));
	TwistKnockdownAirAnim = SorceressAnim(TEXT("twistknockdown"));
	TwistKnockdownLandAnim = SorceressAnim(TEXT("twistknockdown_land"));
	KnockdownLoopAnim = SorceressAnim(TEXT("down"));
	BoundAnim = SorceressAnim(TEXT("bound"));
	BoundLandAnim = SorceressAnim(TEXT("bound_land"));
	BoundHitLandAnim = SorceressAnim(TEXT("bound_hit_land"));
	StandUpAnim = SorceressAnim(TEXT("resurrect_1"));
	InstantGetUpAnim = SorceressAnim(TEXT("sk_standup_1"));	// sk_standup_normal_1과 사실상 같음(끝 자세만 전투/평상)
	CharmAnim = SorceressAnim(TEXT("abn_stun_1"));
	StunAnim = SorceressAnim(TEXT("abn_stun_1"));
	StaggerAnim = SorceressAnim(TEXT("dmg_idle_1"));
	StaggerAltAnim = SorceressAnim(TEXT("dmg_idle_2"));
	DeathAnim = SorceressAnim(TEXT("dead_1"));
	DeathLoopAnim = SorceressAnim(TEXT("dead_1_loop"));
	JustGuardAnim = SorceressAnim(TEXT("evt_defence_loop_1"));
	DashAnim = SorceressAnim(TEXT("sk_movingillusion"));
	IdentityReleaseAnim = SorceressAnim(TEXT("sk_arcanereinforce"));
	BlinkAnim = SorceressAnim(TEXT("sk_arcaneshift"));

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> AmplifyFinder(
		TEXT("/Game/LostArk/Character/Magician/Sorceress/VFX/NS_SorceressMagickAmplify.NS_SorceressMagickAmplify"));
	if (AmplifyFinder.Succeeded()) MagickAmplifyVFX = AmplifyFinder.Object;
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ReleaseFinder(
		TEXT("/Game/LostArk/Character/Magician/Sorceress/VFX/NS_SorceressMagickRelease.NS_SorceressMagickRelease"));
	if (ReleaseFinder.Succeeded()) MagickReleaseVFX = ReleaseFinder.Object;
}

void ASorceress::BeginPlay()
{
	Super::BeginPlay();
	OnDied.AddWeakLambda(this, [this]()
	{
		if (Identity) Identity->ResetIdentity();
	});
}

void ASorceress::ExecuteDash_Implementation(const FVector& TargetLocation)
{
	// 소서리스 스페이스: 앞으로 짧게 이동 (거리 = DT 대시 행 DashImpulse). 애니메이션이 있으면 그 움직임 타이밍대로 밀고,
	// 없으면(마네킹) 예전처럼 순간이동
	const FVector Dir = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) return;

	const FSkillData DashData = SkillManager->GetSlotSkillData(USkillManagerComponent::DashSlotIndex);
	SetActorRotation(Dir.Rotation());
	if (DashAnim && GetLoAAnim())
	{
		StartAnimMove(DashAnim, Dir, DashData.DashImpulse, true, DashHideStartProgress, DashShowProgress);
	}
	else
	{
		SetActorLocation(GetActorLocation() + Dir * DashData.DashImpulse, true);
	}
	if (Engravings) Engravings->NotifyMovementSkill();  // 추진력 (이동기)
}

void ASorceress::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDashing) TickDash(DeltaSeconds);
	UpdateMagickVFX();
}

void ASorceress::UpdateMagickVFX()
{
	const ESorceressMagickState State = Identity ? Identity->GetState() : ESorceressMagickState::None;
	if (static_cast<uint8>(State) == MagickVFXState) return;
	MagickVFXState = static_cast<uint8>(State);

	// 상태가 바뀌면(강화·해방 시작, 끝, 사망 초기화) 이전 오라는 새 입자만 멈추고 남은 건 자연스럽게 사라지게
	if (MagickVFXComp)
	{
		MagickVFXComp->Deactivate();
		MagickVFXComp = nullptr;
	}
	// 방출 자체가 주먹 쥘 때 일어나므로(TryActivateIdentity) 상태가 생기면 바로 켠다
	UNiagaraSystem* System = State == ESorceressMagickState::Amplify ? MagickAmplifyVFX.Get()
		: State == ESorceressMagickState::Release ? MagickReleaseVFX.Get() : nullptr;
	if (!System || MagickVFXComp) return;
	// 몸 중심(캡슐 중심)에 붙여 따라다니게 — 발밑에 두면 몸에 가려 안 보였다. 반복 이미터라 상태가 끝날 때 Deactivate
	MagickVFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator, FVector(MagickVFXScale), EAttachLocation::KeepRelativeOffset, true,
		ENCPoolMethod::None);
	UE_LOG(LogTemp, Log, TEXT("[Identity] 마력 방출 VFX %s → %s"), *System->GetName(), MagickVFXComp ? TEXT("생성") : TEXT("실패"));
}

void ASorceress::StartAnimMove(UAnimSequence* Anim, const FVector& Dir, float Distance, bool bSweep, float HideStart, float Show)
{
	bDashing = true;
	bScriptedMoveLock = true;
	bMoveImmune = true;	// 대시·점멸 중엔 모든 피해·CC 면역
	MoveAnim = Anim;
	bMoveSweep = bSweep;
	MoveHideStart = HideStart;
	MoveShow = Show;
	DashStart = GetActorLocation();
	DashDir = Dir;
	DashDistance = Distance;
	DashElapsed = 0.f;
	DashDuration = Anim->GetPlayLength();
	GetCharacterMovement()->StopMovementImmediately();
	PlayStateAnim(Anim, false, 0.05f, true);	// 밀기는 SetActorLocation이라 속도 0 — 끝난 뒤 걸으면 남은 꼬리가 끊긴다
}

float ASorceress::GetDashAnimProgress(float Time) const
{
	// b_root가 시작 위치에서 얼마나 멀어졌는지 / 끝에서의 거리 — 축 방향과 무관하게 진행도만 쓴다
	const USkeleton* Skeleton = MoveAnim ? MoveAnim->GetSkeleton() : nullptr;
	const int32 BoneIndex = Skeleton ? Skeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("b_root")) : INDEX_NONE;
	if (BoneIndex == INDEX_NONE || DashDuration <= 0.f) return FMath::Clamp(Time / FMath::Max(DashDuration, 0.01f), 0.f, 1.f);

	auto RootAt = [this, BoneIndex](float T)
	{
		FTransform Out;
		MoveAnim->GetBoneTransform(Out, FSkeletonPoseBoneIndex(BoneIndex), FAnimExtractContext(static_cast<double>(T)), false);
		return Out.GetTranslation();
	};
	const FVector Start = RootAt(0.f);
	const float Total = FVector::Dist(Start, RootAt(DashDuration));
	if (Total < 1.f) return FMath::Clamp(Time / DashDuration, 0.f, 1.f);
	return FMath::Clamp(static_cast<float>(FVector::Dist(Start, RootAt(FMath::Min(Time, DashDuration)))) / Total, 0.f, 1.f);
}

void ASorceress::TickDash(float DeltaSeconds)
{
	// CC·사망이 끊으면 그 자리에서 멈춘다
	if (bIsDead || bIsKnockedDown || bIsStunned || bIsHeld || bIsPulled)
	{
		EndDash();
		return;
	}
	DashElapsed += DeltaSeconds;
	const float Progress = GetDashAnimProgress(DashElapsed);
	FVector Target = DashStart + DashDir * (DashDistance * Progress);
	Target.Z = GetActorLocation().Z;
	// 거울 카운터의 다가오는 거울 줄은 대시·점멸로도 못 넘는다 (충돌 없는 비주얼이라 sweep·점멸 검사에 안 걸렸음)
	bool bBlockedByMirrorRow = false;
	Target = AEchidnaMirrorWallActor::ClampMoveAcrossRows(GetWorld(), this, GetActorLocation(), Target, bBlockedByMirrorRow);
	FHitResult Hit;
	SetActorLocation(Target, bMoveSweep, bMoveSweep ? &Hit : nullptr);
	// 빠르게 나가는 동안 투명 → 도착 지점에서 다시 보임
	SetDashHidden(Progress >= MoveHideStart && Progress < MoveShow);
	if (Hit.bBlockingHit || bBlockedByMirrorRow || DashElapsed >= DashDuration || Progress >= 1.f)
	{
		EndDash();
	}
}

void ASorceress::EndDash()
{
	bDashing = false;
	bScriptedMoveLock = false;
	bMoveImmune = false;
	SetDashHidden(false);
	// 통과 이동(점멸)은 보스와 겹친 채 끝날 수 있다 — TeleportTo가 비켜 선 자리를 찾는다
	if (!bMoveSweep) TeleportTo(GetActorLocation(), GetActorRotation());
}

void ASorceress::SetDashHidden(bool bHide)
{
	if (bDashHidden == bHide) return;
	bDashHidden = bHide;
	// 몸 + 붙은 아바타 부위·무기 — 원래 꺼져 있던 부위(빈 아바타 칸)는 다시 켜지 않게, 숨긴 것만 기억해 되돌린다
	if (bHide)
	{
		DashHiddenMeshes.Reset();
		TArray<USceneComponent*> Parts;
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			// GetChildrenComponents는 배열을 먼저 비운다 — 몸은 그 뒤에 넣을 것(먼저 넣었더니 몸의 얼굴·머리카락만 남았다)
			Body->GetChildrenComponents(true, Parts);
			Parts.Add(Body);
		}
		for (USceneComponent* Part : Parts)
		{
			UMeshComponent* MeshPart = Cast<UMeshComponent>(Part);
			if (!MeshPart || !MeshPart->IsVisible()) continue;
			MeshPart->SetVisibility(false);
			DashHiddenMeshes.Add(MeshPart);
		}
		return;
	}
	for (const TWeakObjectPtr<UMeshComponent>& MeshPart : DashHiddenMeshes)
	{
		if (MeshPart.IsValid()) MeshPart->SetVisibility(true);
	}
	DashHiddenMeshes.Reset();
}

bool ASorceress::BlinkTo(const FVector& TargetLocation)
{
	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule) return false;

	const FVector Start = GetActorLocation();
	FVector Delta = TargetLocation - Start;
	Delta.Z = 0.f;
	FVector Dir = Delta.GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = GetActorForwardVector().GetSafeNormal2D();
	// 커서가 15m 안이면 커서까지, 밖이면 커서 방향 15m
	const float Distance = FMath::Clamp(static_cast<float>(Delta.Size()), 100.f, LoASorceressIdentity::BlinkDistance);
	FVector Dest = Start + Dir * Distance;

	// 벽에서 멈춘다 — WorldStatic만 보니 보스·몬스터(Pawn)는 넘어간다. 캡슐을 살짝 줄여 바닥에 닿은 채로 시작하지 않게
	const float Radius = Capsule->GetScaledCapsuleRadius() * 0.9f;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() * 0.8f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SorceressBlink), false, this);
	FHitResult Hit;
	if (World->SweepSingleByObjectType(Hit, Start, Dest, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) && !Hit.bStartPenetrating)
	{
		Dest = Hit.Location;
	}

	// 바닥이 없는 곳(부서진 타일·맵 밖)이면 출발점 쪽으로 1m씩 당긴다
	const float FootOffset = Capsule->GetScaledCapsuleHalfHeight();
	auto HasGround = [&](const FVector& Center)
	{
		FHitResult Ground;
		return World->LineTraceSingleByChannel(Ground, Center, Center - FVector(0.f, 0.f, FootOffset + 300.f), ECC_Visibility, Params);
	};
	while (!HasGround(Dest) && FVector::DistSquared2D(Dest, Start) > FMath::Square(100.f))
	{
		Dest -= Dir * 100.f;
	}

	SetActorRotation(Dir.Rotation());
	UE_LOG(LogTemp, Log, TEXT("[Blink] 커서 거리 %.1fm → 이동 %.1fm (최대 %.0fm)"),
		Delta.Size() / 100.f, FVector::Dist2D(Start, Dest) / 100.f, LoASorceressIdentity::BlinkDistance / 100.f);
	if (BlinkAnim && GetLoAAnim())
	{
		// 애니메이션 곡선대로 날아가며 빠른 구간은 안 보임 — 목적지는 위에서 벽·바닥 검사를 끝냈으니 통과 이동
		StartAnimMove(BlinkAnim, Dir, static_cast<float>(FVector::Dist2D(Start, Dest)), false, BlinkHideStartProgress, BlinkShowProgress);
		return true;
	}
	// 애니메이션이 없으면(마네킹) 순간이동 — TeleportTo는 보스와 겹치면 비켜 선 자리를 찾는다. 그래도 실패하면 그냥 옮긴다
	if (!TeleportTo(Dest, GetActorRotation()))
	{
		SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	}
	return true;
}

void ASorceress::ApplyKnockdown(const FVector& SourceLocation, float HopScale, bool bBlown)
{
	if (Identity && Identity->IsSuperArmor()) return;
	Super::ApplyKnockdown(SourceLocation, HopScale, bBlown);
}

void ASorceress::ApplyStagger()
{
	if (Identity && Identity->IsSuperArmor()) return;
	Super::ApplyStagger();
}

void ASorceress::ActivateIdentity()
{
	FText Reason;
	TryActivateIdentity(Reason);
}

bool ASorceress::TryActivateIdentity(FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	if (!Identity || IsIdentityReleasePending() || !Identity->CanRelease(OutFailReason)) return false;
	// 이동 중이어도 멈춰 서서 동작 — 걷는 속도가 남아 있으면 애니메이션이 바로 끊긴다
	GetCharacterMovement()->StopMovementImmediately();
	PlayStateAnim(IdentityReleaseAnim, false, 0.08f, true);
	// 동작이 끝날 때까지 클릭 이동 금지 — 안 막으면 걸으면서 Z가 나가고 동작이 바로 끊겼다(즉발 스킬과 같은 잠금)
	LockMoveForSkillAnim(IdentityReleaseAnim ? IdentityReleaseAnim->GetPlayLength() : MagickReleaseDelay);
	if (MagickReleaseDelay <= 0.f)
	{
		FinishIdentityRelease();
		return true;
	}
	GetWorldTimerManager().SetTimer(MagickReleaseTimer, this, &ASorceress::FinishIdentityRelease, MagickReleaseDelay, false);
	return true;
}

bool ASorceress::IsIdentityReleasePending() const
{
	return GetWorldTimerManager().IsTimerActive(MagickReleaseTimer);
}

void ASorceress::FinishIdentityRelease()
{
	// 주먹을 쥐기 전에 넉다운·기절·사망 등으로 끊겼으면 취소 — 게이지는 그대로
	if (!Identity || IsActionLocked()) return;
	FText Reason;
	if (Identity->TryRelease(Reason)) UpdateMagickVFX();	// 다음 틱을 기다리지 않고 바로
}

float ASorceress::GetCastTimeReduction() const
{
	return Identity && Identity->GetState() != ESorceressMagickState::None ? LoASorceressIdentity::CastTimeReduction : 0.f;
}

float ASorceress::GetIdentityGaugePercent() const
{
	return Identity ? Identity->GetGauge() / LoASorceressIdentity::MaxGauge : 0.f;
}
