#include "Raid/EchidnaTetherActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "LoACharacter.h"
#include "LoA.h"

AEchidnaTetherActor::AEchidnaTetherActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	// BasicShapeMaterial(Opaque)은 Alpha를 무시해서 반투명 예고/불투명 실행 구분이 안 먹힘 — Translucent+Unlit인 M_MirrorLaser 기본 사용
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));

	ZoneMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMeshComp"));
	ZoneMeshComp->SetupAttachment(Root);
	ZoneMeshComp->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	ZoneMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMeshComp->SetCastShadow(false);
	if (PlaneMeshFinder.Succeeded()) ZoneMeshComp->SetStaticMesh(PlaneMeshFinder.Object);
	if (DefaultMatFinder.Succeeded()) ZoneMeshComp->SetMaterial(0, DefaultMatFinder.Object);
}

void AEchidnaTetherActor::BeginPlay()
{
	Super::BeginPlay();
}

void AEchidnaTetherActor::Activate(const FVector& InPullTarget, float InPullStrength, AController* InInstigator)
{
	PullTarget = InPullTarget;
	PullStrength = InPullStrength;
	InstigatorController = InInstigator;
	bSnapped = false;

	// 엔진 기본 Plane 메시는 100x100(cm) 기준 — 로컬 +X로 TetherRange만큼, 폭은 TetherHalfWidth*2만큼 스케일
	if (ZoneMeshComp)
	{
		ZoneMeshComp->SetRelativeLocation(FVector(TetherRange * 0.5f, 0.f, 5.f));
		ZoneMeshComp->SetRelativeScale3D(FVector(TetherRange / 100.f, (TetherHalfWidth * 2.f) / 100.f, 1.f));
	}
	ApplyMeshColor(TetherColor, TetherOpacity);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaTether] Activate — Loc=%s Dir=%s Range=%.0f"),
		*GetActorLocation().ToString(), *GetActorForwardVector().ToString(), TetherRange);

	if (SnapDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(SnapTimerHandle, this, &AEchidnaTetherActor::PerformSnap, SnapDelay, false);
	}
	else
	{
		PerformSnap();
	}
}

void AEchidnaTetherActor::PerformSnap()
{
	bSnapped = true;

	// 판정이 실제로 발동하는 순간 — 예상 범위(반투명)에서 실제 실행 범위(완전 불투명)로 전환
	ApplyMeshColor(SnapColor, SnapOpacity);

	UWorld* World = GetWorld();
	if (World)
	{
		APawn* InstigatorPawn = InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		if (InstigatorPawn) QueryParams.AddIgnoredActor(InstigatorPawn);

		const FVector Origin = GetActorLocation();
		const FVector Direction = GetActorForwardVector();
		const FVector BoxCenter = Origin + Direction * (TetherRange * 0.5f);
		const FCollisionShape Box = FCollisionShape::MakeBox(
			FVector(TetherRange * 0.5f, TetherHalfWidth, TetherHalfHeight));

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps, BoxCenter, GetActorRotation().Quaternion(),
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
			Box, QueryParams);

		TSet<AActor*> Unique;
		for (const FOverlapResult& Hit : Overlaps)
		{
			if (AActor* HitActor = Hit.GetActor())
			{
				Unique.Add(HitActor);
			}
		}

		for (AActor* HitActor : Unique)
		{
			if (ALoACharacter* HitCharacter = Cast<ALoACharacter>(HitActor))
			{
				HitCharacter->ApplyPull(PullTarget, PullStrength);
				bDidHit = true;
			}
		}

		UE_LOG(LogLoA, Log, TEXT("[EchidnaTether] PerformSnap — %d명 당김 판정"), Unique.Num());
	}

	// 불투명하게 바뀐 실행 범위를 LifeAfterSnap 동안 그대로 보여준 뒤 소멸
	SetLifeSpan(LifeAfterSnap);
}

void AEchidnaTetherActor::ApplyMeshColor(const FLinearColor& Color, float Opacity)
{
	if (!ZoneMeshComp) return;

	UMaterialInterface* Source = ZoneMeshComp->GetMaterial(0);
	if (!Source) return;

	UMaterialInstanceDynamic* MID = ZoneMeshComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Source);
	if (MID)
	{
		MID->SetVectorParameterValue(ColorParameterName, FLinearColor(Color.R, Color.G, Color.B, Opacity));
	}
}
