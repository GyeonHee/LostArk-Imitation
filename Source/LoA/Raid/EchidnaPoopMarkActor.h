#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaPoopMarkActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class ALoACharacter;
class AHexArena;

/**
 * 똥장판 패턴(광폭화 8분 20초) — 플레이어 발밑에 붙는 원형 게이지.
 * Duration 동안 링이 시계방향으로 차오르고, 다 차는 순간 플레이어가 밟고 있는 타일을 **비활성** 오염 장판으로 바꾼다
 * (이미 오염돼 있으면 변화 없음 — 활성이면 활성 그대로). 플레이어를 매 틱 따라다닌다.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaPoopMarkActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaPoopMarkActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** bWaitForStunEnd: 대상이 기절 중이면 풀릴 때까지 게이지를 숨긴 채 기다렸다가 시작 (하트에 맞아 3초 기절 → 풀린 뒤 5초) */
	void Activate(ALoACharacter* InTarget, AHexArena* InArena, bool bWaitForStunEnd = false);

	// 게이지가 다 차서 타일 변환까지 끝났으면 true
	bool IsFinished() const { return bFinished; }

	ALoACharacter* GetTarget() const { return Target.Get(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PoopMark")
	TObjectPtr<UProceduralMeshComponent> RingMesh;

	// 게이지가 다 차는 데 걸리는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	float Duration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	float RingInnerRadius = 75.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	float RingOuterRadius = 95.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	int32 ArcSegments = 48;

	// 발밑 지면에서 띄우는 높이 (Z-fighting 방지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	float ZOffset = 6.f;

	// 아직 안 찬 부분(바탕 링) / 차오른 부분 — M_MirrorLaser(Translucent+Unlit)라 1보다 큰 값이 발광
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	FLinearColor BackgroundColor = FLinearColor(0.6f, 0.05f, 0.1f, 0.35f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	FLinearColor FillColor = FLinearColor(5.f, 0.3f, 0.5f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopMark")
	FName ColorParameterName = TEXT("Base Color");

private:
	void BuildRing(int32 SectionIndex, float Progress);
	void ConvertTileUnderTarget();
	void FollowTarget();

	TWeakObjectPtr<ALoACharacter> Target;
	TWeakObjectPtr<AHexArena> Arena;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackgroundMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMID;

	float Elapsed = 0.f;
	bool bActive = false;
	bool bWaitingForStunEnd = false;
	bool bFinished = false;
};
