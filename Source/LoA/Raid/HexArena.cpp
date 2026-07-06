#include "Raid/HexArena.h"
#include "Raid/HexTile.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

// 6방향 이웃 정의 (Pointy-top Axial 기준)
//   인덱스:  0     1     2      3     4      5
//   방향:  오른쪽 우상  좌상   왼쪽  좌하   우하
static const int32 GDQ[6] = { 1,  0, -1, -1,  0,  1 };
static const int32 GDR[6] = { 0,  1,  1,  0, -1, -1 };
// 각 방향으로 나가는 외향 법선 각도 (도)
static const float GEdgeAngle[6] = { 0.f, 60.f, 120.f, 180.f, 240.f, 300.f };

AHexArena::AHexArena()
{
	PrimaryActorTick.bCanEverTick = false;

	HexMeshes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HexMeshes"));
	HexMeshes->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HexMeshes->SetCollisionProfileName(TEXT("BlockAll"));
	HexMeshes->SetRenderCustomDepth(true);
	HexMeshes->SetCustomDepthStencilValue(1);
	SetRootComponent(HexMeshes);

	WallMeshes = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WallMeshes"));
	WallMeshes->SetupAttachment(RootComponent);
	WallMeshes->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallMeshes->SetCollisionProfileName(TEXT("BlockAll"));
	WallMeshes->bUseComplexAsSimpleCollision = true;
}

void AHexArena::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildGrid();
}

void AHexArena::BeginPlay()
{
	Super::BeginPlay();

	// 에디터 프리뷰용 HISM은 실제 플레이에서는 숨기고, 오버랩/타입 전환이 가능한
	// 개별 AHexTile 액터로 교체한다 (자세한 이유는 HexTile.h 주석 참고)
	HexMeshes->SetVisibility(false);
	HexMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpawnGameplayTiles();
}

void AHexArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGameplayTiles();
	Super::EndPlay(EndPlayReason);
}

bool AHexArena::IsValidTile(int32 q, int32 r, int32 R)
{
	return FMath::Abs(q) <= R && FMath::Abs(r) <= R && FMath::Abs(q + r) <= R;
}

FTransform AHexArena::ComputeTileLocalTransform(int32 q, int32 r) const
{
	const float Sqrt3 = FMath::Sqrt(3.f);
	const float D = TileSpacing + HexGap;

	const float X = D * (q + r * 0.5f);
	const float Y = D * Sqrt3 * 0.5f * r;

	FTransform T;
	T.SetLocation(FVector(X, Y, 0.f));
	T.SetRotation(FRotator(0.f, TileYaw, 0.f).Quaternion());
	return T;
}

void AHexArena::RebuildGrid()
{
	HexMeshes->ClearInstances();

	if (!HexMeshes->GetStaticMesh()) return;

	const int32 R = SideCount - 1;
	const float D = TileSpacing + HexGap;

	int32 Count = 0;

	for (int32 q = -R; q <= R; q++)
	{
		const int32 rMin = FMath::Max(-R, -q - R);
		const int32 rMax = FMath::Min(R, -q + R);

		for (int32 r = rMin; r <= rMax; r++)
		{
			HexMeshes->AddInstance(ComputeTileLocalTransform(q, r), false);
			Count++;
		}
	}

	TileCount = Count;

	RebuildWalls(R, D);
}

