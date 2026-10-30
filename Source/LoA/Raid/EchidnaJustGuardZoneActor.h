#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaJustGuardZoneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ALoACharacter;
class AEchidnaBoss;

/**
 * 저스트 가드 장판 — 노란 원(맵 전체를 덮는 반지름)이 중심(오답 거울)에서 바깥으로 차오르는 게이지.
 * 게이지가 다 차는 순간(FillDuration)을 기준으로 ±판정 창 안에 G를 누르고, 그때 커서가 "플레이어 → 중심" 방향
 * GuardHalfAngle 안을 가리키면 성공. 한 판에 한 번만 누를 수 있다 — 너무 일찍 누르거나 방향이 틀리면 그 자리에서 실패 확정.
 * 실패(안 누름 포함)면 게이지가 다 찬 순간(늦은 창이 끝난 뒤) 최대 HP × DamageMaxHPRatio 고정 피해.
 * G 입력은 ALoAPlayerController::OnInteractKey가 커서 조준점과 함께 TryJustGuard로 넘긴다.
 * 판정 시간은 실시간(CustomTimeDilation 1) — 광폭화와 무관
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaJustGuardZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaJustGuardZoneActor();

	virtual void Tick(float DeltaTime) override;

	void Activate(ALoACharacter* InPlayer, AEchidnaBoss* InBoss, float InRadius);

	/** G 입력 — 아직 판정 전이면 소비하고 true(성공·실패 무관), 이미 눌렀거나 끝났으면 false */
	bool TryJustGuard(APawn* Pawn, const FVector& AimLocation);

	bool IsAwaitingInput() const { return bActive && !bResolved && !bPressed; }
	bool IsFinished() const { return bFinished; }
	bool WasSuccess() const { return bSuccess; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JustGuard")
	TObjectPtr<UProceduralMeshComponent> BackMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "JustGuard")
	TObjectPtr<UProceduralMeshComponent> FillMesh;

	// 게이지가 0 → 가득 차는 시간 (초) = 판정 기준 시각
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Timing")
	float FillDuration = 2.5f;

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

	// ── 비주얼 (M_MirrorLaser "Base Color", Alpha = 불투명도) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor BackColor = FLinearColor(1.2f, 1.0f, 0.1f, 0.16f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor FillColor = FLinearColor(3.f, 2.4f, 0.2f, 0.38f);

	// 판정 창 안에 들어오면 게이지를 이 색으로 — "지금 눌러" 신호
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JustGuard|Visual")
	FLinearColor WindowColor = FLinearColor(8.f, 6.f, 0.6f, 0.55f);

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
	void Resolve(bool bInSuccess);
	void ShowResultLabel(bool bInSuccess);

	TWeakObjectPtr<ALoACharacter> Player;
	TWeakObjectPtr<AEchidnaBoss> Boss;

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
