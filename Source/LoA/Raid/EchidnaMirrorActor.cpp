#include "Raid/EchidnaMirrorActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "LoACharacter.h"
#include "LoA.h"

AEchidnaMirrorActor::AEchidnaMirrorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	MirrorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMeshComp"));
	MirrorMeshComp->SetupAttachment(Root);
	MirrorMeshComp->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	MirrorMeshComp->SetRelativeScale3D(FVector(1.0f));
	MirrorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MirrorMeshComp->SetCastShadow(false);
	if (SphereMeshFinder.Succeeded()) MirrorMeshComp->SetStaticMesh(SphereMeshFinder.Object);
	if (DefaultMatFinder.Succeeded()) MirrorMeshComp->SetMaterial(0, DefaultMatFinder.Object);

	ZoneMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMeshComp"));
	ZoneMeshComp->SetupAttachment(Root);
	ZoneMeshComp->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	ZoneMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMeshComp->SetCastShadow(false);
	if (PlaneMeshFinder.Succeeded()) ZoneMeshComp->SetStaticMesh(PlaneMeshFinder.Object);
	if (DefaultMatFinder.Succeeded()) ZoneMeshComp->SetMaterial(0, DefaultMatFinder.Object);
}

void AEchidnaMirrorActor::BeginPlay()
{
	Super::BeginPlay();
}

void AEchidnaMirrorActor::Activate(float InTickDamage, AController* InInstigator)
{
	TickDamage = InTickDamage;
	InstigatorController = InInstigator;
	Phase = EEchidnaMirrorPhase::Tracking;
	ElapsedTracking = 0.f;
	CurrentDamageTick = 0;

	ApplyPhaseVisuals(TrackingZoneMaterial, TrackingColor);

	// 스폰 직후 첫 프레임부터 플레이어 방향으로 즉시 정렬 + 장판 표시
	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		FVector ToPlayer = PlayerChar->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.f;
		const float Distance = FMath::Min(ToPlayer.Size(), MaxRange);
		if (!ToPlayer.IsNearlyZero())
		{
			SetActorRotation(ToPlayer.GetSafeNormal().Rotation());
		}
		UpdateZoneTransform(Distance);
	}
	else
	{
		UpdateZoneTransform(MaxRange * 0.5f);
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirror] Activate — Loc=%s TrackingDuration=%.1f FiringDuration=%.1f"),
		*GetActorLocation().ToString(), TrackingDuration, FiringDuration);
}

void AEchidnaMirrorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Phase != EEchidnaMirrorPhase::Tracking)
	{
		return;
	}

	if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		FVector ToPlayer = PlayerChar->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.f;
		const float Distance = FMath::Min(ToPlayer.Size(), MaxRange);
		if (!ToPlayer.IsNearlyZero())
		{
			// 즉시 스냅하지 않고 일정 각속도로 따라가게 — 대시 같은 순간이동에도 즉시 안 꺾임
			const FRotator TargetRotation = ToPlayer.GetSafeNormal().Rotation();
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), TargetRotation, DeltaTime, TrackingRotationSpeed));
		}
		UpdateZoneTransform(Distance);
	}

	ElapsedTracking += DeltaTime;
	if (ElapsedTracking >= TrackingDuration)
	{
		BeginFiring();
	}
}

void AEchidnaMirrorActor::UpdateZoneTransform(float CurrentDistance)
{
	if (!ZoneMeshComp) return;

	const float Length = FMath::Max(CurrentDistance, 10.f);
	// 엔진 기본 Plane 메시는 100x100(cm) 기준이므로 100으로 나눠 스케일 계산
	ZoneMeshComp->SetRelativeLocation(FVector(Length * 0.5f, 0.f, 5.f));
	ZoneMeshComp->SetRelativeScale3D(FVector(Length / 100.f, (BeamHalfWidth * 2.f) / 100.f, 1.f));
}

