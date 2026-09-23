#include "Raid/EchidnaFlytrapZoneActor.h"
#include "Raid/EchidnaBoss.h"
#include "Raid/HexArena.h"
#include "LoACharacter.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

namespace
{
	enum : int32 { MID_ZoneBackground = 0, MID_ZoneFill, MID_Lobe, MID_Tooth, MID_Count };

	// 앞/뒤 양쪽 감김 (다른 PMC 액터들과 같은 방식)
	void AddTrapTri(TArray<FVector>& Verts, TArray<int32>& Tris, const FVector& A, const FVector& B, const FVector& C)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 1 });
	}

	void AddTrapQuad(TArray<FVector>& Verts, TArray<int32>& Tris, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		AddTrapTri(Verts, Tris, A, B, C);
		AddTrapTri(Verts, Tris, A, C, D);
	}

	void CommitSection(UProceduralMeshComponent* Mesh, int32 Section, const TArray<FVector>& Verts, const TArray<int32>& Tris)
	{
		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Verts.Num());
		TArray<FVector2D> UVs;
		UVs.Init(FVector2D::ZeroVector, Verts.Num());
		Mesh->CreateMeshSection(Section, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	}

	UProceduralMeshComponent* MakePMC(AActor* Owner, const TCHAR* Name)
	{
		UProceduralMeshComponent* PMC = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		PMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PMC->SetCastShadow(false);
		return PMC;
	}
}

AEchidnaFlytrapZoneActor::AEchidnaFlytrapZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	ZoneMesh = MakePMC(this, TEXT("ZoneMesh"));
	SetRootComponent(ZoneMesh);

	TrapRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TrapRoot"));
	TrapRoot->SetupAttachment(ZoneMesh);

	HingeA = CreateDefaultSubobject<USceneComponent>(TEXT("HingeA"));
	HingeA->SetupAttachment(TrapRoot);
	HingeB = CreateDefaultSubobject<USceneComponent>(TEXT("HingeB"));
	HingeB->SetupAttachment(TrapRoot);

	LobeA = MakePMC(this, TEXT("LobeA"));
	LobeA->SetupAttachment(HingeA);
	LobeB = MakePMC(this, TEXT("LobeB"));
	LobeB->SetupAttachment(HingeB);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		ZoneMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaFlytrapZoneActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — Tick 기반이라 이 값만으로 연출·판정 타이밍이 같이 빨라진다
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	if (UMaterialInterface* BaseMat = ZoneMesh->GetMaterial(0))
	{
		const FLinearColor Colors[MID_Count] = { ZoneBackgroundColor, ZoneFillColor, LobeColor, ToothColor };
		for (const FLinearColor& Color : Colors)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(ColorParameterName, Color);
			MIDs.Add(MID);
		}
	}

	TrapRoot->SetVisibility(false, true);
}

void AEchidnaFlytrapZoneActor::Activate(AHexArena* InArena, const FIntPoint& InCoord, float InTileInRadius,
	float InEatDamageRatio, AController* InInstigator)
{
	Arena = InArena;
	Coord = InCoord;
	TileInRadius = InTileInRadius > 0.f ? InTileInRadius : TileInRadius;
	EatDamageRatio = InEatDamageRatio;
	InstigatorController = InInstigator;
	bBitten = false;

	BuildHex(0, 0.95f);
	BuildLobe(LobeA);
	BuildLobe(LobeB);
	SetLobeAngle(OpenAngle);

	EnterPhase(EEchidnaFlytrapPhase::Filling);
}

void AEchidnaFlytrapZoneActor::EnterPhase(EEchidnaFlytrapPhase NewPhase)
{
	Phase = NewPhase;
	PhaseElapsed = 0.f;

	switch (Phase)
	{
	case EEchidnaFlytrapPhase::Emerging:
		// 장판이 꽉 찬 순간 = 판정 순간. 이후 연출(솟아오름·닫힘)은 판정과 무관
		ZoneMesh->ClearMeshSection(0);
		ZoneMesh->ClearMeshSection(1);
		TrapRoot->SetVisibility(true, true);
		Bite();
		break;
	case EEchidnaFlytrapPhase::Done:
		Destroy();
		break;
	default:
		break;
	}
}

void AEchidnaFlytrapZoneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PhaseElapsed += DeltaTime;

	const float TrapDepth = TileInRadius * LobeRadiusRatio;

	// 꽃이 나와 있는 동안(솟는 중~유지) 타일을 밟으면 먹힌다
	if (Phase == EEchidnaFlytrapPhase::Emerging || Phase == EEchidnaFlytrapPhase::Snapping || Phase == EEchidnaFlytrapPhase::Lingering)
	{
		EatCharactersOnTile();
	}

	switch (Phase)
	{
	case EEchidnaFlytrapPhase::Filling:
	{
		const float Progress = FillDuration > 0.f ? FMath::Clamp(PhaseElapsed / FillDuration, 0.f, 1.f) : 1.f;
		BuildHex(1, 0.95f * Progress);
		if (Progress >= 1.f) EnterPhase(EEchidnaFlytrapPhase::Emerging);
		break;
	}
	case EEchidnaFlytrapPhase::Emerging:
	{
		// 땅속에서 튀어나오며 커진다 (입은 벌린 채)
		const float T = EmergeDuration > 0.f ? FMath::Clamp(PhaseElapsed / EmergeDuration, 0.f, 1.f) : 1.f;
		const float Ease = 1.f - FMath::Square(1.f - T);
		TrapRoot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(-TrapDepth, 0.f, Ease)));
		TrapRoot->SetRelativeScale3D(FVector(FMath::Lerp(0.3f, 1.f, Ease)));
		if (T >= 1.f) EnterPhase(EEchidnaFlytrapPhase::Snapping);
		break;
	}
	case EEchidnaFlytrapPhase::Snapping:
	{
		const float T = SnapDuration > 0.f ? FMath::Clamp(PhaseElapsed / SnapDuration, 0.f, 1.f) : 1.f;
		SetLobeAngle(FMath::Lerp(OpenAngle, ClosedAngle, T * T));
		if (T >= 1.f) EnterPhase(EEchidnaFlytrapPhase::Lingering);
		break;
	}
	case EEchidnaFlytrapPhase::Lingering:
		// Dismiss()가 불릴 때까지 그대로 — 밟으면 먹힘 판정은 switch 위 공통 처리
		break;
	case EEchidnaFlytrapPhase::Sinking:
	{
		const float T = SinkDuration > 0.f ? FMath::Clamp(PhaseElapsed / SinkDuration, 0.f, 1.f) : 1.f;
		TrapRoot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(0.f, -TrapDepth, T)));
		TrapRoot->SetRelativeScale3D(FVector(FMath::Lerp(1.f, 0.3f, T)));
		if (T >= 1.f) EnterPhase(EEchidnaFlytrapPhase::Done);
		break;
	}
	default:
		break;
	}
}

void AEchidnaFlytrapZoneActor::Bite()
{
	bBitten = true;
	EatCharactersOnTile();
}

void AEchidnaFlytrapZoneActor::EatCharactersOnTile()
{
	AHexArena* HexArena = Arena.Get();
	if (!HexArena) return;

	// 이 타일 위에 서 있는 플레이어만 — 이미 붙잡힌 사람은 다시 먹지 않는다(데미지 중복 방지)
	for (TActorIterator<ALoACharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsHeldByPattern()) continue;

		FIntPoint CharCoord;
		if (!HexArena->WorldToTileCoord(It->GetActorLocation(), CharCoord) || CharCoord != Coord) continue;

		UGameplayStatics::ApplyDamage(*It, It->GetMaxHP() * EatDamageRatio, InstigatorController.Get(), this, UDamageType::StaticClass());
		It->SetHeldByPattern(true);
		UE_LOG(LogTemp, Log, TEXT("[Flytrap] (%d,%d) %s 잡아먹힘"), Coord.X, Coord.Y, *It->GetName());
	}
}

void AEchidnaFlytrapZoneActor::Dismiss()
{
	if (Phase == EEchidnaFlytrapPhase::Filling || Phase == EEchidnaFlytrapPhase::Inactive)
	{
		EnterPhase(EEchidnaFlytrapPhase::Done);
	}
	else if (Phase != EEchidnaFlytrapPhase::Sinking && Phase != EEchidnaFlytrapPhase::Done)
	{
		EnterPhase(EEchidnaFlytrapPhase::Sinking);
	}
}

