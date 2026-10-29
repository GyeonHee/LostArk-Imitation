#include "Raid/EchidnaGaugeRectActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEchidnaGaugeRectActor::AEchidnaGaugeRectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GaugeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GaugeMesh"));
	SetRootComponent(GaugeMesh);
	GaugeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GaugeMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		GaugeMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaGaugeRectActor::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInterface* BaseMat = GaugeMesh->GetMaterial(0))
	{
		const FLinearColor Colors[2] = { BackgroundColor, FillColor };
		for (const FLinearColor& Color : Colors)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(ColorParameterName, Color);
			SectionMIDs.Add(MID);
		}
	}
}

void AEchidnaGaugeRectActor::Init(float InLength, float InHalfWidth)
{
	Length = FMath::Max(InLength, 0.f);
	HalfWidth = FMath::Max(InHalfWidth, 0.f);
	BuildSection(SectionBackground, Length, GroundOffset);
	BuildSection(SectionFill, 0.f, GroundOffset + 1.f);
}

void AEchidnaGaugeRectActor::SetProgress(float Progress)
{
	BuildSection(SectionFill, Length * FMath::Clamp(Progress, 0.f, 1.f), GroundOffset + 1.f);
}

void AEchidnaGaugeRectActor::BuildSection(int32 Section, float FillLength, float Z)
{
	if (FillLength <= KINDA_SMALL_NUMBER || HalfWidth <= KINDA_SMALL_NUMBER)
	{
		GaugeMesh->ClearMeshSection(Section);
		return;
	}

	// 위·아래 양쪽 감김 (다른 PMC 액터들과 같은 방식)
	const TArray<FVector> Verts = {
		FVector(0.f, -HalfWidth, Z), FVector(FillLength, -HalfWidth, Z),
		FVector(FillLength, HalfWidth, Z), FVector(0.f, HalfWidth, Z) };
	const TArray<int32> Tris = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
	GaugeMesh->CreateMeshSection(Section, Verts, Tris, {}, {}, {}, {}, false);
	if (SectionMIDs.IsValidIndex(Section))
	{
		GaugeMesh->SetMaterial(Section, SectionMIDs[Section]);
	}
}
