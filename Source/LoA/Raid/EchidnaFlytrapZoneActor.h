#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaFlytrapZoneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class AHexArena;
class ALoACharacter;

UENUM()
enum class EEchidnaFlytrapPhase : uint8
{
	Inactive,
	Filling,	// 타일 위 파란 헥스 장판이 중심에서 바깥으로 차오름
	Emerging,	// 파리지옥이 솟아오르며 입을 벌림 — 이 단계에 들어가는 순간(장판이 꽉 찬 순간) 타일 위 플레이어를 잡아먹는다
	Snapping,	// 입을 닫음
	Lingering,	// 패턴이 끝날 때까지 유지 — 이 동안 타일을 밟으면 먹힌다
	Sinking,	// Dismiss() 후 가라앉아 사라짐
	Done
};

/**
 * 랜잡 패턴(광폭화 7분 40초) — 타일 하나에 생기는 파란 장판 + 파리지옥.
 * FillDuration 동안 파란 장판이 꽉 차면 파리지옥이 솟아 입을 닫고, 그 순간 **이 타일 위에 서 있는** 플레이어를 잡아먹는다.
 * 파리지옥은 패턴이 끝나 Dismiss()가 불릴 때까지 남아 있고, 그동안 이 타일을 밟아도 똑같이 먹힌다.
 * 먹히면 최대 HP × EatDamageRatio 데미지 + 패턴이 끝날 때까지 붙잡힘(ALoACharacter::SetHeldByPattern — 푸는 건 Task).
 * 에셋 없이 ProceduralMesh로 그림 — 장판은 타일 헥스 모양, 파리지옥은 경첩으로 여닫는 잎 두 장 + 가시.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaFlytrapZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaFlytrapZoneActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** TileInRadius: 타일 내접원 반지름(변까지 거리). 액터는 타일과 같은 위치·회전으로 스폰할 것 */
	void Activate(AHexArena* InArena, const FIntPoint& InCoord, float InTileInRadius,
		float InEatDamageRatio, AController* InInstigator);

	// 패턴 종료 — 파란 장판 단계면 즉시 제거, 꽃이면 가라앉은 뒤 제거
	void Dismiss();

	const FIntPoint& GetCoord() const { return Coord; }

	// 판정이 끝났는가(장판이 꽉 찬 순간 true) — 이후 연출은 계속된다
	bool HasBitten() const { return bBitten; }
	bool IsFinished() const { return Phase == EEchidnaFlytrapPhase::Done; }

	// 파리지옥이 다 솟아 입까지 닫았는가 — 랜잡 Task는 이걸 보고 다음 회차를 시작한다
	bool IsTrapShown() const { return Phase >= EEchidnaFlytrapPhase::Lingering; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flytrap")
	TObjectPtr<UProceduralMeshComponent> ZoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flytrap")
	TObjectPtr<USceneComponent> TrapRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flytrap")
	TObjectPtr<USceneComponent> HingeA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flytrap")
	TObjectPtr<USceneComponent> HingeB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flytrap")
	TObjectPtr<UProceduralMeshComponent> LobeA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flytrap")
	TObjectPtr<UProceduralMeshComponent> LobeB;

	// ── 타이밍 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Timing")
	float FillDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Timing")
	float EmergeDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Timing")
	float SnapDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Timing")
	float SinkDuration = 0.35f;

	// ── 모양 ──
	// 잎 한 장의 반지름 = 타일 내접원 반지름 × 이 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Shape")
	float LobeRadiusRatio = 0.75f;

	// 잎이 오목한 정도 (잎 반지름 대비)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Shape")
	float LobeDepthRatio = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Shape")
	int32 ToothCount = 9;

	// 벌린 각도 / 닫은 각도 (바닥 기준, 도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Shape")
	float OpenAngle = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Shape")
	float ClosedAngle = 86.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Shape")
	float ZoneZOffset = 4.f;

	// ── 색 (M_MirrorLaser "Base Color", 1보다 크면 발광) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Visual")
	FLinearColor ZoneBackgroundColor = FLinearColor(0.15f, 0.35f, 1.5f, 0.3f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Visual")
	FLinearColor ZoneFillColor = FLinearColor(0.4f, 1.0f, 5.f, 0.7f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Visual")
	FLinearColor LobeColor = FLinearColor(0.35f, 1.6f, 0.25f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Visual")
	FLinearColor ToothColor = FLinearColor(2.5f, 0.25f, 0.3f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flytrap|Visual")
	FName ColorParameterName = TEXT("Base Color");

private:
	void BuildHex(int32 Section, float Scale);
	void BuildLobe(UProceduralMeshComponent* Lobe);
	void SetLobeAngle(float AngleDeg);
	void Bite();
	void EatCharactersOnTile();
	void EnterPhase(EEchidnaFlytrapPhase NewPhase);

	TWeakObjectPtr<AHexArena> Arena;
	TWeakObjectPtr<AController> InstigatorController;
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;

	EEchidnaFlytrapPhase Phase = EEchidnaFlytrapPhase::Inactive;
	float PhaseElapsed = 0.f;
	float TileInRadius = 260.f;
	float EatDamageRatio = 0.9f;
	bool bBitten = false;
};