void AHexArena::SpawnGameplayTiles()
{
	ClearGameplayTiles();

	UWorld* World = GetWorld();
	if (!World) return;

	const int32 R = SideCount - 1;
	const float TileRadius = TileSpacing * 0.5f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ClassToSpawn = TileClass ? TileClass.Get() : AHexTile::StaticClass();

	for (int32 q = -R; q <= R; q++)
	{
		const int32 rMin = FMath::Max(-R, -q - R);
		const int32 rMax = FMath::Min(R, -q + R);

		for (int32 r = rMin; r <= rMax; r++)
		{
			const FTransform WorldT = ComputeTileLocalTransform(q, r) * GetActorTransform();

			AHexTile* Tile = World->SpawnActor<AHexTile>(ClassToSpawn, WorldT, SpawnParams);
			if (!Tile) continue;

			const FIntPoint Coord(q, r);
			Tile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Tile->InitTile(this, Coord, TileRadius);
			TileMap.Add(Coord, Tile);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[HexArena] SpawnGameplayTiles: spawned %d tiles (TileClass=%s)"),
		TileMap.Num(), *GetNameSafe(ClassToSpawn));
}

void AHexArena::ClearGameplayTiles()
{
	for (const TPair<FIntPoint, TObjectPtr<AHexTile>>& Pair : TileMap)
	{
		if (AHexTile* Tile = Pair.Value.Get())
		{
			Tile->Destroy();
		}
	}
	TileMap.Empty();
}

AHexTile* AHexArena::GetTile(const FIntPoint& Coord) const
{
	if (const TObjectPtr<AHexTile>* Found = TileMap.Find(Coord))
	{
		return Found->Get();
	}
	return nullptr;
}

void AHexArena::NotifyTileTypeChanged(const FIntPoint& ChangedCoord)
{
	const int32 R = SideCount - 1;

	// 방금 타입이 바뀐 타일의 6방향 이웃 각각에 대해, "그 이웃이 실제로 갖고 있는 모든 이웃(그리드 밖 제외)이
	// 전부 PoopZone인지" 검사. 외곽/코너 타일은 실제 이웃이 3~5개뿐이라 그 개수만큼만 만족하면 됨
	// (모든 타일이 개화 가능해야 하므로 그리드 밖 방향은 검사에서 제외하고 실제 존재하는 이웃만 확인)
	for (int32 d = 0; d < 6; d++)
	{
		const FIntPoint NeighborCoord(ChangedCoord.X + GDQ[d], ChangedCoord.Y + GDR[d]);
		if (!IsValidTile(NeighborCoord.X, NeighborCoord.Y, R)) continue;

		AHexTile* NeighborTile = GetTile(NeighborCoord);
		if (!NeighborTile || NeighborTile->GetTileType() != EHexTileType::Normal) continue;

		bool bAllPoop = true;
		bool bHasAnyNeighbor = false;
		for (int32 dd = 0; dd < 6; dd++)
		{
			const FIntPoint SurroundCoord(NeighborCoord.X + GDQ[dd], NeighborCoord.Y + GDR[dd]);
			if (!IsValidTile(SurroundCoord.X, SurroundCoord.Y, R)) continue;

			bHasAnyNeighbor = true;
			const AHexTile* SurroundTile = GetTile(SurroundCoord);
			if (!SurroundTile || SurroundTile->GetTileType() != EHexTileType::PoopZone)
			{
				bAllPoop = false;
				break;
			}
		}

		if (bAllPoop && bHasAnyNeighbor)
		{
			NeighborTile->SetTileType(EHexTileType::Flower);
		}
	}
}

namespace
{
	// 4점으로 쿼드 추가. UE의 실제 front-face 감김 방향에 의존하지 않도록
	// 양쪽 감김 순서(앞/뒤)를 모두 추가해 항상 양면이 보이게 한다.
	void AddQuad(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals,
		TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Normal)
	{
		FVector TangentVec = FVector::CrossProduct(Normal, FVector::UpVector);
		TangentVec = TangentVec.IsNearlyZero() ? FVector::ForwardVector : TangentVec.GetSafeNormal();

		const int32 Base = Verts.Num();
		Verts.Add(P0); Verts.Add(P1); Verts.Add(P2); Verts.Add(P3);

		for (int32 i = 0; i < 4; i++)
		{
			Normals.Add(Normal);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(TangentVec, false));
		}

		UVs.Add(FVector2D(0.f, 0.f));
		UVs.Add(FVector2D(1.f, 0.f));
		UVs.Add(FVector2D(1.f, 1.f));
		UVs.Add(FVector2D(0.f, 1.f));

		// 정면 감김
		Tris.Add(Base + 0); Tris.Add(Base + 1); Tris.Add(Base + 2);
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 3);
		// 역방향 감김 (양면 렌더링)
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 1);
		Tris.Add(Base + 0); Tris.Add(Base + 3); Tris.Add(Base + 2);
	}
}

