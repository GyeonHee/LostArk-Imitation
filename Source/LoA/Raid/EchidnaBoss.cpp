#include "EchidnaBoss.h"

AEchidnaBoss::AEchidnaBoss()
{
	PrimaryActorTick.bCanEverTick = false;

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
