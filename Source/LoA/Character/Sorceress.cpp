#include "Character/Sorceress.h"

ASorceress::ASorceress()
{
}

void ASorceress::ExecuteDash_Implementation(const FVector& TargetLocation)
{
	// 소서리스: 순간이동 (블링크)
	// TODO: 블링크 거리 제한, 장애물 체크, 이펙트 추가
	const FVector Dir = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) return;

	const FSkillData DashData = SkillManager->GetSlotSkillData(USkillManagerComponent::DashSlotIndex);
	const FVector BlinkDestination = GetActorLocation() + Dir * DashData.DashImpulse;
	SetActorLocation(BlinkDestination, true);
}

void ASorceress::AddIdentityOrb(int32 Count)
{
	IdentityOrbs = FMath::Clamp(IdentityOrbs + Count, 0, MaxIdentityOrbs);
	OnIdentityOrbsChanged.Broadcast(IdentityOrbs);
}

void ASorceress::ActivateIdentity()
{
	if (IdentityOrbs <= 0) return;

	// TODO: 구슬 수에 따른 아케인 토렌트 효과 (스킬 시스템과 연동)
	IdentityOrbs = 0;
	OnIdentityOrbsChanged.Broadcast(IdentityOrbs);
}

float ASorceress::GetIdentityGaugePercent() const
{
	return static_cast<float>(IdentityOrbs) / static_cast<float>(MaxIdentityOrbs);
}
