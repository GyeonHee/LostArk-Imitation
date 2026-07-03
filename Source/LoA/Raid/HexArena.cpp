#include "Raid/HexArena.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

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

	WallMeshes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WallMeshes"));
	WallMeshes->SetupAttachment(RootComponent);
	WallMeshes->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallMeshes->SetCollisionProfileName(TEXT("BlockAll"));
}

void AHexArena::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildGrid();
}

bool AHexArena::IsValidTile(int32 q, int32 r, int32 R)
{
	return FMath::Abs(q) <= R && FMath::Abs(r) <= R && FMath::Abs(q + r) <= R;
}

void AHexArena::RebuildGrid()
{
	HexMeshes->ClearInstances();

	if (!HexMeshes->GetStaticMesh()) return;

	const int32 R = SideCount - 1;
	const float Sqrt3 = FMath::Sqrt(3.f);
	const float D = TileSpacing + HexGap;

	int32 Count = 0;

	for (int32 q = -R; q <= R; q++)
	{
		const int32 rMin = FMath::Max(-R, -q - R);
		const int32 rMax = FMath::Min(R, -q + R);

		for (int32 r = rMin; r <= rMax; r++)
		{
			const float X = D * (q + r * 0.5f);
			const float Y = D * Sqrt3 * 0.5f * r;

			FTransform T;
			T.SetLocation(FVector(X, Y, 0.f));
			T.SetRotation(FRotator(0.f, TileYaw, 0.f).Quaternion());
			HexMeshes->AddInstance(T, false);
			Count++;
		}
	}

	TileCount = Count;

	RebuildWalls(R, D);
}

void AHexArena::RebuildWalls(int32 R, float D)
{
	WallMeshes->ClearInstances();

	if (!WallMesh) return;
	WallMeshes->SetStaticMesh(WallMesh);

	const float Sqrt3 = FMath::Sqrt(3.f);

	/*
	 * 타일 중심 → 변 중점까지의 거리 = Apothem = TileSpacing / 2
	 *   (TileSpacing = 닿아있을 때 중심간 거리 = 타일 flat-to-flat)
	 *
	 * 벽 위치 = 타일 중심 + (TileSpacing/2 + WallOffset) * 외향 단위벡터
	 * 벽 길이 = TileSpacing / sqrt(3)  (타일 한 변의 실제 길이)
	 * 벽 스케일 = (두께, 길이, 높이)  — 단위 메시(1×1×1) 기준
	 */
	// 벽 안쪽 면이 타일 외곽선에 정렬 → 내부에서 벽 속이 안 보임
	const float EdgeDist = TileSpacing * 0.5f + WallThickness * 0.5f + WallOffset;
	const float EdgeLength = (WallLengthOverride > 0.f) ? WallLengthOverride : (TileSpacing / Sqrt3);
	const FVector WallScale(WallThickness, EdgeLength, WallHeight);

	int32 Count = 0;

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

				const float AngleRad = FMath::DegreesToRadians(GEdgeAngle[d]);

				FTransform T;
				T.SetLocation(FVector(
					TileX + EdgeDist * FMath::Cos(AngleRad),
					TileY + EdgeDist * FMath::Sin(AngleRad),
					WallHeight * 0.5f
				));
				// X축이 외향 법선 방향을 향하도록 회전
				T.SetRotation(FRotator(0.f, GEdgeAngle[d], 0.f).Quaternion());
				T.SetScale3D(WallScale);

				WallMeshes->AddInstance(T, false);
				Count++;
			}
		}
	}

	WallCount = Count;
}
