#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaGaugeRectActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 게이지식 직사각형 예고 장판 — 반투명 배경 위로 원점(로컬 +X 시작)부터 끝까지 채움이 차오른다.
 * 판정·타이머 없이 표시만 — 진행도·위치·회전은 부른 쪽(StateTree Task)이 매 틱 넣는다(장풍).
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaGaugeRectActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaGaugeRectActor();

	// 크기 지정 + 배경 표시, 채움 0
	void Init(float InLength, float InHalfWidth);

	// 0~1 — 원점에서 Length * Progress까지 채움
	void SetProgress(float Progress);

	UPROPERTY(VisibleAnywhere, Category = "Gauge")
	TObjectPtr<UProceduralMeshComponent> GaugeMesh;

	// M_MirrorLaser "Base Color"(A = 불투명도). 배경은 다른 패턴 예고 장판과 같은 빨강 0.35(부채꼴·거울·리본 TelegraphColor),
	// 채움은 같은 빨강을 더 진하게
	UPROPERTY(EditDefaultsOnly, Category = "Gauge")
	FLinearColor BackgroundColor = FLinearColor(1.f, 0.f, 0.f, 0.35f);

	UPROPERTY(EditDefaultsOnly, Category = "Gauge")
	FLinearColor FillColor = FLinearColor(1.f, 0.f, 0.f, 0.7f);

	UPROPERTY(EditDefaultsOnly, Category = "Gauge")
	FName ColorParameterName = TEXT("Base Color");

	// 바닥 Z-fighting 방지 높이 (cm) — 채움은 배경보다 살짝 위
	UPROPERTY(EditDefaultsOnly, Category = "Gauge")
	float GroundOffset = 3.f;

protected:
	virtual void BeginPlay() override;

private:
	enum : int32 { SectionBackground = 0, SectionFill = 1 };

	float Length = 0.f;
	float HalfWidth = 0.f;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SectionMIDs;

	void BuildSection(int32 Section, float FillLength, float Z);
};
