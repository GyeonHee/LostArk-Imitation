#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaPoopBeamActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class ACharacter;

UENUM()
enum class EEchidnaPoopBeamPhase : uint8
{
	Inactive,
	Tracking,	// 보스 → 플레이어 방향 직사각형이 따라다니며 게이지처럼 차오름 (+ 보스 중심 원도 같이 차오름)
	Exploding,	// 원이 먼저 터지고, 직사각형이 보스 쪽부터 한 칸씩 순차적으로 터짐 (블레이즈처럼 앞으로 뻗어나감)
	Done
};

/**
 * 똥장판 패턴(광폭화 8분 20초) — 보스에서 플레이어 쪽으로 뻗는 추적 장판.
 * 거울 패턴처럼 TrackDuration 동안 회전속도 제한을 두고 플레이어를 따라다니고, 그동안 장판 안쪽이 보스 쪽부터
 * 게이지처럼 차올라 "언제 터지는지"를 보여준다. 꽉 차면 보스 중심 원형 범위가 터지고,
 * 직사각형은 ExplosionSegmentCount칸으로 나뉘어 보스 쪽부터 ExplosionSegmentInterval 간격으로 순차 폭발한다.
 * 한 캐릭터는 이 액터 전체(원 + 직사각형)에서 최대 한 번만 맞는다.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaPoopBeamActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaPoopBeamActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 음수 오버라이드는 BP 값 유지 */
	void Activate(ACharacter* InTarget, float InBeamDamage, float InCircleDamage, AController* InInstigator,
		float LengthOverride = -1.f, float HalfWidthOverride = -1.f, float CircleRadiusOverride = -1.f);

	bool IsFinished() const { return Phase == EEchidnaPoopBeamPhase::Done; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PoopBeam")
	TObjectPtr<UProceduralMeshComponent> BeamMesh;

	// ── 모양 ──
	// 맵 끝과 끝(SideCount=4 기준 타일 중심 간 최대 3180cm + 타일 가장자리)에서도 닿도록 4000
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Shape")
	float BeamLength = 4000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Shape")
	float BeamHalfWidth = 160.f;

	// 보스 중심 원형 폭발 반지름 (0이면 원 없음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Shape")
	float CircleRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Shape")
	int32 CircleSegments = 48;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Shape")
	float ZOffset = 4.f;

	// ── 타이밍 ──
	// 플레이어를 따라다니며 게이지가 차는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Timing")
	float TrackDuration = 3.f;

	// 추적 회전 속도 (도/초) — 걸어서도 피할 수 있게.
	// 장판이 플레이어를 휩쓰는 속도 = 각속도 × 거리라 멀수록 빠르다. 걷기 600cm/s보다 느리려면
	// 맵 끝(약 4000cm)에서도 ω < 600/4000 rad/s ≈ 8.6도/초 → 8 (90 → 45 → 20 → 8)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Timing")
	float TrackingRotationSpeed = 8.f;

	// 직사각형을 몇 칸으로 나눠 순차 폭발시킬지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Timing")
	int32 ExplosionSegmentCount = 8;

	// 칸 사이 폭발 간격 (초) — 총 전진 시간 = Count × Interval
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Timing")
	float ExplosionSegmentInterval = 0.07f;

	// 마지막 칸이 터진 뒤 잔상 유지 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Timing")
	float LingerAfterExplosion = 0.4f;

	// ── 피격 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Hit")
	bool bApplyKnockdownOnHit = true;

	// ── 색 (M_MirrorLaser "Base Color", 1보다 크면 발광) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Visual")
	FLinearColor BackgroundColor = FLinearColor(0.8f, 0.1f, 0.3f, 0.25f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Visual")
	FLinearColor FillColor = FLinearColor(3.f, 0.4f, 1.2f, 0.55f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Visual")
	FLinearColor ExplodeColor = FLinearColor(8.f, 1.2f, 2.5f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoopBeam|Visual")
	FName ColorParameterName = TEXT("Base Color");

private:
	enum : int32 { SectionBackground = 0, SectionFill = 1, SectionExplode = 2 };

	void TickTracking(float DeltaTime);
	void TickExploding(float DeltaTime);

	/** 섹션 하나를 직사각형(X 0~RectLength) + 원(반지름 Radius)으로 다시 그린다. 둘 다 0이면 섹션 제거 */
	void BuildSection(int32 SectionIndex, float RectStart, float RectEnd, float Radius);

	void ExplodeCircle();
	void ExplodeSegment(int32 SegmentIndex);
	void TryHit(ACharacter* Character);

	TWeakObjectPtr<ACharacter> Target;
	TWeakObjectPtr<AController> InstigatorController;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SectionMIDs;

	TSet<TWeakObjectPtr<ACharacter>> AlreadyHit;

	EEchidnaPoopBeamPhase Phase = EEchidnaPoopBeamPhase::Inactive;
	float BeamDamage = 0.f;
	float CircleDamage = 0.f;
	float PhaseElapsed = 0.f;
	int32 ExplodedSegments = 0;
};
