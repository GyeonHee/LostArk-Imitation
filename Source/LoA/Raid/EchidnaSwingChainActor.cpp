#include "Raid/EchidnaSwingChainActor.h"
#include "Raid/EchidnaBoss.h"
#include "Raid/HexArena.h"
#include "Raid/HexTile.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 엔진 기본 Cylinder는 높이 100cm(Z축), 지름 100cm
	constexpr float CylinderHeight = 100.f;
	constexpr float CylinderDiameter = 100.f;
}

AEchidnaSwingChainActor::AEchidnaSwingChainActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ChainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChainMesh"));
	ChainMesh->SetupAttachment(Root);
	ChainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChainMesh->SetCastShadow(false);
	ChainMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		ChainMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		ChainMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaSwingChainActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — ChainDuration 판정이 Tick 누적이라 이 값으로 자동 가속
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	if (UMaterialInterface* BaseMat = ChainMesh->GetMaterial(0))
	{
		ChainMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		ChainMID->SetVectorParameterValue(ColorParameterName, StartColor);
		ChainMesh->SetMaterial(0, ChainMID);
	}
}

void AEchidnaSwingChainActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 중간에 파괴돼도(패턴이 끊김) 노란 테두리가 남지 않게
	SetBreakTileHighlighted(false);
	Super::EndPlay(EndPlayReason);
}

void AEchidnaSwingChainActor::Activate(AActor* InBoss, ALoACharacter* InPlayer, AHexArena* InArena, const FIntPoint& InBreakCoord)
{
	Boss = InBoss;
	Player = InPlayer;
	Arena = InArena;
	BreakCoord = InBreakCoord;

	Result = EEchidnaSwingChainResult::Pending;
	Elapsed = 0.f;
	bActive = true;

	SetBreakTileHighlighted(true);
	ChainMesh->SetVisibility(true);
	UpdateChainTransform();

	UE_LOG(LogLoA, Log, TEXT("[SwingChain] 사슬 연결 — 끊는 타일 (%d,%d), 제한 %.1f초"), BreakCoord.X, BreakCoord.Y, ChainDuration);
}

void AEchidnaSwingChainActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActive) return;

	ALoACharacter* Target = Player.Get();
	if (!Target || !Boss.IsValid())
	{
		Resolve(EEchidnaSwingChainResult::Broken);
		return;
	}

	Elapsed += DeltaTime;
	UpdateChainTransform();

	// 남은 시간이 줄수록 빨개짐
	if (ChainMID)
	{
		const float Alpha = ChainDuration > 0.f ? FMath::Clamp(Elapsed / ChainDuration, 0.f, 1.f) : 1.f;
		ChainMID->SetVectorParameterValue(ColorParameterName, FMath::Lerp(StartColor, EndColor, Alpha));
	}

	// 보스 반대편 타일 위면 끊김
	FIntPoint Coord;
	if (AHexArena* A = Arena.Get(); A && A->WorldToTileCoord(Target->GetActorLocation(), Coord) && Coord == BreakCoord)
	{
		Resolve(EEchidnaSwingChainResult::Broken);
		return;
	}

	if (Elapsed >= ChainDuration)
	{
		// 못 끊음 — 스택이 몇이든 한 번에 최대까지 채워 매혹 3스택과 같은 매혹 상태로
		Target->AddCharmGauge(Target->MaxCharmGauge);
		Resolve(EEchidnaSwingChainResult::Failed);
	}
}

void AEchidnaSwingChainActor::UpdateChainTransform()
{
	const AActor* B = Boss.Get();
	const ALoACharacter* P = Player.Get();
	if (!B || !P) return;

	const FVector Start = B->GetActorLocation() + FVector(0.f, 0.f, EndpointZOffset);
	const FVector End = P->GetActorLocation() + FVector(0.f, 0.f, EndpointZOffset);
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length < KINDA_SMALL_NUMBER) return;

	// Cylinder는 로컬 Z축이 길이 방향 — Z를 두 끝점 방향으로 돌리고 Z 스케일로 길이를 맞춘다 (피벗이 중심이라 중점에 둠)
	const FRotator Rot = FRotationMatrix::MakeFromZ(Delta / Length).Rotator();
	const float Thick = ChainThickness / CylinderDiameter;
	SetActorLocation((Start + End) * 0.5f);
	ChainMesh->SetWorldRotation(Rot);
	ChainMesh->SetWorldScale3D(FVector(Thick, Thick, Length / CylinderHeight));
}

void AEchidnaSwingChainActor::Resolve(EEchidnaSwingChainResult NewResult)
{
	if (!bActive) return;

	bActive = false;
	Result = NewResult;
	ChainMesh->SetVisibility(false);
	SetBreakTileHighlighted(false);

	UE_LOG(LogLoA, Log, TEXT("[SwingChain] %s (%.1f초)"),
		NewResult == EEchidnaSwingChainResult::Broken ? TEXT("사슬 끊음 — 파훼") : TEXT("사슬 못 끊음 — 매혹"), Elapsed);
}

void AEchidnaSwingChainActor::SetBreakTileHighlighted(bool bHighlighted)
{
	if (AHexArena* A = Arena.Get())
	{
		if (AHexTile* Tile = A->GetTile(BreakCoord))
		{
			Tile->SetLinkHighlighted(bHighlighted);
		}
	}
}
