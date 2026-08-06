#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaMirrorActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UNiagaraSystem;

UENUM()
enum class EEchidnaMirrorPhase : uint8
{
	Tracking,	// 플레이어를 조준하며 장판이 따라가는 중
	Firing,		// 방향 고정, 레이저 반복 판정 중
	Done		// 전부 종료, 소멸 대기
};

/**
 * 에키드나 "4거울" 짤패턴에 쓰이는 거울 1개.
 * 1) 스폰 직후 TrackingDuration(기본 3초) 동안 매 틱 플레이어를 향해 회전하고,
 *    거울에서 플레이어 방향/거리로 뻗은 직사각형 장판(ZoneMeshComp)이 실시간으로 따라간다.
 * 2) TrackingDuration이 끝나면 방향을 고정하고, FiringDuration(기본 3초) 동안
 *    고정된 방향으로 MaxRange까지 뻗은 레이저 장판을 유지하며 LaserDamageTickInterval마다 반복 판정한다.
 * 4방향 배치/동시 스폰은 StateTree Task(FStateTreeTask_EchidnaFourMirrorPattern)가 담당.
 * MirrorMeshComp/ZoneMeshComp는 기본적으로 엔진 내장 Plane/Sphere + BasicShapeMaterial로 채워져 있어
 * 별도 에셋 할당 없이도 즉시 보인다 — BP에서 Static Mesh/Material만 교체하면 비주얼 커스터마이즈 가능.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaMirrorActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaMirrorActor();

	void Activate(float InTickDamage, AController* InInstigator);

	// 거울 자체를 표시하는 메시 (기본값: 엔진 내장 Sphere)
	UPROPERTY(VisibleAnywhere, Category = "Mirror")
	TObjectPtr<UStaticMeshComponent> MirrorMeshComp;

	// 추적/발사 중 표시되는 직사각형 장판 메시 (기본값: 엔진 내장 Plane)
	UPROPERTY(VisibleAnywhere, Category = "Mirror")
	TObjectPtr<UStaticMeshComponent> ZoneMeshComp;

	// 플레이어를 조준하며 추적하는 시간 (초) — 종료 시점 방향으로 레이저 발사 시작
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float TrackingDuration = 3.0f;

	// 추적 중 회전 속도 (도/초) — 낮을수록 천천히 따라옴. 대시 같은 순간이동에도 즉시 안 꺾이고 일정 속도로 쫓아옴
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float TrackingRotationSpeed = 50.f;

	// 방향 고정 후 레이저를 계속 유지/판정하는 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float FiringDuration = 1.0f;

	// 레이저 판정 반복 간격 (초) — 기본값(FiringDuration/3)은 맞는 순간 1틱 + 이 간격으로 3틱 더 = 1초간 총 4틱.
	// 각 틱마다 ALoACharacter는 ApplyKnockdown으로 넉다운(뒤로 튕겨나감)도 같이 발생 — 착지 전에 다음 틱이 오면
	// 계속 다시 띄워지므로 4틱을 맞는 동안은 쭉 공중에 떠 있는 것처럼 보임
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float LaserDamageTickInterval = 1.f / 3.f;

	// 장판/레이저 최대 사거리 (cm) — 추적 중엔 플레이어까지 거리로 클램프, 발사 중엔 이 값 고정 사용
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float MaxRange = 3000.f;

	// 장판/레이저 절반 너비 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float BeamHalfWidth = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float BeamHalfHeight = 150.f;

	// 발사 종료 후 액터 정리까지 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float LifeAfterBeam = 1.0f;

	// 추적 중 거울/장판에 사용할 머티리얼 (Color 계열 파라미터가 있어야 아래 색상이 실제로 적용됨 — 없으면 기본 머티리얼 그대로)
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UMaterialInterface> TrackingZoneMaterial;

	// 발사 중 거울/장판에 사용할 머티리얼
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UMaterialInterface> FiringZoneMaterial;

	// TrackingZoneMaterial/FiringZoneMaterial에서 색상을 넣을 Vector Parameter 이름 (머티리얼에 맞게 수정)
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FName ColorParameterName = TEXT("Color");

	// 추적 중 색상 (기본: 노랑)
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor TrackingColor = FLinearColor(1.f, 0.9f, 0.05f);

	// 발사 중 색상 (기본: 빨강)
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor FiringColor = FLinearColor(1.f, 0.05f, 0.05f);

	// 장판(ZoneMeshComp)만 적용할 투명도 (0=완전투명, 1=불투명) — Color 파라미터의 Alpha로 전달됨.
	// TrackingZoneMaterial/FiringZoneMaterial의 Blend Mode가 Translucent이고 Alpha가 Opacity에 연결돼 있어야 실제로 투명해짐
	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ZoneOpacity = 0.35f;

	// 발사 시작 시 1회 재생할 VFX (선택)
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> BeamStartVFXSystem;

	// StateTree Task가 "이 거울이 발사를 시작했는지" 폴링할 때 사용
	bool HasFired() const { return Phase != EEchidnaMirrorPhase::Tracking; }

	// StateTree Task가 "이 거울의 전체 시퀀스(추적+발사)가 끝났는지" 폴링할 때 사용
	bool IsFinished() const { return Phase == EEchidnaMirrorPhase::Done; }

	UFUNCTION(BlueprintImplementableEvent, Category = "VFX")
	void BP_OnBeginFiring(FVector Origin, FVector Direction);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	EEchidnaMirrorPhase Phase = EEchidnaMirrorPhase::Tracking;
	float TickDamage = 0.f;
	TWeakObjectPtr<AController> InstigatorController;
	float ElapsedTracking = 0.f;
	int32 CurrentDamageTick = 0;
	int32 MaxDamageTicks = 1;

	FTimerHandle DamageTimerHandle;
	FTimerHandle StopFiringTimerHandle;

	void UpdateZoneTransform(float CurrentDistance);
	void BeginFiring();
	void ApplyLaserDamageTick();
	void FinishFiring();
	void ApplyPhaseVisuals(UMaterialInterface* BaseMaterial, const FLinearColor& Color);
	void GetActorsInBeamBox(TArray<AActor*>& OutActors) const;
};
