#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaHeartActor.generated.h"

class UProceduralMeshComponent;
class USphereComponent;
class UMaterialInterface;
class AEchidnaPoopMarkActor;
class ALoACharacter;

/**
 * 에키드나 "전방향 하트발사" 짤패턴에 쓰이는 하트 투사체 1개.
 * Launch() 호출 전에는 제자리에 가만히 떠 있기만 해서(bLaunched=false) 패턴 시작 시 보스 머리 위
 * 예고(텔레그래프) 표시로도 그대로 재사용한다(FStateTreeTask_EchidnaHeartBurstPattern 참조).
 * Launch()가 호출되면 지정한 방향으로 직선 비행하다가 캐릭터에 맞으면 데미지+기절+매혹게이지를
 * 적용하고 소멸하며, MaxRange에 도달할 때까지 아무도 못 맞히면 그냥 소멸한다.
 *
 * 비주얼은 평면 스프라이트(텍스처+알파마스크)가 아니라 HexArena 벽/부채꼴 장판과 동일한 방식으로
 * ProceduralMeshComponent에 실제 하트 곡선(파라메트릭 하트 커브)을 압출(두께를 준 솔리드)해서 직접
 * 생성한다 — 텍스처 UV/회전 정렬 문제 자체가 없고, Opaque 솔리드라 반투명/알파 관련 버그도 없음.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaHeartActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaHeartActor();

	virtual void Tick(float DeltaTime) override;

	// 지정한 방향으로 발사 시작 — 호출 전까지는 제자리에 가만히 있음(예고 마커로 사용 가능)
	void Launch(const FVector& Direction, AController* InInstigator, float InSpeed, float InDamage,
		float InStunDuration, int32 InCharmGaugeAmount);

	/** 켜면 맞았을 때 매혹 스택 대신 오염 장판 게이지(AEchidnaPoopMarkActor — 5초 뒤 서 있는 타일이 비활성 오염 장판)를 붙인다.
	 *  전방향 하트발사가 켜고, 백스탭 하트발사는 그대로 매혹 스택 */
	void SetSpawnPoopMarkOnHit(bool bEnable) { bSpawnPoopMarkOnHit = bEnable; }

	// 비우면 네이티브 AEchidnaPoopMarkActor
	UPROPERTY(EditDefaultsOnly, Category = "Heart")
	TSubclassOf<AEchidnaPoopMarkActor> PoopMarkClass;

	UPROPERTY(VisibleAnywhere, Category = "Heart")
	TObjectPtr<UProceduralMeshComponent> HeartMeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Heart")
	TObjectPtr<USphereComponent> CollisionComp;

	// 최대 비행 거리 (cm) — 이 거리에 도달할 때까지 아무도 못 맞히면 그냥 소멸
	UPROPERTY(EditDefaultsOnly, Category = "Heart")
	float MaxRange = 2000.f;

	// 하트 시각적 크기 배율 — HeartMeshComp(압출된 3D 하트 메시)에 적용되는 스케일. BeginPlay에서 적용되므로
	// BP 서브클래스에서 이 값만 바꾸면 즉시 반영됨(생성자에서 하드코딩된 값을 직접 오버라이드하는 것보다 안전)
	UPROPERTY(EditDefaultsOnly, Category = "Heart")
	float HeartVisualScale = 1.f;

	// 하트 압출 두께 (cm) — 앞뒤 면 사이 거리. 이 값을 0에 가깝게 두면 다시 납작해짐
	UPROPERTY(EditDefaultsOnly, Category = "Heart")
	float HeartThickness = 20.f;

	// 하트 곡선을 몇 조각으로 나눌지 — 값이 클수록 매끄러움
	UPROPERTY(EditDefaultsOnly, Category = "Heart", meta = (ClampMin = "8"))
	int32 HeartSegments = 32;

	// 하트 충돌 판정 반지름 (cm) — 시각적 크기를 키우면 판정 범위도 같이 키우는 걸 권장
	UPROPERTY(EditDefaultsOnly, Category = "Heart")
	float HeartCollisionRadius = 40.f;

	// 날아가는 동안 HeartMeshComp를 로컬 Z축(세로) 기준으로 계속 돌리는 속도 (도/초) — 0이면 회전 없음.
	// 예고 마커(Launch 전)는 세워진 채로 고정, 실제 발사된 뒤(bLaunched)부터만 회전함
	UPROPERTY(EditDefaultsOnly, Category = "Heart")
	float HeartSpinSpeed = 60.f;

	// 하트 메시에 입힐 기본 머티리얼 — Opaque 솔리드라 텍스처/알파 없이 색상만 있으면 됨
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	// 색상(핑크) — BaseMaterial에 ColorParameterName과 같은 이름의 Vector Parameter가 있어야 실제로 색이 바뀜
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor HeartColor = FLinearColor(1.f, 0.05f, 0.4f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FName ColorParameterName = TEXT("Base Color");

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bLaunched = false;
	bool bHasHit = false;
	FVector FlyDirection = FVector::ForwardVector;
	float Speed = 800.f;
	float Damage = 10.f;
	float StunDuration = 3.f;
	int32 CharmGaugeAmount = 1;
	bool bSpawnPoopMarkOnHit = false;

	void SpawnPoopMark(ALoACharacter* Character);
	float TraveledDistance = 0.f;
	TWeakObjectPtr<AController> InstigatorController;

	void ApplyHit(AActor* Target);

	// 파라메트릭 하트 커브를 앞/뒤 면 + 옆면(테두리 두께)으로 압출한 솔리드 메시를 생성
	void BuildHeartMesh();
};