void AEchidnaMirrorActor::BeginFiring()
{
	Phase = EEchidnaMirrorPhase::Firing;

	// 방향 고정, 장판 길이를 MaxRange로 고정
	UpdateZoneTransform(MaxRange);

	ApplyPhaseVisuals(FiringZoneMaterial, FiringColor);

	if (BeamStartVFXSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), BeamStartVFXSystem, GetActorLocation(), GetActorRotation());
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirror] BeginFiring — Loc=%s Dir=%s"),
		*GetActorLocation().ToString(), *GetActorForwardVector().ToString());

	BP_OnBeginFiring(GetActorLocation(), GetActorForwardVector());

	// 맞는 순간 즉시 1틱 + 그 뒤로 Interval마다 반복이라 총 틱 수는 "간격 개수(floor) + 1"
	MaxDamageTicks = FMath::Max(1, FMath::FloorToInt(FiringDuration / FMath::Max(LaserDamageTickInterval, 0.01f) + KINDA_SMALL_NUMBER) + 1);
	CurrentDamageTick = 0;

	ApplyLaserDamageTick();

	if (Phase == EEchidnaMirrorPhase::Firing && LaserDamageTickInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DamageTimerHandle, this,
			&AEchidnaMirrorActor::ApplyLaserDamageTick,
			LaserDamageTickInterval, true);
	}
}

void AEchidnaMirrorActor::GetActorsInBeamBox(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	UWorld* World = GetWorld();
	if (!World) return;

	APawn* InstigatorPawn = InstigatorController.IsValid()
		? InstigatorController->GetPawn() : nullptr;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (InstigatorPawn) QueryParams.AddIgnoredActor(InstigatorPawn);

	const FVector Origin = GetActorLocation();
	const FVector Direction = GetActorForwardVector();
	const FVector BoxCenter = Origin + Direction * (MaxRange * 0.5f);
	const FCollisionShape Box = FCollisionShape::MakeBox(
		FVector(MaxRange * 0.5f, BeamHalfWidth, BeamHalfHeight));

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps, BoxCenter, GetActorRotation().Quaternion(),
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
		Box, QueryParams);

	DrawDebugBox(World, BoxCenter,
		FVector(MaxRange * 0.5f, BeamHalfWidth, BeamHalfHeight),
		GetActorRotation().Quaternion(), FColor::Red, false, 0.2f);

	TSet<AActor*> Unique;
	for (const FOverlapResult& Hit : Overlaps)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			Unique.Add(HitActor);
		}
	}
	OutActors = Unique.Array();
}

void AEchidnaMirrorActor::ApplyLaserDamageTick()
{
	if (Phase != EEchidnaMirrorPhase::Firing) return;

	CurrentDamageTick++;

	TArray<AActor*> HitActors;
	GetActorsInBeamBox(HitActors);
	for (AActor* HitActor : HitActors)
	{
		UGameplayStatics::ApplyDamage(
			HitActor, TickDamage, InstigatorController.Get(),
			this, UDamageType::StaticClass());

		// 맞을 때마다 넉다운(뒤로 튕겨나감) — 착지하기 전에 다음 틱이 오면 계속 다시 띄워지므로
		// 4틱(1초)을 맞는 동안은 쭉 공중에 떠 있다가, 마지막 틱 이후 착지하면서 실제로 넘어짐
		if (ALoACharacter* HitCharacter = Cast<ALoACharacter>(HitActor))
		{
			HitCharacter->ApplyKnockdown(GetActorLocation());
		}
	}

	if (CurrentDamageTick >= MaxDamageTicks)
	{
		FinishFiring();
	}
}

void AEchidnaMirrorActor::FinishFiring()
{
	GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	Phase = EEchidnaMirrorPhase::Done;

	if (ZoneMeshComp)
	{
		ZoneMeshComp->SetVisibility(false);
	}

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirror] FinishFiring — Loc=%s"), *GetActorLocation().ToString());

	SetLifeSpan(LifeAfterBeam);
}

void AEchidnaMirrorActor::ApplyPhaseVisuals(UMaterialInterface* BaseMaterial, const FLinearColor& Color)
{
	for (UStaticMeshComponent* Comp : { MirrorMeshComp, ZoneMeshComp })
	{
		if (!Comp) continue;

		UMaterialInterface* Source = BaseMaterial ? BaseMaterial : Comp->GetMaterial(0);
		if (!Source) continue;

		UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Source);
		if (MID)
		{
			// 장판만 ZoneOpacity를 Alpha로 실어 반투명하게, 거울 몸체는 불투명 유지
			const float Alpha = (Comp == ZoneMeshComp) ? ZoneOpacity : 1.f;
			MID->SetVectorParameterValue(ColorParameterName, FLinearColor(Color.R, Color.G, Color.B, Alpha));
		}
	}
}
