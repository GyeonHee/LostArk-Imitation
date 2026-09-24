#include "Raid/EchidnaSwingZoneActor.h"
#include "Raid/EchidnaBoss.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

AEchidnaSwingZoneActor::AEchidnaSwingZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	ZoneMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ZoneMesh"));
	SetRootComponent(ZoneMesh);
	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		ZoneMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaSwingZoneActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — Tick 기반 진행은 이 값으로 자동 가속
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	if (UMaterialInterface* BaseMat = ZoneMesh->GetMaterial(0))
	{
		const FLinearColor Colors[SectionCount] = { BackgroundColor, FillColor, BorderColor, ExplodeColor };
		for (const FLinearColor& Color : Colors)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(ColorParameterName, Color);
			SectionMIDs.Add(MID);
		}
	}
}

void AEchidnaSwingZoneActor::Activate(float InDamageRatio, AController* InInstigator, float SafeRadiusOverride)
{
	DamageRatio = InDamageRatio;
	InstigatorController = InInstigator;
	if (SafeRadiusOverride >= 0.f) SafeRadius = SafeRadiusOverride;

	Phase = EEchidnaSwingZonePhase::Telegraph;
	PhaseElapsed = 0.f;

	BuildRing(SectionBackground, SafeRadius, OuterRadius);
	BuildRing(SectionBorder, SafeRadius, SafeRadius + BorderWidth);
	BuildRing(SectionFill, 0.f, 0.f);
}

void AEchidnaSwingZoneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PhaseElapsed += DeltaTime;

	switch (Phase)
	{
	case EEchidnaSwingZonePhase::Telegraph:
	{
		// 안전 원 경계에서 바깥쪽으로 차오름 — 끝까지 차면 터진다
		const float Progress = TelegraphDuration > 0.f ? FMath::Clamp(PhaseElapsed / TelegraphDuration, 0.f, 1.f) : 1.f;
		BuildRing(SectionFill, SafeRadius, FMath::Lerp(SafeRadius, OuterRadius, Progress));
		if (Progress >= 1.f)
		{
			Explode();
		}
		break;
	}
	case EEchidnaSwingZonePhase::Exploded:
		if (PhaseElapsed >= ExplodeLinger)
		{
			Phase = EEchidnaSwingZonePhase::Done;
			BuildRing(SectionExplode, 0.f, 0.f);
			SetLifeSpan(0.05f);
		}
		break;
	default:
		break;
	}
}

void AEchidnaSwingZoneActor::Explode()
{
	Phase = EEchidnaSwingZonePhase::Exploded;
	PhaseElapsed = 0.f;

	BuildRing(SectionBackground, 0.f, 0.f);
	BuildRing(SectionFill, 0.f, 0.f);
	BuildRing(SectionBorder, 0.f, 0.f);
	BuildRing(SectionExplode, SafeRadius, OuterRadius);

	// 안전 원 밖이면 거리와 무관하게 맞음 (캐릭터 중심 기준 — 원 경계에 걸쳐 있으면 중심이 안에 있어야 안전)
	const FVector Center = GetActorLocation();
	for (TActorIterator<ALoACharacter> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist2D(It->GetActorLocation(), Center);
		if (Dist > SafeRadius)
		{
			UGameplayStatics::ApplyDamage(*It, It->GetMaxHP() * DamageRatio, InstigatorController.Get(), this, UDamageType::StaticClass());
			UE_LOG(LogLoA, Log, TEXT("[SwingZone] %s 피격 — 안전 원 밖 (거리 %.0f > %.0f)"), *It->GetName(), Dist, SafeRadius);
		}
	}
}

void AEchidnaSwingZoneActor::BuildRing(int32 SectionIndex, float Inner, float Outer)
{
	if (Outer <= Inner)
	{
		ZoneMesh->ClearMeshSection(SectionIndex);
		return;
	}

	// 섹션마다 살짝 높이를 달리해 겹치는 곳에서 깜빡이지 않게
	const float Z = ZOffset + SectionIndex * 0.5f;
	const int32 Segs = FMath::Max(12, Segments);

	TArray<FVector> Verts;
	TArray<int32> Tris;
	Verts.Reserve((Segs + 1) * 2);
	for (int32 i = 0; i <= Segs; ++i)
	{
		const float A = 2.f * PI * i / Segs;
		const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
		Verts.Add(Dir * Inner + FVector(0.f, 0.f, Z));
		Verts.Add(Dir * Outer + FVector(0.f, 0.f, Z));
	}
	for (int32 i = 0; i < Segs; ++i)
	{
		const int32 I0 = i * 2, O0 = i * 2 + 1, I1 = i * 2 + 2, O1 = i * 2 + 3;
		// 앞/뒤 양쪽 감김 (다른 PMC 액터들과 같은 방식)
		Tris.Append({ I0, O0, O1, I0, O1, I1 });
		Tris.Append({ I0, O1, O0, I0, I1, O1 });
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Verts.Num());

	ZoneMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (SectionMIDs.IsValidIndex(SectionIndex))
	{
		ZoneMesh->SetMaterial(SectionIndex, SectionMIDs[SectionIndex]);
	}
}
