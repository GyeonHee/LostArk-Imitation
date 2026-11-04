#include "Raid/RaidSound.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "LoAGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

namespace
{
	TAutoConsoleVariable<float> CVarRaidSoundGain(
		TEXT("LoA.RaidSound.Gain"), 2.5f,
		TEXT("거울 사운드 볼륨 배율 — 원본이 스킬 사운드보다 작게 들려서 키움(엔진 상한 4). 다음 재생부터 적용"));

	TAutoConsoleVariable<float> CVarRaidSoundPatternGain(
		TEXT("LoA.RaidSound.PatternGain"), 1.f,
		TEXT("보스 패턴 사운드(RaidCue) 볼륨 배율 — 거울 배율을 그대로 쓰면 너무 커서 따로 둠. 다음 재생부터 적용"));

	/** 게임 설정 효과음 × 거울 배율 */
	float RaidSound_GetVolume(const UObject* WorldContext)
	{
		return ULoAGameInstance::GetSfxScale(WorldContext) * CVarRaidSoundGain.GetValueOnGameThread();
	}

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

	/** FRaidCue::Voice — 지금 나고 있는 소리와 마지막으로 튼 번호·시각 */
	struct FRaidVoiceState
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		int32 LastBank = 0;
		int32 LastNumber = 0;
		double LastTime = 0.0;
	};
	constexpr double RaidVoiceMinInterval = 0.2;
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

	void Play(const UObject* WorldContext, const FRaidCue& Cue)
	{
		Spawn(WorldContext, Cue);
	}

	UAudioComponent* Spawn(const UObject* WorldContext, const FRaidCue& Cue, bool bAutoDestroy)
	{
		if (!WorldContext || Cue.Bank <= 0) return nullptr;
		const float Volume = ULoAGameInstance::GetSfxScale(WorldContext) * CVarRaidSoundPatternGain.GetValueOnGameThread();
		if (Volume <= 0.f) return nullptr;

		const int32 Number = FMath::RandRange(Cue.First, FMath::Max(Cue.First, Cue.Last));
		const FString Bank = Cue.Bank > 100
			? FString::Printf(TEXT("WwiseDefaultBank_S_Mob_G_BigEchidna%d"), Cue.Bank - 100)
			: FString::Printf(TEXT("WwiseDefaultBank_S_Mob_G_Echidna%d"), Cue.Bank);
		const FString Folder = FString::Printf(TEXT("KR/%s"), *Bank);
		const FString Name = FString::Printf(TEXT("%s_%03d"), *Bank, Number);
		if (RaidSound_PlayedThisFrame(*Folder, *Name)) return nullptr;

		static TMap<int32, FRaidVoiceState> Voices;
		FRaidVoiceState* Voice = Cue.Voice > 0 ? &Voices.FindOrAdd(Cue.Voice) : nullptr;
		if (Voice)
		{
			const UWorld* World = WorldContext->GetWorld();
			const double Now = World ? World->GetTimeSeconds() : 0.0;
			// Now < LastTime = PIE를 다시 시작해 월드 시간이 되돌아감
			if (Voice->LastBank == Cue.Bank && Voice->LastNumber == Number && Now >= Voice->LastTime && Now - Voice->LastTime < RaidVoiceMinInterval)
			{
				return nullptr;
			}
			Stop(Voice->Comp.Get(), 0.05f);
			Voice->LastBank = Cue.Bank;
			Voice->LastNumber = Number;
			Voice->LastTime = Now;
		}

		USoundBase* Sound = Load(*Folder, *Name);
		if (!Sound) return nullptr;
		// 액터에 안 붙음 — 부른 쪽(장판·리본)이 먼저 사라져도 끝까지 나온다
		UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(WorldContext, Sound, Volume, 1.f, 0.f, nullptr, false, bAutoDestroy);
		if (Voice) Voice->Comp = Comp;
		return Comp;
	}

	UAudioComponent* PlayAttached(AActor* Owner, const TCHAR* Folder, const TCHAR* Name, bool bAutoDestroy)
	{
		if (!IsValid(Owner) || !Owner->GetRootComponent()) return nullptr;
		const float Volume = RaidSound_GetVolume(Owner);
		if (Volume <= 0.f || RaidSound_PlayedThisFrame(Folder, Name)) return nullptr;
		USoundBase* Sound = Load(Folder, Name);
		if (!Sound) return nullptr;
		return UGameplayStatics::SpawnSoundAttached(Sound, Owner->GetRootComponent(), NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, true, Volume, 1.f, 0.f, nullptr, nullptr, bAutoDestroy);
	}

	void PlayAt(const UObject* WorldContext, const TCHAR* Folder, const TCHAR* Name, const FVector& Location)
	{
		if (!WorldContext) return;
		const float Volume = RaidSound_GetVolume(WorldContext);
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
		Comp->bAutoDestroy = true;
		if (!Comp->IsPlaying())
		{
			Comp->DestroyComponent();	// bAutoDestroy=false로 만들어 이미 끝난 소리 — 멈출 게 없어 자동 소멸도 안 탄다
			return;
		}
		if (FadeOut > 0.f) Comp->FadeOut(FadeOut, 0.f);
		else Comp->Stop();
	}
}
