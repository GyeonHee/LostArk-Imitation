#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexArena.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class LOA_API AHexArena : public AActor
{
	GENERATED_BODY()

public:
	AHexArena();

	virtual void OnConstruction(const FTransform& Transform) override;

	// ── 타일 ──────────────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HexMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SideCount = 4;

	// 인접 타일 중심-중심 거리 cm (타일이 딱 붙는 거리)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (ClampMin = "10.0"))
	float TileSpacing = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Grid", meta = (ClampMin = "0.0"))
	float HexGap = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Grid")
	float TileYaw = 30.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Grid")
	int32 TileCount = 0;

	// ── 외곽 벽 ───────────────────────────────────────────────
	// 외곽 타일의 노출된 변마다 벽 조각을 배치 (SideCount=4 → 24개)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WallMeshes;

	// 1×1×1 단위 박스 메시 할당 — 코드에서 스케일로 크기 조정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	TObjectPtr<UStaticMesh> WallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallHeight = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallThickness = 30.f;

	// 타일 외곽선에서 추가 거리 cm (양수=바깥, 음수=안쪽)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallOffset = 0.f;

	// 벽 한 조각 길이 cm. 0 = 자동(TileSpacing/√3), 양수 = 직접 지정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallLengthOverride = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	int32 WallCount = 0;

private:
	void RebuildGrid();
	void RebuildWalls(int32 R, float D);

	static bool IsValidTile(int32 q, int32 r, int32 R);
};
