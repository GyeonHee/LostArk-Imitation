#include "Raid/RaidSound.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "LoAGameInstance.h"

namespace
{
	/** 같은 소리가 이번 프레임에 이미 나갔는지 — 처음이면 기록하고 false */
	bool RaidSound_PlayedThisFrame(const TCHAR* Folder, const TCHAR* Name)
	{
		static TMap<FString, uint64> LastFrame;
		const FString Key = FString::Printf(TEXT("%s/%s"), Folder, Name);
		uint64& Frame = LastFrame.FindOrAdd(Key, MAX_uint64);
		if (Frame == GFrameCounter) return true;
		Frame = GFrameCounter;
		return false;
	}
}

namespace LoARaidSound
{
	USoundBase* Load(const TCHAR* Folder, const TCHAR* Name)
	{
		if (!Folder || !Name) return nullptr;
		const FString Path = FString::Printf(TEXT("/Game/LostArk/Raid/EchidnaModling/sound/%s/%s.%s"), Folder, Name, Name);
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
		if (!Sound) UE_LOG(LogTemp, Warning, TEXT("[RaidSound] 없음: %s"), *Path);
		return Sound;
	}

	UAudioComponent* PlayAttached(AActor* Owner, const TCHAR* Folder, const TCHAR* Name, bool bAutoDestroy)
	{
		if (!IsValid(Owner) || !Owner->GetRootComponent()) return nullptr;
		const float Volume = ULoAGameInstance::GetSfxScale(Owner);
		if (Volume <= 0.f || RaidSound_PlayedThisFrame(Folder, Name)) return nullptr;
		USoundBase* Sound = Load(Folder, Name);
		if (!Sound) return nullptr;
		return UGameplayStatics::SpawnSoundAttached(Sound, Owner->GetRootComponent(), NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, true, Volume, 1.f, 0.f, nullptr, nullptr, bAutoDestroy);
	}

	void PlayAt(const UObject* WorldContext, const TCHAR* Folder, const TCHAR* Name, const FVector& Location)
	{
		if (!WorldContext) return;
		const float Volume = ULoAGameInstance::GetSfxScale(WorldContext);
		if (Volume <= 0.f || RaidSound_PlayedThisFrame(Folder, Name)) return;
		if (USoundBase* Sound = Load(Folder, Name))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume);
		}
	}

	void KeepLooping(UAudioComponent* Comp)
	{
		if (Comp && !Comp->IsPlaying()) Comp->Play(0.f);
	}

	void Stop(UAudioComponent* Comp, float FadeOut)
	{
		if (!Comp) return;
		if (FadeOut > 0.f) Comp->FadeOut(FadeOut, 0.f);
		else Comp->Stop();
	}
}
