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

	// 월드 좌표가 속한 타일 좌표(q,r) — 그리드 밖이면 false
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	bool WorldToTileCoord(const FVector& WorldLocation, FIntPoint& OutCoord) const;

	// ── 레이드 시작 배치 ──────────────────────────────────────
	// BeginPlay 다음 틱(플레이어·보스가 자리 잡은 뒤)에 SetupInitialLayout()이 한 번 실행된다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Initial Layout")
	bool bSetupInitialLayoutOnBeginPlay = true;

	// 랜덤으로 오염시킬 타일 수 (폰이 서 있는 타일만 제외 — 파란 테두리 타일도 오염될 수 있음, 꽃이 피는 배치는 피함).
	// 처음엔 전부 비활성(밟아도 무해) — 특정 패턴이 SetAllPoopTilesActive(true)로 켠다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Initial Layout", meta = (ClampMin = "0"))
	int32 InitialPoopTileCount = 8;

	// 파란 테두리 타일 2개 사이 헥스 거리 (2 = 사이에 1칸). 첫 번째는 외곽, 두 번째는 외곽이 아닌 안쪽 타일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Initial Layout", meta = (ClampMin = "1"))
	int32 MarkerTileDistance = 2;

	// 이번 레이드에서 파란 테두리가 켜진 타일들 — [0]=외곽, [1]=안쪽. 반정산 패턴에서 참조
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Hex Tile|Initial Layout")
	TArray<FIntPoint> MarkerTileCoords;

	UFUNCTION(BlueprintCallable, Category = "Hex Tile|Initial Layout")
	void SetupInitialLayout();

	// 외곽 링 좌표를 링을 따라 순서대로 (인접한 원소끼리 실제로 이웃)
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	TArray<FIntPoint> GetOuterRingCoords() const;

	// 타일 윗면 중심(월드) — 패턴 액터를 타일 위에 세울 때. 타일이 없으면 false
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	bool GetTileTopLocation(const FIntPoint& Coord, FVector& OutLocation) const;

	// 전 타일 빨간 점멸 켜기/끄기 (거울잇기 실패 연출)
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	void SetAllTilesDangerFlash(bool bFlash);

	// 전 타일 노란(거울잇기) 테두리 해제
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	void ClearAllLinkHighlights();

	// 두 타일 좌표 사이 헥스 거리
	UFUNCTION(BlueprintPure, Category = "Hex Tile")
	static int32 GetHexDistance(const FIntPoint& A, const FIntPoint& B);

	// ── 큰 꽃 매혹 오라 ──────────────────────────────────────
	// Flower 타일과의 헥스 거리가 FlowerAuraRange 이하인 타일(꽃 타일 자신 포함)에 서 있으면
	// FlowerCharmInterval마다 FlowerCharmAmount 매혹 스택. 꽃이 여러 개여도 한 번에 한 번만
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Flower")
	int32 FlowerAuraRange = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Flower")
	float FlowerCharmInterval = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Flower")
	int32 FlowerCharmAmount = 1;

	// 모든 오염 장판을 한꺼번에 활성/비활성 — 오염 장판이 활성화되는 패턴에서 호출
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	void SetAllPoopTilesActive(bool bActive);

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

	// 벽 안쪽 면을 위로 연장한 보이지 않는 충돌벽 — 백스텝/넉백으로 공중에 뜬 캐릭터가 벽 위에 올라가 갇히는 것 방지.
	// Pawn만 막는다(Visibility 등은 무시) — 벽과 같은 BlockAll이면 카메라 쪽 가장자리에서 마우스 클릭 이동 트레이스를 가로챈다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> BarrierMesh;

	// 벽 윗면보다 더 솟는 높이 (cm, 0이면 충돌벽 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Wall", meta = (ClampMin = "0.0"))
	float InvisibleBarrierHeight = 1500.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Wall")
	int32 WallCount = 0;

private:
	void RebuildGrid();
	void RebuildWalls(int32 R, float D);
	FTransform ComputeTileLocalTransform(int32 q, int32 r) const;

	void SpawnGameplayTiles();

	FTimerHandle FlowerAuraTimerHandle;
	void TickFlowerAura();
	void ClearGameplayTiles();

	static bool IsValidTile(int32 q, int32 r, int32 R);
};
