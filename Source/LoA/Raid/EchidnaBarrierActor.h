#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaBarrierActor.generated.h"

class UBoxComponent;

/**
 * 보이지 않는 벽 — 패턴 동안 플레이어 이동 범위를 막는다(거울 카운터 정사각형).
 * Pawn 채널만 Block, 나머지(클릭 Visibility·카메라·돌풍 오브젝트 오버랩)는 전부 무시. 오브젝트 타입 WorldStatic이라
 * 돌풍의 AllDynamicObjects 오버랩에도 안 잡힌다. 크기는 스폰한 쪽이 SetHalfExtent로
 */
UCLASS()
class LOA_API AEchidnaBarrierActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaBarrierActor();

	void SetHalfExtent(const FVector& HalfExtent);

	UPROPERTY(VisibleAnywhere, Category = "Barrier")
	TObjectPtr<UBoxComponent> BoxComp;
};
