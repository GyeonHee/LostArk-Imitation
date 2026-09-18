#include "EchidnaBoss.h"
#include "GameFramework/CharacterMovementComponent.h"

AEchidnaBoss::AEchidnaBoss()
{
	PrimaryActorTick.bCanEverTick = false;

	// UCharacterMovementComponent 기본값(true)을 끔 — 켜져 있으면 LaunchCharacter로 물리 이동시킬 때마다
	// (예: 뒤로 빠지며 좌우장판의 후방 홉) 보스가 자기 이동 방향(예: 후방)을 향해 자동으로 회전해버려서,
	// 패턴 코드가 SetActorRotation으로 잡아둔 조준 방향과 무관하게 잠깐 홱 돌았다가 다시 스냅되는 것처럼
	// 보임 — 이 프로젝트의 모든 회전은 패턴 코드가 직접 SetActorRotation으로 관리하므로 자동 회전 불필요
	GetCharacterMovement()->bOrientRotationToMovement = false;

	BigPatternThresholds.Add(FBossPatternThreshold{ 210, TEXT("MirrorCounter") });
}

void AEchidnaBoss::BeginPlay()
{
	Super::BeginPlay();

	HP = MaxHP;
	LastBroadcastLine = GetCurrentLine();
}

int32 AEchidnaBoss::GetCurrentLine() const
{
	if (MaxHP <= 0.f || TotalLines <= 0)
	{
		return 0;
	}

	const float LineValue = MaxHP / static_cast<float>(TotalLines);
	return FMath::Clamp(FMath::CeilToInt(HP / LineValue), 0, TotalLines);
}

float AEchidnaBoss::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ReceiveDamage(Applied);
	return Applied;
}

void AEchidnaBoss::ReceiveDamage(float DamageAmount)
{
	HP = FMath::Clamp(HP - DamageAmount, 0.f, MaxHP);

	const int32 NewLine = GetCurrentLine();
	if (NewLine != LastBroadcastLine)
	{
		LastBroadcastLine = NewLine;
		OnLineChanged.Broadcast(NewLine);
	}
}

bool AEchidnaBoss::IsPatternTriggered(FName PatternName) const
{
	return TriggeredPatterns.Contains(PatternName);
}

void AEchidnaBoss::MarkPatternTriggered(FName PatternName)
{
	TriggeredPatterns.Add(PatternName);
}
