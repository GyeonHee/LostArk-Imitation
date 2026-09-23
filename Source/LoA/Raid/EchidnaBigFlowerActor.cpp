#include "Raid/EchidnaBigFlowerActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 앞/뒤 양쪽 감김 (다른 PMC 액터들과 같은 방식) — 꽃잎은 양면이 다 보여야 한다
	void AddFlowerQuad(TArray<FVector>& Verts, TArray<int32>& Tris,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C, D });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Tris.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}

	void CommitSection(UProceduralMeshComponent* Mesh, int32 Section, const TArray<FVector>& Verts, const TArray<int32>& Tris)
	{
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Verts.Num());
		TArray<FVector2D> UVs;
		UVs.Init(FVector2D::ZeroVector, Verts.Num());
		Mesh->CreateMeshSection(Section, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	}
}

AEchidnaBigFlowerActor::AEchidnaBigFlowerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 스케일/회전 애니메이션은 이 메시에만 건다 (액터 위치는 타일 윗면에 고정)
	FlowerMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FlowerMesh"));
	FlowerMesh->SetupAttachment(Root);
	FlowerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlowerMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		FlowerMesh->SetMaterial(0, MatFinder.Object);
	}

	// 레퍼런스: 바깥은 붉은 분홍, 안으로 갈수록 연한 살구/크림
	LayerColors = {
		FLinearColor(2.4f, 0.22f, 0.45f, 0.95f),	// 바깥 — 붉은 분홍
		FLinearColor(2.8f, 0.55f, 0.7f, 0.95f),
		FLinearColor(3.2f, 1.6f, 1.1f, 0.95f),	// 안쪽 — 살구/크림
	};
}

void AEchidnaBigFlowerActor::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* BaseMat = FlowerMesh->GetMaterial(0))
	{
		// 섹션 순서: 꽃잎 겹(0 ~ LayerCount-1) → 꽃술(LayerCount) → 잎(LayerCount+1)
		for (int32 i = 0; i <= LayerCount + 1; ++i)
		{
			FLinearColor Color;
			if (i == LayerCount) Color = CenterColor;
			else if (i == LayerCount + 1) Color = LeafColor;
			else Color = LayerColors.Num() > 0 ? LayerColors[FMath::Min(i, LayerColors.Num() - 1)] : FLinearColor::White;
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(ColorParameterName, Color);
			SectionMIDs.Add(MID);
		}
	}

	BuildFlower();
	FlowerMesh->SetRelativeScale3D(FVector(0.01f));
}

void AEchidnaBigFlowerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;

	// 피어나기 — 살짝 튀어나왔다 자리잡는 ease-out-back
	const float T = BloomDuration > 0.f ? FMath::Clamp(Age / BloomDuration, 0.f, 1.f) : 1.f;
	const float C1 = 1.4f;
	const float Bloom = 1.f + (C1 + 1.f) * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
	FlowerMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, Bloom)));

	const float Bob = FMath::Sin(Age * 1.6f) * BobAmplitude;
	FlowerMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, HoverHeight + Bob), FRotator(0.f, Age * SpinSpeed, 0.f));
}

void AEchidnaBigFlowerActor::BuildFlower()
{
	for (int32 i = 0; i < LayerCount; ++i)
	{
		BuildLayer(i);
	}
	BuildCenter();
	BuildLeaves();
}

void AEchidnaBigFlowerActor::BuildLeaves()
{
	const int32 Section = LayerCount + 1;
	if (LeafCount <= 0)
	{
		FlowerMesh->ClearMeshSection(Section);
		return;
	}

	// 꽃잎보다 길고 가늘게, 거의 바닥에 눕혀서(살짝 처지게) 꽃잎 사이로 삐져나오게
	const float Length = PetalLength * 1.3f;
	const float HalfWidth = Length * 0.14f;
	constexpr int32 LengthSteps = 10;
	constexpr int32 WidthSteps = 2;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	for (int32 l = 0; l < LeafCount; ++l)
	{
		const FRotator LeafRot(-6.f, 45.f + 360.f * l / LeafCount, 0.f);
		auto LeafPoint = [&](int32 Li, int32 Wi)
		{
			const float T = static_cast<float>(Li) / LengthSteps;
			const float U = static_cast<float>(Wi) / WidthSteps * 2.f - 1.f;
			const float W = HalfWidth * FMath::Sin(PI * FMath::Pow(T, 0.8f));
			return LeafRot.RotateVector(FVector(Length * T, W * U, 6.f * FMath::Abs(U))) + FVector(0.f, 0.f, -8.f);
		};
		for (int32 Li = 0; Li < LengthSteps; ++Li)
		{
			for (int32 Wi = 0; Wi < WidthSteps; ++Wi)
			{
				AddFlowerQuad(Verts, Tris, LeafPoint(Li, Wi), LeafPoint(Li + 1, Wi), LeafPoint(Li + 1, Wi + 1), LeafPoint(Li, Wi + 1));
			}
		}
	}

	CommitSection(FlowerMesh, Section, Verts, Tris);
	if (SectionMIDs.IsValidIndex(Section))
	{
		FlowerMesh->SetMaterial(Section, SectionMIDs[Section]);
	}
}

