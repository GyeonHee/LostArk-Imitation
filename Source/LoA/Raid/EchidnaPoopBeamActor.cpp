#include "Raid/EchidnaPoopBeamActor.h"
#include "Raid/EchidnaBoss.h"
#include "LoACharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

namespace
{
	// 앞/뒤 양쪽 감김 (다른 PMC 액터들과 같은 방식)
	void AddBeamQuad(TArray<FVector>& Verts, TArray<int32>& Tris,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C, D });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Tris.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}

	float GetCapsuleRadius(const ACharacter* Character)
	{
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
	}
}

AEchidnaPoopBeamActor::AEchidnaPoopBeamActor()
{
	PrimaryActorTick.bCanEverTick = true;

	BeamMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BeamMesh"));
	SetRootComponent(BeamMesh);
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		BeamMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaPoopBeamActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — Tick 기반 진행은 이 값으로 자동 가속 (월드 타이머는 안 씀)
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	if (UMaterialInterface* BaseMat = BeamMesh->GetMaterial(0))
	{
		const FLinearColor Colors[3] = { BackgroundColor, FillColor, ExplodeColor };
		for (const FLinearColor& Color : Colors)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(ColorParameterName, Color);
			SectionMIDs.Add(MID);
		}
	}
}

void AEchidnaPoopBeamActor::Activate(ACharacter* InTarget, float InBeamDamage, float InCircleDamage, AController* InInstigator,
	float LengthOverride, float HalfWidthOverride, float CircleRadiusOverride)
{
	Target = InTarget;
	BeamDamage = InBeamDamage;
	CircleDamage = InCircleDamage;
	InstigatorController = InInstigator;

	if (LengthOverride >= 0.f) BeamLength = LengthOverride;
	if (HalfWidthOverride >= 0.f) BeamHalfWidth = HalfWidthOverride;
	if (CircleRadiusOverride >= 0.f) CircleRadius = CircleRadiusOverride;

	AlreadyHit.Reset();
	ExplodedSegments = 0;
	PhaseElapsed = 0.f;
	Phase = EEchidnaPoopBeamPhase::Tracking;

	BuildSection(SectionBackground, 0.f, BeamLength, CircleRadius);
	BuildSection(SectionFill, 0.f, 0.f, 0.f);
}

void AEchidnaPoopBeamActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (Phase)
	{
	case EEchidnaPoopBeamPhase::Tracking:  TickTracking(DeltaTime); break;
	case EEchidnaPoopBeamPhase::Exploding: TickExploding(DeltaTime); break;
	default: break;
	}
}

void AEchidnaPoopBeamActor::TickTracking(float DeltaTime)
{
	PhaseElapsed += DeltaTime;

	if (const ACharacter* Character = Target.Get())
	{
		const FVector ToTarget = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Desired(0.f, ToTarget.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Desired, DeltaTime, TrackingRotationSpeed));
		}
	}

	// 보스 쪽부터 게이지처럼 차오름 — 직사각형은 길이, 원은 반지름이 같은 비율로
	const float Progress = TrackDuration > 0.f ? FMath::Clamp(PhaseElapsed / TrackDuration, 0.f, 1.f) : 1.f;
	BuildSection(SectionFill, 0.f, BeamLength * Progress, CircleRadius * Progress);

	if (Progress >= 1.f)
	{
		Phase = EEchidnaPoopBeamPhase::Exploding;
		PhaseElapsed = 0.f;
		BuildSection(SectionFill, 0.f, 0.f, 0.f);

		ExplodeCircle();
		ExplodeSegment(0);
		ExplodedSegments = 1;
	}
}

void AEchidnaPoopBeamActor::TickExploding(float DeltaTime)
{
	PhaseElapsed += DeltaTime;

	const int32 Count = FMath::Max(1, ExplosionSegmentCount);
	while (ExplodedSegments < Count && PhaseElapsed >= ExplosionSegmentInterval * ExplodedSegments)
	{
		ExplodeSegment(ExplodedSegments);
		++ExplodedSegments;
	}

	if (ExplodedSegments >= Count)
	{
		Phase = EEchidnaPoopBeamPhase::Done;
		BuildSection(SectionBackground, 0.f, 0.f, 0.f);
		SetLifeSpan(FMath::Max(0.05f, LingerAfterExplosion));
	}
}