void AEchidnaFlytrapZoneActor::BuildHex(int32 Section, float Scale)
{
	if (Scale <= 0.f)
	{
		ZoneMesh->ClearMeshSection(Section);
		return;
	}

	// 타일과 같은 회전으로 스폰되므로 로컬 꼭짓점은 0/60/120...도 (AHexTile 테두리와 같은 규칙)
	const float CornerRadius = TileInRadius * 2.f / FMath::Sqrt(3.f) * Scale;
	const float Z = ZoneZOffset + Section * 0.5f;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	for (int32 i = 0; i < 6; ++i)
	{
		const float A0 = FMath::DegreesToRadians(60.f * i);
		const float A1 = FMath::DegreesToRadians(60.f * (i + 1));
		AddTrapTri(Verts, Tris, FVector(0.f, 0.f, Z),
			FVector(FMath::Cos(A0) * CornerRadius, FMath::Sin(A0) * CornerRadius, Z),
			FVector(FMath::Cos(A1) * CornerRadius, FMath::Sin(A1) * CornerRadius, Z));
	}

	CommitSection(ZoneMesh, Section, Verts, Tris);
	const int32 MIDIndex = Section == 0 ? MID_ZoneBackground : MID_ZoneFill;
	if (MIDs.IsValidIndex(MIDIndex))
	{
		ZoneMesh->SetMaterial(Section, MIDs[MIDIndex]);
	}
}

void AEchidnaFlytrapZoneActor::BuildLobe(UProceduralMeshComponent* Lobe)
{
	// 경첩(로컬 Y축)에서 +X로 뻗은 반원 잎. 안쪽 면이 +Z라 닫히면 두 잎의 안쪽이 마주보고,
	// 바깥쪽(-Z)으로 볼록하게 부풀려 닫힌 모양이 꼬투리처럼 보이게 한다
	const float R = TileInRadius * LobeRadiusRatio;
	const float Depth = R * LobeDepthRatio;
	constexpr int32 NX = 8;
	constexpr int32 NY = 8;

	auto LobePoint = [&](int32 Xi, int32 Yi)
	{
		const float T = static_cast<float>(Xi) / NX;
		const float V = static_cast<float>(Yi) / NY * 2.f - 1.f;
		const float X = R * T;
		const float HalfSpan = R * FMath::Sqrt(FMath::Max(0.f, 1.f - T * T));
		const float Bulge = Depth * FMath::Sin(PI * T) * (1.f - V * V);
		return FVector(X, V * HalfSpan, -Bulge);
	};

	TArray<FVector> Verts;
	TArray<int32> Tris;
	for (int32 Xi = 0; Xi < NX; ++Xi)
	{
		for (int32 Yi = 0; Yi < NY; ++Yi)
		{
			AddTrapQuad(Verts, Tris, LobePoint(Xi, Yi), LobePoint(Xi + 1, Yi), LobePoint(Xi + 1, Yi + 1), LobePoint(Xi, Yi + 1));
		}
	}
	CommitSection(Lobe, 0, Verts, Tris);

	// 가시 — 반원 가장자리를 따라 바깥으로 뻗고 안쪽(+Z)으로 살짝 휨
	TArray<FVector> TVerts;
	TArray<int32> TTris;
	const int32 Teeth = FMath::Max(3, ToothCount);
	const float ToothLen = R * 0.35f;
	const float HalfStep = PI / Teeth * 0.35f;
	for (int32 i = 0; i < Teeth; ++i)
	{
		const float Phi = -PI * 0.5f + PI * (i + 0.5f) / Teeth;	// -90 ~ 90도 사이
		const FVector Dir(FMath::Cos(Phi), FMath::Sin(Phi), 0.f);
		const FVector BaseL(FMath::Cos(Phi - HalfStep) * R, FMath::Sin(Phi - HalfStep) * R, 0.f);
		const FVector BaseR(FMath::Cos(Phi + HalfStep) * R, FMath::Sin(Phi + HalfStep) * R, 0.f);
		const FVector Tip = Dir * (R + ToothLen) + FVector(0.f, 0.f, ToothLen * 0.4f);
		AddTrapTri(TVerts, TTris, BaseL, BaseR, Tip);
	}
	CommitSection(Lobe, 1, TVerts, TTris);

	if (MIDs.IsValidIndex(MID_Tooth))
	{
		Lobe->SetMaterial(0, MIDs[MID_Lobe]);
		Lobe->SetMaterial(1, MIDs[MID_Tooth]);
	}
}

void AEchidnaFlytrapZoneActor::SetLobeAngle(float AngleDeg)
{
	// 양쪽 잎이 경첩에서 들어 올려진다 — B는 반대편을 보도록 Yaw 180
	HingeA->SetRelativeRotation(FRotator(AngleDeg, 0.f, 0.f));
	HingeB->SetRelativeRotation(FRotator(AngleDeg, 180.f, 0.f));
}
