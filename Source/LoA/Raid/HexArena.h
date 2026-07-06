#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexArena.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class AHexTile;

UCLASS()
class LOA_API AHexArena : public AActor
{
	GENERATED_BODY()

public:
	AHexArena();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// ── 개별 타일 (게임플레이 전용, BeginPlay에서 스폰) ───────────
	// HexMeshes는 에디터 프리뷰/비주얼 전용으로 남고, 실제 오버랩/타입 전환은
	// BeginPlay에서 좌표마다 스폰하는 AHexTile 액터가 담당한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile")
	TSubclassOf<AHexTile> TileClass;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Hex Tile")
	TMap<FIntPoint, TObjectPtr<AHexTile>> TileMap;

	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	AHexTile* GetTile(const FIntPoint& Coord) const;

	// AHexTile::SetTileType에서 호출 — 바뀐 타일 주변 6칸을 훑어
	// 전부 PoopZone이 된 이웃이 있으면 그 이웃을 Flower로 전환한다
	void NotifyTileTypeChanged(const FIntPoint& ChangedCoord);

	// ── 외곽 벽 ───────────────────────────────────────────────
	// 외곽 타일의 노출된 변마다 미터(miter) 접합된 사다리꼴 벽 조각을 프로시저럴 메시로 생성
	// (안쪽은 타일 실제 변 길이, 바깥쪽은 인접 조각과 꼭짓점을 공유하도록 자동으로 늘어남)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> WallMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallHeight = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallThickness = 30.f;

	// 타일 외곽선에서 추가 거리 cm (양수=바깥, 음수=안쪽)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	float WallOffset = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	int32 WallCount = 0;

private:
	void RebuildGrid();
	void RebuildWalls(int32 R, float D);
	FTransform ComputeTileLocalTransform(int32 q, int32 r) const;

	void SpawnGameplayTiles();
	void ClearGameplayTiles();

	static bool IsValidTile(int32 q, int32 r, int32 R);
};