void AEchidnaPoopBeamActor::ExplodeCircle()
{
	if (CircleRadius <= 0.f) return;

	const FVector Center = GetActorLocation();
	for (TActorIterator<ALoACharacter> It(GetWorld()); It; ++It)
	{
		const float Dist = FVector::Dist2D(It->GetActorLocation(), Center);
		if (Dist <= CircleRadius + GetCapsuleRadius(*It))
		{
			TryHit(*It);
		}
	}
}

void AEchidnaPoopBeamActor::ExplodeSegment(int32 SegmentIndex)
{
	const int32 Count = FMath::Max(1, ExplosionSegmentCount);
	const float SegLen = BeamLength / Count;
	const float Start = SegLen * SegmentIndex;
	const float End = SegLen * (SegmentIndex + 1);

	// 터진 칸까지 누적해서 밝게 — 원은 첫 칸과 함께 이미 터진 상태로 유지
	BuildSection(SectionExplode, 0.f, End, CircleRadius);

	const FTransform& Xform = GetActorTransform();
	for (TActorIterator<ALoACharacter> It(GetWorld()); It; ++It)
	{
		const FVector Local = Xform.InverseTransformPosition(It->GetActorLocation());
		const float Margin = GetCapsuleRadius(*It);
		if (Local.X >= Start - Margin && Local.X <= End + Margin && FMath::Abs(Local.Y) <= BeamHalfWidth + Margin)
		{
			TryHit(*It);
		}
	}
}

void AEchidnaPoopBeamActor::TryHit(ACharacter* Character)
{
	if (!Character || AlreadyHit.Contains(Character)) return;
	AlreadyHit.Add(Character);

	const bool bInCircle = FVector::Dist2D(Character->GetActorLocation(), GetActorLocation()) <= CircleRadius + GetCapsuleRadius(Character);
	const float Damage = bInCircle ? CircleDamage : BeamDamage;

	UGameplayStatics::ApplyDamage(Character, Damage, InstigatorController.Get(), this, UDamageType::StaticClass());

	if (bApplyKnockdownOnHit)
	{
		if (ALoACharacter* LoACharacter = Cast<ALoACharacter>(Character))
		{
			LoACharacter->ApplyKnockdown(GetActorLocation());
		}
	}
}

void AEchidnaPoopBeamActor::BuildSection(int32 SectionIndex, float RectStart, float RectEnd, float Radius)
{
	const bool bHasRect = RectEnd > RectStart;
	const bool bHasCircle = Radius > 0.f;
	if (!bHasRect && !bHasCircle)
	{
		BeamMesh->ClearMeshSection(SectionIndex);
		return;
	}

	// 섹션마다 살짝 높이를 달리해 겹치는 곳에서 깜빡이지 않게
	const float Z = ZOffset + SectionIndex * 0.5f;

	TArray<FVector> Verts;
	TArray<int32> Tris;

	if (bHasRect)
	{
		AddBeamQuad(Verts, Tris,
			FVector(RectStart, -BeamHalfWidth, Z), FVector(RectEnd, -BeamHalfWidth, Z),
			FVector(RectEnd, BeamHalfWidth, Z), FVector(RectStart, BeamHalfWidth, Z));
	}

	if (bHasCircle)
	{
		const int32 Segs = FMath::Max(8, CircleSegments);
		const int32 CenterIndex = Verts.Add(FVector(0.f, 0.f, Z));
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = 2.f * PI * i / Segs;
			Verts.Add(FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, Z));
		}
		for (int32 i = 0; i < Segs; ++i)
		{
			const int32 V0 = CenterIndex + 1 + i;
			Tris.Append({ CenterIndex, V0, V0 + 1, CenterIndex, V0 + 1, V0 });
		}
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Verts.Num());

	BeamMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (SectionMIDs.IsValidIndex(SectionIndex))
	{
		BeamMesh->SetMaterial(SectionIndex, SectionMIDs[SectionIndex]);
	}
}