void AHexArena::RebuildWalls(int32 R, float D)
{
	WallMeshes->ClearAllMeshSections();
	WallCount = 0;

	const float Sqrt3 = FMath::Sqrt(3.f);
	// 타일의 실제 반지름(중심→꼭짓점). 배치 간격 D에는 HexGap이 섞여 있으므로
	// 꼭짓점 계산은 순수 TileSpacing 기준으로 한다 (기존 EdgeLength 공식과 동일 기준).
	const float RHex = TileSpacing / Sqrt3;

	struct FRawEdge
	{
		FVector2D CornerA;
		FVector2D CornerB;
		FVector2D Normal;
	};
	TArray<FRawEdge> RawEdges;

	for (int32 q = -R; q <= R; q++)
	{
		const int32 rMin = FMath::Max(-R, -q - R);
		const int32 rMax = FMath::Min(R, -q + R);

		for (int32 r = rMin; r <= rMax; r++)
		{
			const float TileX = D * (q + r * 0.5f);
			const float TileY = D * Sqrt3 * 0.5f * r;

			for (int32 d = 0; d < 6; d++)
			{
				// 이웃 타일이 없는 방향 = 외곽 노출 변
				if (IsValidTile(q + GDQ[d], r + GDR[d], R)) continue;

				const float AngA = FMath::DegreesToRadians(GEdgeAngle[d] - 30.f);
				const float AngB = FMath::DegreesToRadians(GEdgeAngle[d] + 30.f);
				const float AngN = FMath::DegreesToRadians(GEdgeAngle[d]);

				FRawEdge E;
				E.CornerA = FVector2D(TileX + RHex * FMath::Cos(AngA), TileY + RHex * FMath::Sin(AngA));
				E.CornerB = FVector2D(TileX + RHex * FMath::Cos(AngB), TileY + RHex * FMath::Sin(AngB));
				E.Normal = FVector2D(FMath::Cos(AngN), FMath::Sin(AngN));
				RawEdges.Add(E);
			}
		}
	}

	if (RawEdges.Num() == 0) return;

	// ── 정점 용접(weld) ──
	// 서로 다른 두 타일의 배치 간격(D)에는 HexGap이 포함되어 있어, 각 타일 자체 반지름(RHex)만으로
	// 계산한 꼭짓점끼리는 정확히 겹치지 않고 HexGap 크기만큼 살짝 어긋난다.
	// 허용오차 내에서 하나의 정점으로 합쳐 이음매 없는 벽 윤곽선을 만든다.
	const float WeldTolerance = FMath::Clamp(HexGap * 2.f, 1.f, 50.f);
	const float WeldToleranceSq = WeldTolerance * WeldTolerance;

	struct FWeldedVertex
	{
		FVector2D Pos;
		TArray<FVector2D> Normals;
	};
	TArray<FWeldedVertex> Vertices;

	auto WeldVertex = [&Vertices, WeldToleranceSq](const FVector2D& Pos, const FVector2D& Normal) -> int32
	{
		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			if (FVector2D::DistSquared(Vertices[i].Pos, Pos) <= WeldToleranceSq)
			{
				Vertices[i].Normals.AddUnique(Normal);
				return i;
			}
		}
		FWeldedVertex NewV;
		NewV.Pos = Pos;
		NewV.Normals.Add(Normal);
		return Vertices.Add(NewV);
	};

	struct FWeldedEdge
	{
		int32 VA;
		int32 VB;
		FVector2D Normal;
	};
	TArray<FWeldedEdge> WeldedEdges;
	WeldedEdges.Reserve(RawEdges.Num());

	for (const FRawEdge& E : RawEdges)
	{
		FWeldedEdge WE;
		WE.VA = WeldVertex(E.CornerA, E.Normal);
		WE.VB = WeldVertex(E.CornerB, E.Normal);
		WE.Normal = E.Normal;
		WeldedEdges.Add(WE);
	}

	// ── 정점별 미터(miter) 오프셋 방향/배율 ──
	// 한 꼭짓점에 모이는 변들의 외향 노멀을 합산해 이등분 방향을 구하고,
	// 그 방향으로 1/cos(반각)만큼 늘려 오프셋해야 각 변에서 수직 거리가 균일하게 유지된다.
	// (이게 바깥쪽 변이 안쪽보다 길어지는 사다리꼴 벽의 핵심 — 인접 조각과 꼭짓점을 정확히 공유한다)
	TArray<FVector2D> MiterDir;
	TArray<float> MiterFactor;
	MiterDir.SetNum(Vertices.Num());
	MiterFactor.SetNum(Vertices.Num());

	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& N : Vertices[i].Normals) Sum += N;

		FVector2D Dir = Sum.GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = Vertices[i].Normals[0];

		const float Dot = FMath::Max(FVector2D::DotProduct(Dir, Vertices[i].Normals[0]), 0.15f);

		MiterDir[i] = Dir;
		MiterFactor[i] = 1.f / Dot;
	}

	auto OffsetPoint = [&Vertices, &MiterDir, &MiterFactor](int32 VertexIdx, float Dist) -> FVector2D
	{
		return Vertices[VertexIdx].Pos + MiterDir[VertexIdx] * Dist * MiterFactor[VertexIdx];
	};

	// ── 메시 생성 ──
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	for (const FWeldedEdge& WE : WeldedEdges)
	{
		const FVector2D InnerA2D = OffsetPoint(WE.VA, WallOffset);
		const FVector2D InnerB2D = OffsetPoint(WE.VB, WallOffset);
		const FVector2D OuterA2D = OffsetPoint(WE.VA, WallOffset + WallThickness);
		const FVector2D OuterB2D = OffsetPoint(WE.VB, WallOffset + WallThickness);

		const FVector InnerA_b(InnerA2D, 0.f), InnerB_b(InnerB2D, 0.f);
		const FVector OuterA_b(OuterA2D, 0.f), OuterB_b(OuterB2D, 0.f);
		const FVector HeightVec(0.f, 0.f, WallHeight);
		const FVector InnerA_t = InnerA_b + HeightVec, InnerB_t = InnerB_b + HeightVec;
		const FVector OuterA_t = OuterA_b + HeightVec, OuterB_t = OuterB_b + HeightVec;

		const FVector OuterNormal(WE.Normal.X, WE.Normal.Y, 0.f);

		AddQuad(Verts, Tris, Normals, UVs, Colors, Tangents, InnerA_t, InnerB_t, OuterB_t, OuterA_t, FVector::UpVector);
		AddQuad(Verts, Tris, Normals, UVs, Colors, Tangents, OuterA_b, OuterB_b, OuterB_t, OuterA_t, OuterNormal);
		AddQuad(Verts, Tris, Normals, UVs, Colors, Tangents, InnerA_b, InnerB_b, InnerB_t, InnerA_t, -OuterNormal);

		WallCount++;
	}

	WallMeshes->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, true);
	if (WallMaterial)
	{
		WallMeshes->SetMaterial(0, WallMaterial);
	}

	UE_LOG(LogTemp, Warning, TEXT("[HexArena] RebuildWalls: RawEdges=%d WeldedVerts=%d Verts=%d Tris=%d WallHeight=%.1f WallThickness=%.1f HasMaterial=%d"),
		RawEdges.Num(), Vertices.Num(), Verts.Num(), Tris.Num(), WallHeight, WallThickness, WallMaterial != nullptr);
}
