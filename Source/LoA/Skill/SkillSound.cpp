#include "Skill/SkillSound.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "LoAGameInstance.h"

namespace LoASkillSound
{
	USoundBase* Load(const TCHAR* Id)
	{
		if (!Id) return nullptr;
		const FString Path = FString::Printf(
			TEXT("/Game/LostArk/Character/Magician/Sorceress/Sound/SORCERESS_F_%s.SORCERESS_F_%s"), Id, Id);
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
		if (!Sound) UE_LOG(LogTemp, Warning, TEXT("[SkillSound] 없음: %s"), *Path);
		return Sound;
	}

	UAudioComponent* PlayAttached(AActor* Owner, const TCHAR* Id)
	{
		if (!IsValid(Owner) || !Owner->GetRootComponent()) return nullptr;
		const float Volume = ULoAGameInstance::GetSfxScale(Owner);  // 게임 설정 효과음 볼륨
		if (Volume <= 0.f) return nullptr;
		USoundBase* Sound = Load(Id);
		if (!Sound) return nullptr;
		return UGameplayStatics::SpawnSoundAttached(Sound, Owner->GetRootComponent(), NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, false, Volume);
	}

	void PlayAt(const UObject* WorldContext, const TCHAR* Id, const FVector& Location)
	{
		if (!WorldContext) return;
		const float Volume = ULoAGameInstance::GetSfxScale(WorldContext);
		if (Volume <= 0.f) return;
		if (USoundBase* Sound = Load(Id))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume);
		}
	}
}
