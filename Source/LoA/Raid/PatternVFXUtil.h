#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UObject;

/**
 * 패턴 판정 범위를 Niagara로 "채우는" 공용 함수 — 이펙트 하나를 늘리지 않고 Spacing 간격 격자로 여러 개를 깔아
 * 부채꼴·도넛·원·직사각형 등 판정 모양을 그대로 따라가게 한다. 격자 칸의 "중심"에만 놓아 경계에서 반 칸 안쪽.
 * 개수가 MaxCount를 넘으면 간격을 자동으로 넓힌다. 액터에 붙이지 않고 월드에 스폰(bAutoDestroy).
 * 부채꼴 장판(AEchidnaFanZoneActor)·똥장판(AEchidnaPoopBeamActor)·렌잡 연꽃 폭발이 같이 쓴다.
 */
namespace PatternVFX
{
	/**
	 * 부채꼴/도넛/원 영역 — Origin 기준 로컬 +X가 정면, HalfAngleDeg 180이면 원.
	 * 각 인스턴스는 바깥(반지름) 방향을 바라본다. 스폰한 개수를 반환
	 */
	int32 SpawnFanArea(UObject* WorldContext, UNiagaraSystem* System, const FTransform& Origin,
		float InnerRadius, float OuterRadius, float HalfAngleDeg, float Spacing, float Scale, int32 MaxCount);

	/**
	 * 직사각형 영역 — Origin 기준 로컬 X ∈ [XStart, XEnd], Y ∈ [-HalfWidth, HalfWidth].
	 * 각 인스턴스는 로컬 +X(정면)를 바라본다. 스폰한 개수를 반환
	 */
	int32 SpawnRectArea(UObject* WorldContext, UNiagaraSystem* System, const FTransform& Origin,
		float XStart, float XEnd, float HalfWidth, float Spacing, float Scale, int32 MaxCount);
}
