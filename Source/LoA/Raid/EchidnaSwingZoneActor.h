#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaSwingZoneActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EEchidnaSwingZonePhase : uint8
{
	Inactive,
	Telegraph,	// 안전 원 바깥 전체가 옅게 표시되고, 안전 원 경계에서 바깥으로 게이지처럼 차오름
	Exploded,	// 터진 직후 잠깐 밝게 남음
	Done
};

/**
 * 그네 패턴(광폭화 3분 40초)의 맵 전체 장판.
 * 보스 발밑 기준 SafeRadius 원 안만 안전하고 그 바깥(OuterRadius까지 그리지만 판정은 무한)은 전부 즉사급으로 터진다.
 * TelegraphDuration 동안 안전 원 경계에서 바깥쪽으로 차오르며 터질 시점을 보여준 뒤 1회 판정.
 * 광폭화 규칙: BeginPlay에서 CustomTimeDilation, 진행은 전부 Tick 기반(월드 타이머 안 씀)
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaSwingZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaSwingZoneActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 데미지 = 대상 최대 HP × DamageRatio. SafeRadiusOverride가 음수면 BP 값 유지 */
	void Activate(float InDamageRatio, AController* InInstigator, float SafeRadiusOverride = -1.f);

	/** 판정까지 끝났는가 — 패턴 Task가 폴링 */
	bool HasExploded() const { return Phase == EEchidnaSwingZonePhase::Exploded || Phase == EEchidnaSwingZonePhase::Done; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SwingZone")
	TObjectPtr<UProceduralMeshComponent> ZoneMesh;

	// ── 모양 ──
	// 보스 중심 안전 원 반지름 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Shape")
	float SafeRadius = 900.f;

	// 표시용 바깥 반지름 — 외곽 타일에 선 보스 기준으로 반대편 벽 너머까지 덮도록 넉넉하게 (판정은 거리 제한 없음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Shape")
	float OuterRadius = 4500.f;

	// 안전 원 경계에 그리는 밝은 테두리 두께 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Shape")
	float BorderWidth = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Shape")
	int32 Segments = 96;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Shape")
	float ZOffset = 6.f;

	// ── 타이밍 ──
	// 장판이 보인 뒤 터지기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Timing")
	float TelegraphDuration = 3.f;

	// 터진 뒤 밝은 잔상이 남아 있는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Timing")
	float ExplodeLinger = 0.6f;

	// ── 색 (M_MirrorLaser "Base Color", 1보다 크면 발광) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Visual")
	FLinearColor BackgroundColor = FLinearColor(0.9f, 0.05f, 0.15f, 0.25f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Visual")
	FLinearColor FillColor = FLinearColor(2.5f, 0.2f, 0.5f, 0.45f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Visual")
	FLinearColor BorderColor = FLinearColor(6.f, 0.6f, 1.5f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Visual")
	FLinearColor ExplodeColor = FLinearColor(10.f, 1.5f, 2.5f, 0.85f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingZone|Visual")
	FName ColorParameterName = TEXT("Base Color");

private:
	enum : int32 { SectionBackground = 0, SectionFill = 1, SectionBorder = 2, SectionExplode = 3, SectionCount = 4 };

	/** 섹션 하나를 Inner~Outer 고리로 다시 그린다. Outer <= Inner면 섹션 제거 */
	void BuildRing(int32 SectionIndex, float Inner, float Outer);

	void Explode();

	TWeakObjectPtr<AController> InstigatorController;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SectionMIDs;

	EEchidnaSwingZonePhase Phase = EEchidnaSwingZonePhase::Inactive;
	float DamageRatio = 10.f;
	float PhaseElapsed = 0.f;
};
