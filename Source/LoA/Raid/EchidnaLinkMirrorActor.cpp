#include "Raid/EchidnaLinkMirrorActor.h"
#include "Raid/EchidnaBoss.h"
#include "Raid/HexArena.h"
#include "Raid/HexTile.h"
#include "LoACharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 헥스 이웃 6방향 (AHexArena와 같은 순서) — 유니티 빌드 이름 충돌을 피하려고 파일 고유 이름
	const int32 LinkMirrorDQ[6] = { 1,  0, -1, -1,  0,  1 };
	const int32 LinkMirrorDR[6] = { 0,  1,  1,  0, -1, -1 };
}

AEchidnaLinkMirrorActor::AEchidnaLinkMirrorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MirrorSurfaceMatFinder(TEXT("/Game/LostArk/Raid/Echidna/Pattern/M_EchidnaMirrorSurface.M_EchidnaMirrorSurface"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMatFinder(TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));

	// 거울 원반 — 기존 거울(AEchidnaMirrorActor)과 같은 모양. 로컬 +X(정면)를 보도록 세움 → 액터가 돌면 같이 돈다
	MirrorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMeshComp"));
	MirrorMeshComp->SetupAttachment(Root);
	MirrorMeshComp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	MirrorMeshComp->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.15f));
	MirrorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MirrorMeshComp->SetCastShadow(false);
	if (CylinderMeshFinder.Succeeded()) MirrorMeshComp->SetStaticMesh(CylinderMeshFinder.Object);
	if (MirrorSurfaceMatFinder.Succeeded()) MirrorMeshComp->SetMaterial(0, MirrorSurfaceMatFinder.Object);

	BeamMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMeshComp"));
	BeamMeshComp->SetupAttachment(Root);
	BeamMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMeshComp->SetCastShadow(false);
	if (PlaneMeshFinder.Succeeded()) BeamMeshComp->SetStaticMesh(PlaneMeshFinder.Object);
	if (GlowMatFinder.Succeeded()) BeamMeshComp->SetMaterial(0, GlowMatFinder.Object);

	// 빛 덩어리는 발사 후 월드에서 따로 움직이므로 절대 위치로 둔다
	OrbMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMeshComp"));
	OrbMeshComp->SetupAttachment(Root);
	OrbMeshComp->SetUsingAbsoluteLocation(true);
	OrbMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMeshComp->SetCastShadow(false);
	OrbMeshComp->SetVisibility(false);
	if (SphereMeshFinder.Succeeded()) OrbMeshComp->SetStaticMesh(SphereMeshFinder.Object);
	if (GlowMatFinder.Succeeded()) OrbMeshComp->SetMaterial(0, GlowMatFinder.Object);
}

void AEchidnaLinkMirrorActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — Tick 기반이라 이 값만으로 추적·발사가 같이 빨라진다
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	MirrorMeshComp->SetRelativeLocation(FVector(0.f, 0.f, MirrorHeight));
	OrbMeshComp->SetWorldScale3D(FVector(OrbRadius / 50.f));	// 엔진 Sphere = 반지름 50

	if (UMaterialInterface* GlowMat = BeamMeshComp->GetMaterial(0))
	{
		UMaterialInstanceDynamic* BeamMID = UMaterialInstanceDynamic::Create(GlowMat, this);
		BeamMID->SetVectorParameterValue(ColorParameterName, BeamColor);
		BeamMeshComp->SetMaterial(0, BeamMID);

		UMaterialInstanceDynamic* OrbMID = UMaterialInstanceDynamic::Create(GlowMat, this);
		OrbMID->SetVectorParameterValue(ColorParameterName, OrbColor);
		OrbMeshComp->SetMaterial(0, OrbMID);
	}
}

