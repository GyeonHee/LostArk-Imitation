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
 *  2) 끝나면 플레이어를 붙잡아(강제 정지) 두고 빛 덩어리를 쏜다 — 빛은 **타일 한 칸씩만** 움직인다
 *  3) 1칸째: 거울 정면 방향의 이웃 타일. 그 타일에 플레이어가 서 있어야 한다(= 거울과 플레이어가 1칸 거리 + 정면) —
 *     닿으면 그 타일에 노란 테두리. 2칸째: 플레이어 타일 바로 옆의 보스(다음 거울) 타일 — 이어지면 성공.
 *     1칸째 타일에 플레이어가 없거나(2칸 이상 떨어짐 / 다른 방향), 플레이어 옆 칸에 보스가 없으면 빛은 그 칸에서 멈추고 실패
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

	// (미사용 — 2026-10-07부터 빛은 타일 한 칸씩만 움직여서 거리 제한이 필요 없음)
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float MaxTravelDistance = 5000.f;

	// ── 모양 ──
	UPROPERTY(EditDefaultsOnly, Category = "LinkMirror")
	float BeamHalfWidth = 50.f;

	// 빛 덩어리 반지름 (시각) / OrbHitRadius는 미사용 (판정은 타일 좌표로 함)
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
	// 빛 덩어리가 웨이포인트에 도착했을 때 — 다음 칸으로 갈지, 성공/실패로 끝낼지
	void OnOrbArrived();
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

	// 한 칸 이동 목표 — 도착하면 OnOrbArrived
	FVector OrbTarget = FVector::ZeroVector;
	FIntPoint MirrorCoord = FIntPoint::ZeroValue;
	FIntPoint PlayerCoord = FIntPoint::ZeroValue;
	bool bPlayerCoordValid = false;
	// 0 = 첫 칸(플레이어 타일이어야 함)으로 가는 중, 1 = 보스에게 가는 중
	int32 OrbHop = 0;
	// 첫 칸에서 성공 조건이 이미 깨졌는지 — 도착하면 실패
	bool bHopFails = false;
};
