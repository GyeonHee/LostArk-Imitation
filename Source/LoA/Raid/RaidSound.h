#pragma once

#include "CoreMinimal.h"

class AActor;
class UAudioComponent;
class USoundBase;

/** 에키드나 레이드 원본 사운드(/Game/LostArk/Raid/EchidnaModling/sound/<Folder>/<Name>) 재생 도우미.
 *  2D, 볼륨 = 게임 설정 효과음. 같은 소리가 같은 프레임에 여러 번 불리면(4거울 4개가 동시에 나옴 등) 한 번만 — 겹치면 크고 뭉개짐.
 *  어느 패턴에 어떤 소리인지는 Raid/CLAUDE.md "레이드 사운드" */
namespace LoARaidSound
{
	LOA_API USoundBase* Load(const TCHAR* Folder, const TCHAR* Name);
	/** 액터에 붙여 재생 — 반환 컴포넌트로 끊거나 다시 틀 수 있다. bAutoDestroy=false면 끝나도 남아 다시 Play 가능(반복용).
	 *  같은 프레임 중복이거나 효과음 0이면 nullptr */
	LOA_API UAudioComponent* PlayAttached(AActor* Owner, const TCHAR* Folder, const TCHAR* Name, bool bAutoDestroy = true);
	LOA_API void PlayAt(const UObject* WorldContext, const TCHAR* Folder, const TCHAR* Name, const FVector& Location);
	/** 반복용 컴포넌트가 끝났으면 처음부터 다시 */
	LOA_API void KeepLooping(UAudioComponent* Comp);
	LOA_API void Stop(UAudioComponent* Comp, float FadeOut = 0.2f);
}
