#include "Raid/EchidnaPoopMarkActor.h"
#include "Raid/EchidnaBoss.h"
#include "Raid/HexArena.h"
#include "Raid/HexTile.h"
#include "LoACharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 고리 조각 하나(사다리꼴) — 앞/뒤 양쪽 감김으로 넣어 노멀 방향과 무관하게 항상 보이게 (다른 PMC 액터들과 같은 방식)
	void AddRingQuad(TArray<FVector>& Verts, TArray<int32>& Tris,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C, D });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Tris.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}
}

AEchidnaPoopMarkActor::AEchidnaPoopMarkActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RingMesh"));
	SetRootComponent(RingMesh);
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		RingMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaPoopMarkActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 중이면 게이지도 보스와 같은 배율로 빨리 찬다 (Tick 기반이라 이 값만으로 충분)
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	if (UMaterialInterface* BaseMat = RingMesh->GetMaterial(0))
	{
		BackgroundMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		BackgroundMID->SetVectorParameterValue(ColorParameterName, BackgroundColor);
		FillMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		FillMID->SetVectorParameterValue(ColorParameterName, FillColor);
	}
}

void AEchidnaPoopMarkActor::Activate(ALoACharacter* InTarget, AHexArena* InArena, bool bWaitForStunEnd)
{
	Target = InTarget;
	Arena = InArena;
	Elapsed = 0.f;
	bActive = true;
	bFinished = false;
	bWaitingForStunEnd = bWaitForStunEnd;

	FollowTarget();
	BuildRing(0, 1.f);
	BuildRing(1, 0.f);
	RingMesh->SetVisibility(!bWaitingForStunEnd);
}

void AEchidnaPoopMarkActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bActive || bFinished) return;

	FollowTarget();

	// 기절이 풀릴 때까지는 숨긴 채 대기 — 풀리는 순간부터 게이지 시작
	if (bWaitingForStunEnd)
	{
		const ALoACharacter* Character = Target.Get();
		if (Character && Character->IsStunned()) return;

		bWaitingForStunEnd = false;
		RingMesh->SetVisibility(true);
	}

	Elapsed += DeltaTime;
	const float Progress = Duration > 0.f ? FMath::Clamp(Elapsed / Duration, 0.f, 1.f) : 1.f;
	BuildRing(1, Progress);

	if (Progress >= 1.f)
	{
		ConvertTileUnderTarget();
		bFinished = true;
		SetLifeSpan(0.2f);
	}
}

void AEchidnaPoopMarkActor::FollowTarget()
{
	ALoACharacter* Character = Target.Get();
	if (!Character) return;

	// GetActorLocation은 캡슐 중심이라 발밑으로 내린다 (장판이 떠 보이던 다른 패턴들과 같은 함정)
	FVector Feet = Character->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Feet.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}
	SetActorLocationAndRotation(Feet + FVector(0.f, 0.f, ZOffset), FRotator::ZeroRotator);
}

void AEchidnaPoopMarkActor::BuildRing(int32 SectionIndex, float Progress)
{
	if (Progress <= 0.f)
	{
		RingMesh->ClearMeshSection(SectionIndex);
		return;
	}

	// 탑다운 카메라(Yaw 0)에서 화면 위쪽 = +X, 오른쪽 = +Y → 각도를 +X에서 +Y 쪽으로 늘리면 화면상 시계방향
	const int32 Segments = FMath::Max(3, FMath::CeilToInt(ArcSegments * Progress));
	const float TotalRad = 2.f * PI * Progress;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = TotalRad * i / Segments;
		const float A1 = TotalRad * (i + 1) / Segments;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f);
		const FVector D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
		AddRingQuad(Verts, Tris, D0 * RingInnerRadius, D0 * RingOuterRadius, D1 * RingOuterRadius, D1 * RingInnerRadius);
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Verts.Num());

	RingMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	RingMesh->SetMaterial(SectionIndex, SectionIndex == 0 ? BackgroundMID.Get() : FillMID.Get());
}

void AEchidnaPoopMarkActor::ConvertTileUnderTarget()
{
	ALoACharacter* Character = Target.Get();
	AHexArena* HexArena = Arena.Get();
	if (!Character || !HexArena) return;

	FIntPoint Coord;
	if (!HexArena->WorldToTileCoord(Character->GetActorLocation(), Coord)) return;

	AHexTile* Tile = HexArena->GetTile(Coord);
	if (!Tile) return;

	// 이미 오염돼 있으면 변화 없음(활성 상태도 그대로).
	// 새로 생기는 장판은 비활성 — 활성으로 만들면 그 위에 서 있는 플레이어에게 생성 즉시 매혹 스택이 쌓인다
	if (Tile->GetTileType() != EHexTileType::PoopZone)
	{
		Tile->SetTileType(EHexTileType::PoopZone);
		Tile->SetPoopActive(false);
	}
}
