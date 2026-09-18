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
 * MirrorMeshComp(거울 몸체)는 엔진 내장 Cylinder를 납작하게 눌러 원판 모양으로, M_EchidnaMirrorSurface
 * (EchidnaModling/other/Mirror 텍스처로 만든 실제 거울 머티리얼)를 입혀서 기본으로 채워져 있음 —
 * 별도 프롭 메시가 프로젝트에 임포트돼 있지 않아 정식 거울 모델 대신 이 방식으로 표현.
 * ZoneMeshComp(장판)는 엔진 내장 Plane + BasicShapeMaterial 기본값 — BP에서 Static Mesh/Material 교체 가능.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaMirrorActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaMirrorActor();

	void Activate(float InTickDamage, AController* InInstigator);

	// true면 플레이어를 조준하지 않고 스폰 시점 Rotation(고정 방향)을 그대로 유지한 채 TrackingDuration 뒤 발사.
	// "8거울 레이저"의 고정 스포크(+/X 대형)처럼 플레이어를 안 쫓는 거울에 사용 — Activate() 호출 전에 설정할 것
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	bool bLockDirectionOnSpawn = false;

	// true면 "하늘에서 나를 향해 쏘는" 유도 레이저 모드 — 다른 패턴들과 달리 예고(Tracking/반투명 예상범위)
	// 단계가 없이 Activate() 즉시 상시 발사 상태로 들어가 StopRepeating()이 불릴 때까지 계속 데미지 틱을
	// 반복한다. 매 틱 자신의 X/Y를 플레이어 위치로 맞춰 따라다니고(Z는 스폰 높이 그대로 유지), 조준도
	// 계속 갱신(수평으로 눕히지 않고 실제 방향(위→아래) 그대로 써서 높은 곳에서 아래로 비스듬히 꽂히는
	// 각도가 나옴) — "패턴이 끝날 때까지 멈추지 않고 계속 쏘는" 유도 거울에 사용
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	bool bSkyGuidedMode = false;

	// bSkyGuidedMode 전용 — 스폰 후 이 시간(초) 동안은 제자리에 그대로 있다가 그 뒤부터 플레이어를 따라가기 시작함
	// (스폰 위치가 바로 안 묻히고 눈에 보이도록)
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float SkyGuidedFollowDelay = 0.5f;

	// bSkyGuidedMode 전용 — 따라가는 속도를 플레이어 이동속도의 몇 배로 할지 (1.0 미만이면 플레이어보다 느려서
	// 계속 거리를 벌리면 따돌릴 수 있음 — "무조건 맞는" 판정 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Mirror", meta = (ClampMin = "0.0"))
	float SkyGuidedFollowSpeedRatio = 0.45f;

	// bSkyGuidedMode 전용 — 조준(액터 Rotation, 발사 방향) 회전 속도 (도/초). 다른 패턴들의 TrackingRotationSpeed는
	// 일부러 느리게 튜닝돼 있어서(4거울 등) 공용 값을 못 씀 — 유도 거울은 "항상 나를 보고 있어야" 하므로 거의
	// 즉시 도는 값을 기본으로 둠
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float SkyGuidedAimRotationSpeed = 720.f;

	// bSkyGuidedMode 전용 — 장판/레이저 길이를 거리에 따라 늘리지 않고 이 값으로 고정. 다른 패턴들처럼
	// MaxRange(먼 거리까지 커버)를 그대로 쓰면 플레이어가 아무리 멀어져도 거의 항상 닿아버려서
	// "무조건 맞는" 구조가 됨 — 유도 거울은 이 거리 안에 있을 때만 맞도록 훨씬 짧게 고정
	UPROPERTY(EditDefaultsOnly, Category = "Mirror")
	float SkyGuidedBeamRange = 450.f;

	// bSkyGuidedMode 반복을 멈춤 — 호출 후 진행 중인 한 사이클(추적 또는 발사)만 마저 끝내고 소멸.
	// StateTree Task가 전체 패턴이 끝날 때 호출
	void StopRepeating() { bStopRequested = true; }

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
	float TrackingRotationSpeed = 15.f;

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

	// 추적(예상 범위) 중 색상 — 반투명 빨강
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor TrackingColor = FLinearColor(1.f, 0.f, 0.f);

	// 발사(실제 판정) 중 색상 — 완전 불투명 빨강
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor FiringColor = FLinearColor(1.f, 0.f, 0.f);

	// 장판(ZoneMeshComp)만 적용할 투명도 (0=완전투명, 1=불투명) — Color 파라미터의 Alpha로 전달됨. 예상 범위는 반투명하게.
	// TrackingZoneMaterial/FiringZoneMaterial의 Blend Mode가 Translucent이고 Alpha가 Opacity에 연결돼 있어야 실제로 투명해짐
	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ZoneOpacity = 0.35f;

	// 발사(실제 판정) 중 장판 투명도 — 실행 범위는 완전 불투명하게
	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FiringOpacity = 1.0f;

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

	// Activate() 호출 전까지 Tick()이 추적 타이머를 진행시키지 않게 막는 가드 —
	// "8거울" 패턴에서 미리 스폰만 해두고 자기 차례가 될 때 Activate()를 호출하는 용도
	bool bActivated = false;

	// bSkyGuidedMode에서 StopRepeating() 호출 여부 — true가 되면 현재 사이클을 마지막으로 반복 종료
	bool bStopRequested = false;

	// bSkyGuidedMode에서 Activate() 이후 누적 시간 — SkyGuidedFollowDelay와 비교해 추적 시작 시점을 판단.
	// 발사→추적 반복 루프에서는 리셋되지 않으므로 최초 한 번만 지연되고 이후로는 계속 따라감
	float SkyGuidedElapsed = 0.f;

	FTimerHandle DamageTimerHandle;
	FTimerHandle StopFiringTimerHandle;

	float GetEffectiveBeamRange() const { return bSkyGuidedMode ? SkyGuidedBeamRange : MaxRange; }
	void UpdateZoneTransform(float CurrentDistance);
	void UpdateMirrorBodyRotation();
	void BeginFiring();
	void ApplyLaserDamageTick();
	void FinishFiring();
	void ApplyPhaseVisuals(UMaterialInterface* BaseMaterial, const FLinearColor& Color, float InZoneOpacity);
	void GetActorsInBeamBox(TArray<AActor*>& OutActors) const;
};