void AEchidnaBigFlowerActor::BuildLayer(int32 LayerIndex)
{
	// 바깥 겹일수록 크고 눕고, 안쪽일수록 작고 세운다
	const float Scale = 1.f - 0.22f * LayerIndex;
	const float Pitch = 12.f + 24.f * LayerIndex;
	const int32 Count = FMath::Max(3, OuterPetalCount - LayerIndex);
	const float YawOffset = LayerIndex * 180.f / Count;	// 겹마다 엇갈리게
	const float Length = PetalLength * Scale;
	const float HalfWidth = Length * PetalWidthRatio * 0.5f;
	const float BaseRadius = 12.f * (LayerCount - LayerIndex);	// 꽃잎 뿌리를 중심에서 살짝 띄움
	const float LayerZ = 4.f * LayerIndex;						// 안쪽 겹이 위로 보이게

	constexpr int32 LengthSteps = 10;
	constexpr int32 WidthSteps = 4;

	TArray<FVector> Verts;
	TArray<int32> Tris;

	for (int32 p = 0; p < Count; ++p)
	{
		const FRotator PetalRot(Pitch, YawOffset + 360.f * p / Count, 0.f);

		// 꽃잎 로컬 좌표: X = 길이 방향, Y = 폭, Z = 오목한 정도
		auto PetalPoint = [&](int32 Li, int32 Wi)
		{
			const float T = static_cast<float>(Li) / LengthSteps;
			const float U = static_cast<float>(Wi) / WidthSteps * 2.f - 1.f;	// -1 ~ 1
			const float W = HalfWidth * FMath::Sin(PI * FMath::Pow(T, 0.7f));	// 뿌리·끝은 뾰족, 60% 지점이 가장 넓음
			const float Z = PetalCup * T * T + PetalCup * 0.35f * U * U * T;	// 끝과 가장자리가 말려 올라감
			const FVector Local(BaseRadius + Length * T, W * U, Z);
			return PetalRot.RotateVector(Local) + FVector(0.f, 0.f, LayerZ);
		};

		for (int32 Li = 0; Li < LengthSteps; ++Li)
		{
			for (int32 Wi = 0; Wi < WidthSteps; ++Wi)
			{
				AddFlowerQuad(Verts, Tris,
					PetalPoint(Li, Wi), PetalPoint(Li + 1, Wi), PetalPoint(Li + 1, Wi + 1), PetalPoint(Li, Wi + 1));
			}
		}
	}

	CommitSection(FlowerMesh, LayerIndex, Verts, Tris);
	if (SectionMIDs.IsValidIndex(LayerIndex))
	{
		FlowerMesh->SetMaterial(LayerIndex, SectionMIDs[LayerIndex]);
	}
}

void AEchidnaBigFlowerActor::BuildCenter()
{
	// 꽃술 — 작은 돔
	const int32 Section = LayerCount;
	const float Radius = PetalLength * 0.16f;
	const float Height = Radius * 0.8f;
	const float Z0 = 4.f * LayerCount;
	constexpr int32 Rings = 5;
	constexpr int32 Segs = 16;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	auto DomePoint = [&](int32 Ri, int32 Si)
	{
		const float Phi = (PI * 0.5f) * Ri / Rings;	// 0 = 가장자리, π/2 = 꼭대기
		const float Theta = 2.f * PI * Si / Segs;
		return FVector(FMath::Cos(Phi) * Radius * FMath::Cos(Theta), FMath::Cos(Phi) * Radius * FMath::Sin(Theta), Z0 + FMath::Sin(Phi) * Height);
	};

	for (int32 Ri = 0; Ri < Rings; ++Ri)
	{
		for (int32 Si = 0; Si < Segs; ++Si)
		{
			AddFlowerQuad(Verts, Tris, DomePoint(Ri, Si), DomePoint(Ri, Si + 1), DomePoint(Ri + 1, Si + 1), DomePoint(Ri + 1, Si));
		}
	}

	CommitSection(FlowerMesh, Section, Verts, Tris);
	if (SectionMIDs.IsValidIndex(Section))
	{
		FlowerMesh->SetMaterial(Section, SectionMIDs[Section]);
	}
}
