#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaJustGuardZoneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ALoACharacter;
class AEchidnaBoss;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 저스트 가드 장판 — 헤링본 무늬 노란 원이 중심(시전자)에서 바깥으로 차오르는 게이지. 그동안 시전자 몸에서 노란 링 파동
 * (NS_EchidnaJustGuardWave — 로컬 공간 이미터, 링 반지름 고정)이 WaveInterval마다 한 점에서 원으로 퍼진다. 퍼짐은
 * 컴포넌트 스케일로 — 이미 생긴 입자까지 링째로 커져야 원 모양이 유지된다(생성 반지름만 키우면 원판처럼 흩어졌음)
 * 게이지가 다 차는 순간(FillDuration)을 기준으로 ±판정 창 안에 G를 누르고, 그때 커서가 "플레이어 → 중심" 방향
 * GuardHalfAngle 안을 가리키면 성공. 한 판에 한 번만 누를 수 있다 — 너무 일찍 누르거나 방향이 틀리면 그 자리에서 실패 확정.
 * 실패(안 누름 포함)면 게이지가 다 찬 순간(늦은 창이 끝난 뒤) 최대 HP × DamageMaxHPRatio 고정 피해.
 * G 입력은 ALoAPlayerController::OnInteractKey가 커서 조준점과 함께 TryJustGuard로 넘긴다 — 장판이 플레이어 근처
 * (반지름 + RelevantDistance)거나 화면 안일 때만. 받은 G는 성공·실패 무관하게 StanceDuration 동안 이동을 멈춘다.
 * 게이지 시간은 콘솔 LoA.JustGuard.FillDuration으로 PIE 중 덮어쓸 수 있다(0 = FillDuration).
 * 판정 시간은 실시간(CustomTimeDilation 1) — 광폭화와 무관
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaJustGuardZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaJustGuardZoneActor();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** WaveSource = 파동이 나오는 시전자(몬스터·거울). 없으면 장판 중심. WaveZOffset = 시전자 위치에서 파동 높이 */
	void Activate(ALoACharacter* InPlayer, AEchidnaBoss* InBoss, float InRadius, AActor* InWaveSource = nullptr, float InWaveZOffset = 0.f);

	/** G 입력 — 아직 판정 전이면 소비하고 true(성공·실패 무관), 이미 눌렀거나 끝났으면 false */
	bool TryJustGuard(APawn* Pawn, const FVector& AimLocation);

	bool IsAwaitingInput() const { return bActive && !bResolved && !bPressed; }
	float GetRadius() const { return Radius; }
	/** 실제로 쓰는 게이지 시간 — 콘솔 LoA.JustGuard.FillDuration > 0이면 그 값 */
	float GetFillDuration() const;
	bool IsFinished() const { return bFinished; }
	bool WasSuccess() const { return bSuccess; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JustGuard")
	TObjectPtr<UProceduralMeshComponent> BackMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JustGuard")
	TObjectPtr<UProceduralMeshComponent> FillMesh;

	// 게이지가 0 → 가득 차는 시간 (초) = 판정 기준 시각. PIE에서 콘솔 LoA.JustGuard.FillDuration으로 시험해 보고 여기에 반영
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Timing")
	float FillDuration = 1.6f;

	// G를 받은 뒤 이동을 멈추는 시간 (초) — 성공·실패 무관
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Input")
	float StanceDuration = 1.5f;

	// 장판 가장자리에서 이 거리(cm) 안이거나 장판 중심이 화면 안이면 G를 받는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Input")
	float RelevantDistance = 1500.f;

	// 판정 창 — 가득 차는 순간보다 이만큼 먼저 / 늦게 눌러도 성공 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Timing")
	float EarlyWindow = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Timing")
	float LateWindow = 0.15f;

	// 커서 방향 허용 반각(도) — "플레이어 → 중심"과 "플레이어 → 커서" 사이 각도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Judge", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float GuardHalfAngle = 30.f;

	// 플레이어가 중심에 이만큼 가까우면 방향을 따지지 않는다 (cm) — 바로 앞에선 커서 조금만 흔들려도 각도가 크게 튐
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Judge")
	float IgnoreAngleDistance = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Judge")
	float DamageMaxHPRatio = 0.8f;

	// 결과 표시 후 사라지기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Timing")
	float ResultLingerTime = 0.6f;

	// ── 시전자 파동 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	TObjectPtr<UNiagaraSystem> WaveVFX;

	// 새 파동을 띄우는 간격 / 한 파동이 한 점에서 끝 반지름까지 퍼지는 시간 (초, 완만한 ease-out).
	// 0.4초 + 3제곱 곡선은 0.13초 만에 70%까지 퍼져 "퍼지는 과정"이 안 보였음. 입자 수명(NS 0.5~0.8초)이 짧아 다 퍼진 링은 곧 사라진다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveExpandDuration = 0.9f;

	// 링 실제 반지름 (cm) — 시작은 거의 한 점
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveStartRadius = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveEndRadius = 450.f;

	// NS가 링을 그리는 반지름(User.CircleRadius, 원본 125) — 실제 반지름 = 이 값 × 컴포넌트 스케일
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveBaseRadius = 125.f;

	// 링 두께(위아래 흩어짐) 스케일 상한 — 가로만 크게 키우고 세로는 이 배율까지만
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveMaxHeightScale = 1.5f;

	// 파티클 양 (NS의 User.Density, 1 = 원본 초당 7500)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Wave")
	float WaveDensity = 1.f;

	// ── 비주얼 (M_EchidnaJustGuardZone — 헤링본, "Base Color" RGB × 밝기, Alpha = 불투명도) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor BackColor = FLinearColor(1.2f, 0.6f, 0.1f, 0.18f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor FillColor = FLinearColor(3.f, 1.6f, 0.25f, 0.45f);

	// 판정 창 안에 들어오면 게이지를 이 색으로 — "지금 눌러" 신호
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor WindowColor = FLinearColor(8.f, 5.f, 0.6f, 0.6f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor SuccessColor = FLinearColor(1.5f, 3.5f, 9.f, 0.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor FailColor = FLinearColor(8.f, 0.3f, 0.3f, 0.5f);

	// 바닥에서 띄우는 높이 (cm) — 타일과 Z-파이팅 방지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	float FloorOffset = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FName ColorParameterName = TEXT("Base Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard")
	FText SuccessText = FText::FromString(TEXT("Just Guard!"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard")
	FText FailText = FText::FromString(TEXT("Guard Fail"));

private:
	void BuildDisc(UProceduralMeshComponent* Mesh, float DiscRadius, float Z);
	void TickWaves(float DeltaTime);
	void SpawnWave();
	void StopWaves();
	void ApplyWaveRadius(UNiagaraComponent* Comp, float RingRadius) const;
	void Resolve(bool bInSuccess);
	void ShowResultLabel(bool bInSuccess);

	TWeakObjectPtr<ALoACharacter> Player;
	TWeakObjectPtr<AEchidnaBoss> Boss;
	TWeakObjectPtr<AActor> WaveSource;
	float WaveZOffset = 0.f;
	float WaveSpawnElapsed = 0.f;

	struct FWave
	{
		TWeakObjectPtr<UNiagaraComponent> Comp;
		float Age = 0.f;
		bool bStopped = false;
	};
	TArray<FWave> Waves;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMID;

	float Radius = 3000.f;
	float Elapsed = 0.f;
	float ResolvedAt = 0.f;
	bool bActive = false;
	bool bPressed = false;
	bool bPressSuccess = false;
	bool bResolved = false;
	bool bSuccess = false;
	bool bFinished = false;
};
