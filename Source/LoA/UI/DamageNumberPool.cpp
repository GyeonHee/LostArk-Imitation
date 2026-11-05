#include "UI/DamageNumberPool.h"
#include "UI/DamageNumberActor.h"
#include "Engine/World.h"

bool UDamageNumberPool::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

ADamageNumberActor* UDamageNumberPool::SpawnPooled(TSubclassOf<ADamageNumberActor> Class, const FVector& Location, AActor* Owner)
{
	UWorld* World = GetWorld();
	if (!World || !Class) return nullptr;

	// BeginPlay가 수명 타이머를 걸지 않도록 스폰을 끝내기 전에 풀 소속으로 표시한다
	ADamageNumberActor* Number = World->SpawnActorDeferred<ADamageNumberActor>(
		Class, FTransform(Location), Owner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Number) return nullptr;
	Number->bPooled = true;
	Number->FinishSpawning(FTransform(Location));
	return Number;
}

ADamageNumberActor* UDamageNumberPool::Acquire(TSubclassOf<ADamageNumberActor> Class, const FVector& Location, AActor* Owner)
{
	ADamageNumberActor* Number = nullptr;
	for (int32 i = FreeNumbers.Num() - 1; i >= 0 && !Number; --i)
	{
		ADamageNumberActor* Candidate = FreeNumbers[i];
		if (!IsValid(Candidate))
		{
			FreeNumbers.RemoveAtSwap(i);
		}
		else if (Candidate->GetClass() == *Class)
		{
			Number = Candidate;
			FreeNumbers.RemoveAtSwap(i);
		}
	}
	if (!Number)
	{
		Number = SpawnPooled(Class, Location, Owner);
	}
	if (Number)
	{
		Number->BeginUse(Location, Owner);
	}
	return Number;
}

void UDamageNumberPool::Release(ADamageNumberActor* Number)
{
	if (!IsValid(Number)) return;
	Number->EndUse();
	FreeNumbers.Add(Number);
}

void UDamageNumberPool::Prewarm(TSubclassOf<ADamageNumberActor> Class, int32 Count, AActor* Owner)
{
	for (int32 i = 0; i < Count; ++i)
	{
		Release(SpawnPooled(Class, FVector::ZeroVector, Owner));
	}
}
