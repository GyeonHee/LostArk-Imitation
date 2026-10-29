#include "Character/Sorceress.h"
#include "Character/SorceressIdentityComponent.h"
#include "Components/CapsuleComponent.h"
#include "Stat/EngravingComponent.h"
#include "Engine/World.h"

ASorceress::ASorceress()
{
	Identity = CreateDefaultSubobject<USorceressIdentityComponent>(TEXT("Identity"));
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
	// 소서리스 스페이스: 짧은 순간이동 (거리 = DT 대시 행 DashImpulse)
	const FVector Dir = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) return;

	const FSkillData DashData = SkillManager->GetSlotSkillData(USkillManagerComponent::DashSlotIndex);
	const FVector BlinkDestination = GetActorLocation() + Dir * DashData.DashImpulse;
	SetActorLocation(BlinkDestination, true);
	if (Engravings) Engravings->NotifyMovementSkill();  // 추진력 (이동기)
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
	// TeleportTo는 보스와 겹치면 비켜 선 자리를 찾는다. 그래도 실패하면 그냥 옮긴다
	if (!TeleportTo(Dest, GetActorRotation()))
	{
		SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	}
	return true;
}

void ASorceress::ApplyKnockdown(const FVector& SourceLocation, float HopScale)
{
	if (Identity && Identity->IsSuperArmor()) return;
	Super::ApplyKnockdown(SourceLocation, HopScale);
}

void ASorceress::ApplyStagger()
{
	if (Identity && Identity->IsSuperArmor()) return;
	Super::ApplyStagger();
}

void ASorceress::ActivateIdentity()
{
	FText Reason;
	if (Identity) Identity->TryRelease(Reason);
}

float ASorceress::GetIdentityGaugePercent() const
{
	return Identity ? Identity->GetGauge() / LoASorceressIdentity::MaxGauge : 0.f;
}
