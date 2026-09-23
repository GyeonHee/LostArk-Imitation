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

	// 5초가 되면 플레이어는 강제로 멈춘다 — 푸는 건 패턴 Task의 ExitState
	if (ALoACharacter* Character = Player.Get())
	{
		Character->SetHeldByPattern(true);
	}

	BeamMeshComp->SetVisibility(false);

	// 거울 정면(현재 빛줄기 방향)으로 발사
	OrbDirection = GetActorForwardVector().GetSafeNormal2D();
	OrbMeshComp->SetWorldLocation(GetActorLocation() + FVector(0.f, 0.f, OrbHeight));
	OrbMeshComp->SetVisibility(true);
}

void AEchidnaLinkMirrorActor::TickFiring(float DeltaTime)
{
	const float Step = OrbSpeed * DeltaTime;
	const FVector NewLocation = OrbMeshComp->GetComponentLocation() + OrbDirection * Step;
	OrbMeshComp->SetWorldLocation(NewLocation);
	OrbTraveled += Step;

	ALoACharacter* Character = Player.Get();
	if (Character)
	{
		const float CapsuleRadius = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
		if (FVector::Dist2D(NewLocation, Character->GetActorLocation()) <= OrbHitRadius + CapsuleRadius)
		{
			// 플레이어에 닿음 — 플레이어 타일에 노란 테두리, 그 타일 옆에 보스가 있으면 보스에게로
			AHexArena* HexArena = Arena.Get();
			FIntPoint PlayerCoord;
			if (HexArena && HexArena->WorldToTileCoord(Character->GetActorLocation(), PlayerCoord))
			{
				if (AHexTile* Tile = HexArena->GetTile(PlayerCoord))
				{
					Tile->SetLinkHighlighted(true);
				}

				if (AHexArena::GetHexDistance(PlayerCoord, BossCoord) == 1 && Boss.IsValid())
				{
					Phase = EEchidnaLinkMirrorPhase::Linking;
					PhaseElapsed = 0.f;
					OrbMeshComp->SetWorldLocation(Character->GetActorLocation() + FVector(0.f, 0.f, OrbHeight * 0.3f));
					return;
				}
			}

			UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 플레이어는 맞혔지만 옆 칸에 보스가 없음 — 실패"));
			Finish(EEchidnaLinkResult::Fail);
			return;
		}
	}

	if (OrbTraveled >= MaxTravelDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 빛 덩어리가 플레이어에 닿지 못하고 맵 밖으로 — 실패"));
		Finish(EEchidnaLinkResult::Fail);
	}
}

void AEchidnaLinkMirrorActor::TickLinking(float DeltaTime)
{
	const AActor* BossActor = Boss.Get();
	if (!BossActor)
	{
		Finish(EEchidnaLinkResult::Fail);
		return;
	}

	const FVector Target = BossActor->GetActorLocation();
	const FVector Current = OrbMeshComp->GetComponentLocation();
	const FVector Next = FMath::VInterpConstantTo(Current, Target, DeltaTime, OrbSpeed);
	OrbMeshComp->SetWorldLocation(Next);

	if (FVector::Dist(Next, Target) <= 10.f)
	{
		UE_LOG(LogTemp, Log, TEXT("[LinkMirror] 빛 덩어리가 보스에게 도달 — 거울잇기 성공"));
		Finish(EEchidnaLinkResult::Success);
	}
}

void AEchidnaLinkMirrorActor::Finish(EEchidnaLinkResult InResult)
{
	Result = InResult;
	Phase = EEchidnaLinkMirrorPhase::Done;
	OrbMeshComp->SetVisibility(false);
	BeamMeshComp->SetVisibility(false);
}
