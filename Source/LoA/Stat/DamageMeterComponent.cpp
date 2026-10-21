#include "Stat/DamageMeterComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UDamageMeterComponent::UDamageMeterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDamageMeterComponent::IsSessionActive() const
{
	if (!HasSession()) return false;
	if (!bTrainingSession) return true;
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() - LastHitTime < IdleTimeout;
}

double UDamageMeterComponent::GetElapsedSeconds() const
{
	const UWorld* World = GetWorld();
	if (!HasSession() || !World) return 0.0;
	const double End = IsSessionActive() ? World->GetTimeSeconds() : LastHitTime;
	return FMath::Max(0.0, End - SessionStartTime);
}

double UDamageMeterComponent::GetDPS() const
{
	// 첫 타 직후 1초 미만이면 DPS가 터무니없이 커지므로 최소 1초로 나눈다
	return TotalDamage / FMath::Max(1.0, GetElapsedSeconds());
}

void UDamageMeterComponent::RecordHit(const FLoADamageEvent& Event)
{
	const UWorld* World = GetWorld();
	if (!World || Event.FinalDamage <= 0.f) return;
	const double Now = World->GetTimeSeconds();

	// 끝난 허수아비 세션 뒤의 첫 타 → 새 세션 (보스 세션은 끊기지 않음)
	if (HasSession() && !IsSessionActive())
	{
		ResetSession();
	}
	if (!HasSession())
	{
		SessionStartTime = Now;
		bTrainingSession = Event.bTrainingTarget;
	}
	LastHitTime = Now;

	FDamageMeterSkillEntry& Entry = Entries.FindOrAdd(Event.SkillRowName);
	Entry.SkillRowName = Event.SkillRowName;
	Entry.TotalDamage += Event.FinalDamage;
	Entry.Hits++;
	Entry.CritHits += Event.bCritical ? 1 : 0;
	Entry.MaxHit = FMath::Max(Entry.MaxHit, static_cast<double>(Event.FinalDamage));

	TotalDamage += Event.FinalDamage;
	TotalHits++;
	TotalCritHits += Event.bCritical ? 1 : 0;

	OnChanged.Broadcast();
}

void UDamageMeterComponent::ResetSession()
{
	Entries.Reset();
	TotalDamage = 0.0;
	TotalHits = 0;
	TotalCritHits = 0;
	SessionStartTime = -1.0;
	LastHitTime = -1.0;
	bTrainingSession = false;
	OnChanged.Broadcast();
}

TArray<FDamageMeterSkillEntry> UDamageMeterComponent::GetSortedEntries() const
{
	TArray<FDamageMeterSkillEntry> Result;
	Entries.GenerateValueArray(Result);
	Result.Sort([](const FDamageMeterSkillEntry& A, const FDamageMeterSkillEntry& B) { return A.TotalDamage > B.TotalDamage; });
	return Result;
}

void UDamageMeterComponent::StartDpsTest(float Seconds)
{
	UWorld* World = GetWorld();
	if (!World || Seconds <= 0.f) return;
	ResetSession();
	DpsTestSeconds = Seconds;
	World->GetTimerManager().SetTimer(DpsTestTimer, this, &UDamageMeterComponent::FinishDpsTest, Seconds, false);
	UE_LOG(LogTemp, Log, TEXT("[DPS] 측정 시작 — %.0f초 뒤 결과 기록"), Seconds);
}

void UDamageMeterComponent::FinishDpsTest()
{
	LogSession(*FString::Printf(TEXT("DpsTest %.0fs"), DpsTestSeconds));
}

void UDamageMeterComponent::LogSession(const TCHAR* Label) const
{
	const double Elapsed = GetElapsedSeconds();
	const double CritRate = TotalHits > 0 ? static_cast<double>(TotalCritHits) / TotalHits : 0.0;

	// 엑셀에 붙여넣기 좋게 CSV 한 줄: 라벨,전투시간,총피해,DPS,타수,치명률
	UE_LOG(LogTemp, Log, TEXT("[DPS] CSV,%s,%.1f,%.0f,%.0f,%d,%.4f"), Label, Elapsed, TotalDamage, GetDPS(), TotalHits, CritRate);
	for (const FDamageMeterSkillEntry& Entry : GetSortedEntries())
	{
		UE_LOG(LogTemp, Log, TEXT("[DPS]   %s — 피해 %.0f (%.1f%%) / 타수 %d / 치명 %d / 최대 %.0f"),
			Entry.SkillRowName.IsNone() ? TEXT("기타") : *Entry.SkillRowName.ToString(),
			Entry.TotalDamage, TotalDamage > 0.0 ? Entry.TotalDamage / TotalDamage * 100.0 : 0.0,
			Entry.Hits, Entry.CritHits, Entry.MaxHit);
	}
}