void AEchidnaLinkMirrorActor::Activate(ALoACharacter* InPlayer, AHexArena* InArena, AActor* InBoss, const FIntPoint& InBossCoord)
{
	Player = InPlayer;
	Arena = InArena;
	Boss = InBoss;
	BossCoord = InBossCoord;
	Result = EEchidnaLinkResult::Pending;
	Phase = EEchidnaLinkMirrorPhase::Tracking;
	PhaseElapsed = 0.f;

	// 첫 프레임부터 플레이어를 향해 빛줄기 표시
	if (const ALoACharacter* Character = Player.Get())
	{
		const FVector ToPlayer = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!ToPlayer.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f));
		}
	}
	TickTracking(0.f);
}

void AEchidnaLinkMirrorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PhaseElapsed += DeltaTime;

	switch (Phase)
	{
	case EEchidnaLinkMirrorPhase::Tracking: TickTracking(DeltaTime); break;
	case EEchidnaLinkMirrorPhase::Firing:   TickFiring(DeltaTime); break;
	case EEchidnaLinkMirrorPhase::Linking:  TickLinking(DeltaTime); break;
	default: break;
	}
}

void AEchidnaLinkMirrorActor::UpdateBeam(float Length)
{
	Length = FMath::Max(Length, 10.f);
	// 엔진 Plane = 100x100 — 빛줄기는 바닥이 아니라 거울 높이에서 나가도록 OrbHeight에 띄운다
	BeamMeshComp->SetRelativeLocation(FVector(Length * 0.5f, 0.f, OrbHeight));
	BeamMeshComp->SetRelativeScale3D(FVector(Length / 100.f, BeamHalfWidth * 2.f / 100.f, 1.f));
}

void AEchidnaLinkMirrorActor::TickTracking(float DeltaTime)
{
	const ALoACharacter* Character = Player.Get();
	if (Character)
	{
		const FVector ToPlayer = Character->GetActorLocation() - GetActorLocation();
		const FVector Dir2D = ToPlayer.GetSafeNormal2D();
		if (!Dir2D.IsNearlyZero())
		{
			const FRotator Target(0.f, Dir2D.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Target, DeltaTime, TrackingRotationSpeed));
		}
		UpdateBeam(ToPlayer.Size2D());
	}

	if (PhaseElapsed >= TrackDuration)
	{
		BeginFiring();
	}
}

void AEchidnaLinkMirrorActor::BeginFiring()
{
	Phase = EEchidnaLinkMirrorPhase::Firing;
	PhaseElapsed = 0.f;
	OrbTraveled = 0.f;
	OrbHop = 0;
	bHopFails = false;

	// 5초가 되면 플레이어는 강제로 멈춘다 — 푸는 건 패턴 Task의 ExitState
	ALoACharacter* Character = Player.Get();
	if (Character)
	{
		Character->SetHeldByPattern(true);
	}

	BeamMeshComp->SetVisibility(false);
	OrbMeshComp->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, OrbHeight));
	OrbMeshComp->SetVisibility(true);
	OrbDirection = GetActorForwardVector().GetSafeNormal2D();

	AHexArena* HexArena = Arena.Get();
	bPlayerCoordValid = Character && HexArena && HexArena->WorldToTileCoord(Character->GetActorLocation(), PlayerCoord);
	if (!HexArena || !HexArena->WorldToTileCoord(GetActorLocation(), MirrorCoord))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LinkMirror] 거울 타일을 못 찾음 — 실패"));
		Finish(EEchidnaLinkResult::Fail);
		return;
	}

	// 첫 칸 = 거울 타일의 이웃 중 거울 정면(빛줄기 방향)과 가장 잘 맞는 타일
	const FVector MirrorTop = GetActorLocation();
	FIntPoint FirstHop = MirrorCoord;
	FVector FirstHopTop = FVector::ZeroVector;
	float BestDot = -2.f;
	float NeighborDistance = 0.f;
	for (int32 d = 0; d < 6; d++)
	{
		const FIntPoint N(MirrorCoord.X + LinkMirrorDQ[d], MirrorCoord.Y + LinkMirrorDR[d]);
		FVector Top;
		if (!HexArena->GetTileTopLocation(N, Top)) continue;

		const FVector ToN = Top - MirrorTop;
		NeighborDistance = ToN.Size2D();
		const float Dot = FVector::DotProduct(ToN.GetSafeNormal2D(), OrbDirection);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			FirstHop = N;
			FirstHopTop = Top;
		}
	}

	// 정면 쪽(±30도)에 타일이 없으면(외곽 바깥을 향함) 한 칸 거리만큼 허공으로 나갔다가 실패
	if (BestDot < FMath::Cos(FMath::DegreesToRadians(30.f)) || FirstHop == MirrorCoord)
	{
		const float Dist = NeighborDistance > 0.f ? NeighborDistance : 530.f;
		OrbTarget = MirrorTop + OrbDirection * Dist + FVector(0.f, 0.f, OrbHeight);
		bHopFails = true;
		UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 거울 정면에 타일 없음 — 실패 예정"));
		return;
	}

	OrbTarget = FirstHopTop + FVector(0.f, 0.f, OrbHeight);
	OrbDirection = (FirstHopTop - MirrorTop).GetSafeNormal2D();

	// 첫 칸에 플레이어가 서 있어야 한다 — 거울과 2칸 이상 떨어졌거나 정면이 아니면 실패
	if (!bPlayerCoordValid || PlayerCoord != FirstHop)
	{
		bHopFails = true;
		UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 첫 칸 (%d,%d)에 플레이어 없음 (플레이어 %s, 거울과 거리 %d) — 실패 예정"),
			FirstHop.X, FirstHop.Y, bPlayerCoordValid ? *PlayerCoord.ToString() : TEXT("?"),
			bPlayerCoordValid ? AHexArena::GetHexDistance(MirrorCoord, PlayerCoord) : -1);
	}
}

