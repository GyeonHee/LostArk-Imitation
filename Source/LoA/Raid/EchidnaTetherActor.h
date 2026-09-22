#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaTetherActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * 에키드나 "끌고간후 장판터지는" 짤패턴의 전조(1단계)에 쓰이는 줄기(촉수) 1개.
 * 보스 정면 기준 여러 각도로 동시에 스폰되어(StateTree Task가 배치) 얇은 직선 장판으로 표시되고,
 * SnapDelay 뒤 그 범위 안에 있는 캐릭터를 1회 판정해서 보스 쪽으로 끌어당긴다(ALoACharacter::ApplyPull —
 * 맞는 즉시 잠깐 멈췄다가 보스 앞까지 강제로 끌려가는 2단계 동작).
 * 데미지는 없음 — 순수하게 "맞으면 멈췄다가 끌려간다"만 담당하고, 실제 타격은 이후 DragFan 2단계(부채꼴 장판)가 처리.
 * 끌려가는 게 끝나기 전에 장판이 터지면 안 되므로, 누군가 맞았다면 PullResolveDelay만큼 기다린 뒤에야
 * IsFinished()가 true가 된다.
 * MirrorActor의 ZoneMeshComp/GetActorsInBeamBox와 동일한 패턴(엔진 기본 Plane + 박스 오버랩)을 재사용.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaTetherActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaTetherActor();

	// 스폰 직후 호출 — PullTarget(보스 위치) 방향으로 당길 대상 판정 타이머 시작
	void Activate(const FVector& InPullTarget, float InPullStrength, AController* InInstigator);

	// 줄기를 표시하는 얇고 긴 장판 메시 (기본값: 엔진 내장 Plane, 로컬 +X 방향으로 뻗음)
	UPROPERTY(VisibleAnywhere, Category = "Tether")
	TObjectPtr<UStaticMeshComponent> ZoneMeshComp;

	// 줄기 길이 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Tether")
	float TetherRange = 1200.f;

	// 줄기 절반 너비/높이 (cm) — 판정 박스 크기
	UPROPERTY(EditDefaultsOnly, Category = "Tether")
	float TetherHalfWidth = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tether")
	float TetherHalfHeight = 150.f;

	// 스폰 후 실제로 판정(당기기)이 발동하기까지 걸리는 시간 (초) — 이 동안 줄기가 뻗어나갈 예상 범위를
	// 미리 보여주는 예고 역할(레퍼런스: 7줄기가 나온 뒤 약 1초 보여주고 나서 당김)
	UPROPERTY(EditDefaultsOnly, Category = "Tether")
	float SnapDelay = 1.0f;

	// ── 피격 효과. "끌고간후 장판터지는"의 줄기는 끌기만, "정면 리본 공격"의 리본은 기절+매혹만 쓴다.
	//    같은 액터를 BP 설정만 바꿔 두 패턴에 재사용하기 위한 스위치

	UPROPERTY(EditDefaultsOnly, Category = "Tether|Hit")
	bool bApplyPullOnHit = true;

	UPROPERTY(EditDefaultsOnly, Category = "Tether|Hit")
	bool bApplyStunOnHit = false;

	// bApplyStunOnHit일 때 걸리는 기절 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Tether|Hit")
	float StunDuration = 3.f;

	// 0보다 크면 피격 시 매혹 게이지를 쌓는다
	UPROPERTY(EditDefaultsOnly, Category = "Tether|Hit")
	int32 CharmGaugeAmount = 0;

	// 판정 발동 후 액터 정리까지 대기 시간 (초) — 끌려가는 게 끝난(FinishSnap) 뒤부터 센다
	UPROPERTY(EditDefaultsOnly, Category = "Tether")
	float LifeAfterSnap = 0.5f;

	// 당김 판정 후 "멈춤 + 실제로 끌려가는" 동작이 끝날 때까지 기다리는 시간 (초).
	// 이 시간이 지나야 IsFinished()가 true가 되어 StateTree가 다음 단계(부채꼴 장판)로 넘어간다 —
	// 안 기다리면 아직 끌려오는 중인데 장판이 터져버린다.
	// ALoACharacter의 PullHoldDuration + 실제 드래그 시간보다 넉넉하게 잡을 것
	UPROPERTY(EditDefaultsOnly, Category = "Tether")
	float PullResolveDelay = 1.2f;

	// 뻗어있는 동안(예상 범위) 색상 — 반투명 빨강. Color 계열 파라미터가 있는 머티리얼이어야 실제로 적용됨
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor TetherColor = FLinearColor(1.f, 0.f, 0.f);

	// 기본 머티리얼(M_MirrorLaser)의 Vector Parameter 이름이 "Base Color"라 이게 기본값
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FName ColorParameterName = TEXT("Base Color");

	// 예상 범위 표시 중 불투명도 (반투명)
	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TetherOpacity = 0.35f;

	// 당기기 판정이 실제로 발동하는 순간(PerformSnap) 색상 — 완전 불투명 빨강
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor SnapColor = FLinearColor(1.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SnapOpacity = 1.0f;

	// StateTree Task가 "당기기 판정까지 끝났는지" 폴링할 때 사용
	bool IsFinished() const { return bSnapped; }

	// StateTree Task가 "실제로 누군가를 끌어당겼는지" 확인할 때 사용 — 아무도 안 맞았으면 이후 장판 폭발을 생략시키는 용도
	bool DidHit() const { return bDidHit; }

protected:
	virtual void BeginPlay() override;

private:
	FVector PullTarget = FVector::ZeroVector;
	float PullStrength = 0.f;
	TWeakObjectPtr<AController> InstigatorController;
	bool bSnapped = false;
	bool bDidHit = false;

	FTimerHandle SnapTimerHandle;
	FTimerHandle ResolveTimerHandle;

	void ApplyMeshColor(const FLinearColor& Color, float Opacity);
	void PerformSnap();

	/** 끌려가는 동작까지 끝난 것으로 간주 — bSnapped를 세워 StateTree가 다음 단계로 넘어가게 하고 소멸을 예약 */
	void FinishSnap();
};
