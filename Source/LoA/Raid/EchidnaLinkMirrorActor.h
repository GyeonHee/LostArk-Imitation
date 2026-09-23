#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaLinkMirrorActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ALoACharacter;
class AHexArena;

UENUM()
enum class EEchidnaLinkMirrorPhase : uint8
{
	Inactive,
	Tracking,	// 노란 빛줄기가 플레이어를 따라감 (거울도 같이 회전)
	Firing,		// 거울 정면으로 빛 덩어리 직진 — 플레이어에 닿으면 Linking, 아니면 맵 밖으로 나가 실패
	Linking,	// 플레이어 → 보스 타일로 빛 덩어리 이동
	Done
};

UENUM()
enum class EEchidnaLinkResult : uint8
{
	Pending,
	Success,	// 빛 덩어리가 플레이어를 거쳐 보스에게 도달
	Fail		// 플레이어를 못 맞힘 / 플레이어 옆 칸에 보스가 없음
};

/**
 * 반정산 "거울잇기" 패턴의 거울. 외곽 파란 테두리 타일 위에 서서
 *  1) TrackDuration(5초) 동안 노란 빛줄기로 플레이어를 따라간다 (TrackingRotationSpeed 제한, 거울 몸체도 같이 회전)
 *  2) 끝나면 플레이어를 붙잡아(강제 정지) 두고 거울 정면으로 빛 덩어리를 쏜다
 *  3) 빛 덩어리가 플레이어에 닿으면 플레이어 타일에 노란 테두리 — 그 타일과 보스 타일이 이웃이면 보스에게 이동해 성공,
 *     아니면 실패. 플레이어에 안 닿고 MaxTravelDistance를 넘으면(맵 밖) 실패
 * 결과는 GetResult()로 — 실패 페널티(전 타일 빨강 + 즉사급)와 정리는 패턴 Task가 한다.
 * 비주얼은 기존 거울(AEchidnaMirrorActor)과 같은 컨벤션: 엔진 Cylinder 원반 + M_EchidnaMirrorSurface, 빛줄기는 Plane, 빛 덩어리는 Sphere
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaLinkMirrorActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaLinkMirrorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void Activate(ALoACharacter* InPlayer, AHexArena* InArena, AActor* InBoss, const FIntPoint& InBossCoord);

	EEchidnaLinkResult GetResult() const { return Result; }
	bool IsFinished() const { return Phase == EEchidnaLinkMirrorPhase::Done; }

	UPROPERTY(VisibleAnywhere, Category = "LinkMirror")
	TObjectPtr<UStaticMeshComponent> MirrorMeshComp;

	UPROPERTY(VisibleAnywhere, Category = "LinkMirror")
	TObjectPtr<UStaticMeshComponent> BeamMeshComp;

	UPROPERTY(VisibleAnywhere, Category = "LinkMirror")
	TObjectPtr<UStaticMeshComponent> OrbMeshComp;

	// ── 타이밍/속도 ──
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float TrackDuration = 5.f;

	// 빛줄기 추적 회전 속도 (도/초)
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float TrackingRotationSpeed = 45.f;

	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float OrbSpeed = 1400.f;

	// 이만큼 날아가도 플레이어에 못 닿으면 맵 밖으로 나간 것으로 보고 실패
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float MaxTravelDistance = 5000.f;

	// ── 모양 ──
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float BeamHalfWidth = 50.f;

	// 빛 덩어리 반지름 (시각) / 플레이어 판정 반지름 (캡슐 반지름에 더해짐)
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float OrbRadius = 55.f;

	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float OrbHitRadius = 70.f;

	// 거울 원반·빛 덩어리 높이 (타일 윗면 기준)
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float MirrorHeight = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float OrbHeight = 100.f;

	// ── 색 (M_MirrorLaser "Base Color", 1보다 크면 발광) ──
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	FLinearColor BeamColor = FLinearColor(4.f, 3.2f, 0.3f, 0.5f);

	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	FLinearColor OrbColor = FLinearColor(10.f, 8.f, 1.5f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	FName ColorParameterName = TEXT("Base Color");

private:
	void TickTracking(float DeltaTime);
	void TickFiring(float DeltaTime);
	void TickLinking(float DeltaTime);
	void BeginFiring();
	void Finish(EEchidnaLinkResult InResult);
	void UpdateBeam(float Length);

	TWeakObjectPtr<ALoACharacter> Player;
	TWeakObjectPtr<AHexArena> Arena;
	TWeakObjectPtr<AActor> Boss;
	FIntPoint BossCoord = FIntPoint::ZeroValue;

	EEchidnaLinkMirrorPhase Phase = EEchidnaLinkMirrorPhase::Inactive;
	EEchidnaLinkResult Result = EEchidnaLinkResult::Pending;
	float PhaseElapsed = 0.f;
	float OrbTraveled = 0.f;
	FVector OrbDirection = FVector::ForwardVector;
};
