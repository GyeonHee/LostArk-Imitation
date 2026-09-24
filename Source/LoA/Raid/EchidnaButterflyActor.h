#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaButterflyActor.generated.h"

class USphereComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class AHexArena;

/**
 * 그네 패턴에서 연기와 함께 맵 안쪽을 날아다니는 작은 나비.
 * 랜덤 방향으로 천천히 날다가 WanderInterval마다 방향을 조금씩 틀고, 다음 위치가 아레나 밖이면 안쪽으로 방향을 바꾼다.
 * 플레이어에 닿으면 StunDuration(10초) 기절 후 사라진다 (이미 기절 중이면 그냥 통과 — 기절이 끝없이 갱신되지 않게).
 * 비주얼은 에셋 없이 PMC 날개 두 장 — 날개 폭(Y 스케일)을 흔들어 날갯짓처럼 보이게 한다.
 * 광폭화 규칙: BeginPlay에서 CustomTimeDilation, 진행은 전부 Tick 기반
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaButterflyActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaButterflyActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Arena 안에서만 날아다닌다 — 높이는 스폰 위치 그대로 유지 */
	void Activate(AHexArena* InArena, float InStunDuration);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Butterfly")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Butterfly")
	TObjectPtr<UProceduralMeshComponent> WingMesh;

	// 비행 속도 (cm/s) — 걷기(600)보다 훨씬 느리게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly")
	float Speed = 140.f;

	// 이 간격(초)마다 방향을 ±WanderAngle 안에서 랜덤하게 튼다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly")
	float WanderInterval = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly")
	float WanderAngle = 60.f;

	// 판정 반지름 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly")
	float CollisionRadius = 45.f;

	// 날개 한 장 길이 (cm) — 나비 전체 폭은 이 값의 약 2배
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly|Visual")
	float WingSize = 45.f;

	// 날갯짓 속도 (초당 왕복 횟수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly|Visual")
	float FlapFrequency = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly|Visual")
	FLinearColor WingColor = FLinearColor(5.f, 1.2f, 3.5f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Butterfly|Visual")
	FName ColorParameterName = TEXT("Base Color");

private:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void BuildWings();
	void PickDirectionTowardCenter();
	void SetFlyDirection(const FVector& NewDir);

	TWeakObjectPtr<AHexArena> Arena;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WingMID;

	FVector FlyDirection = FVector::ForwardVector;
	float StunDuration = 10.f;
	float WanderElapsed = 0.f;
	float FlapTime = 0.f;
	bool bActive = false;
};