void AEchidnaLinkMirrorActor::TickFiring(float DeltaTime)
{
	// 빛은 웨이포인트(타일 중심) 사이를 직선으로 한 칸씩
	const FVector Current = OrbMeshComp->GetComponentLocation();
	const FVector Next = FMath::VInterpConstantTo(Current, OrbTarget, DeltaTime, OrbSpeed);
	OrbMeshComp->SetWorldLocation(Next);

	if (FVector::Dist(Next, OrbTarget) <= 5.f)
	{
		OnOrbArrived();
	}
}

void AEchidnaLinkMirrorActor::TickLinking(float DeltaTime)
{
	// 두 번째 칸(보스)으로 가는 이동도 같은 방식
	TickFiring(DeltaTime);
}

void AEchidnaLinkMirrorActor::OnOrbArrived()
{
	if (bHopFails)
	{
		UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 빛이 플레이어를 거치지 못함 — 거울잇기 실패"));
		Finish(EEchidnaLinkResult::Fail);
		return;
	}

	AHexArena* HexArena = Arena.Get();
	if (OrbHop == 0)
	{
		// 플레이어 타일 도착 — 노란 테두리, 다음 칸이 보스(다음 거울)여야 한다
		if (HexArena)
		{
			if (AHexTile* Tile = HexArena->GetTile(PlayerCoord))
			{
				Tile->SetLinkHighlighted(true);
			}
		}

		const AActor* BossActor = Boss.Get();
		if (!BossActor || AHexArena::GetHexDistance(PlayerCoord, BossCoord) != 1)
		{
			UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 플레이어 (%d,%d) 옆 칸에 보스 (%d,%d)가 없음 — 실패"),
				PlayerCoord.X, PlayerCoord.Y, BossCoord.X, BossCoord.Y);
			Finish(EEchidnaLinkResult::Fail);
			return;
		}

		OrbHop = 1;
		Phase = EEchidnaLinkMirrorPhase::Linking;
		PhaseElapsed = 0.f;
		OrbTarget = BossActor->GetActorLocation();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 거울 → 플레이어 → 보스로 한 칸씩 이어짐 — 거울잇기 성공"));
	Finish(EEchidnaLinkResult::Success);
}

void AEchidnaLinkMirrorActor::Finish(EEchidnaLinkResult InResult)
{
	Result = InResult;
	Phase = EEchidnaLinkMirrorPhase::Done;
	OrbMeshComp->SetVisibility(false);
	BeamMeshComp->SetVisibility(false);
}
