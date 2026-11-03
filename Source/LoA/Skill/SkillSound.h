#pragma once

#include "CoreMinimal.h"

class AActor;
class UAudioComponent;
class USoundBase;

/** 소서리스 원본 사운드(/Game/LostArk/Character/Magician/Sorceress/Sound/SORCERESS_F_<Id>) 재생 도우미.
 *  Id는 번호 문자열("044"). 감쇠 설정이 없어 2D(거리 무관)로 들린다. 볼륨 = 게임 설정 효과음(`ULoAGameInstance::GetSfxScale`). 어느 스킬에 어떤 번호인지는 Skill/CLAUDE.md */
namespace LoASkillSound
{
	LOA_API USoundBase* Load(const TCHAR* Id);
	/** 캐릭터에 붙여 재생 — 반환된 컴포넌트로 도중에 끊을 수 있다(캐스팅 취소 등). 실패하면 nullptr */
	LOA_API UAudioComponent* PlayAttached(AActor* Owner, const TCHAR* Id);
	/** 한 번 재생하고 잊는다(폭발·낙뢰 등 효과 액터) */
	LOA_API void PlayAt(const UObject* WorldContext, const TCHAR* Id, const FVector& Location);
}
