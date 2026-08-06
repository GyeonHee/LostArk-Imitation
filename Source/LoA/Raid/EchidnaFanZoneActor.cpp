#include "Raid/EchidnaFanZoneActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "LoACharacter.h"
#include "LoA.h"

namespace
{
	// 4점으로 쿼드(사다리꼴 조각) 추가 — HexArena::AddQuad와 동일한 방식으로,
	// UE의 실제 front-face 감김 방향에 의존하지 않도록 양쪽 감김 순서를 모두 추가해 항상 양면이 보이게 한다.
	void AddFanQuad(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals,
		TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float V0, float V1)
	{
		const int32 Base = Verts.Num();
		Verts.Add(P0); Verts.Add(P1); Verts.Add(P2); Verts.Add(P3);

		for (int32 i = 0; i < 4; i++)
		{
			Normals.Add(FVector::UpVector);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}

		UVs.Add(FVector2D(0.f, V0));
		UVs.Add(FVector2D(1.f, V0));
		UVs.Add(FVector2D(1.f, V1));
		UVs.Add(FVector2D(0.f, V1));

		// 정면 감김
		Tris.Add(Base + 0); Tris.Add(Base + 1); Tris.Add(Base + 2);
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 3);
		// 역방향 감김 (양면 렌더링)
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 1);
		Tris.Add(Base + 0); Tris.Add(Base + 3); Tris.Add(Base + 2);
	}
}

AEchidnaFanZoneActor::AEchidnaFanZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	FanMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FanMeshComp"));
	FanMeshComp->SetupAttachment(Root);
	FanMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FanMeshComp->SetCastShadow(false);
	if (DefaultMatFinder.Succeeded()) FanMeshComp->SetMaterial(0, DefaultMatFinder.Object);
}

void AEchidnaFanZoneActor::BeginPlay()
{
	Super::BeginPlay();
}

void AEchidnaFanZoneActor::Activate(float InDamage, AController* InInstigator)
{
	Damage = InDamage;
	InstigatorController = InInstigator;
	bExploded = false;
	bStartedExploding = false;
	CurrentRing = 0;

	// 예고 단계 — 전체 부채꼴을 미리 보여주기만 함, 데미지 없음
	BuildFanMesh(FanRange);
	ApplyMeshColor(TelegraphColor);

	UE_LOG(LogLoA, Log, TEXT("[EchidnaFanZone] Activate — Loc=%s Dir=%s FanAngle=%.0f FanRange=%.0f RingCount=%d"),
		*GetActorLocation().ToString(), *GetActorForwardVector().ToString(), FanAngle, FanRange, RingCount);

	if (TelegraphDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			TelegraphTimerHandle, this, &AEchidnaFanZoneActor::BeginRingExpansion, TelegraphDuration, false);
	}
	else
	{
		BeginRingExpansion();
	}
}

void AEchidnaFanZoneActor::BeginRingExpansion()
{
	bStartedExploding = true;
	ApplyMeshColor(FanColor);

	// 첫 구간은 예고가 끝나자마자 — 별도 대기 없음
	RevealNextRing();

	if (!bExploded && RingCount > 1)
	{
		GetWorldTimerManager().SetTimer(
			RingTimerHandle, this, &AEchidnaFanZoneActor::RevealNextRing, RingInterval, true);
	}
}

void AEchidnaFanZoneActor::RevealNextRing()
{
	if (CurrentRing >= RingCount)
	{
		return;
	}

	const float PrevOuterRadius = FMath::Lerp(FanInnerRadius, FanRange, (float)CurrentRing / (float)RingCount);
	CurrentRing++;
	const float NewOuterRadius = FMath::Lerp(FanInnerRadius, FanRange, (float)CurrentRing / (float)RingCount);

	BuildFanMesh(NewOuterRadius);
	ApplyRingDamage(PrevOuterRadius, NewOuterRadius);

	if (CurrentRing >= RingCount)
	{
		GetWorldTimerManager().ClearTimer(RingTimerHandle);
		bExploded = true;

		BP_OnExplode(GetActorLocation(), GetActorForwardVector());

		UE_LOG(LogLoA, Log, TEXT("[EchidnaFanZone] 전 구간 확장 완료 — Loc=%s"), *GetActorLocation().ToString());

		SetLifeSpan(LifeAfterExplode);
	}
}

void AEchidnaFanZoneActor::BuildFanMesh(float OuterRadius)
{
	if (!FanMeshComp) return;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	const float HalfAngleRad = FMath::DegreesToRadians(FanAngle * 0.5f);
	const int32 Segments = FMath::Max(ArcSegments, 2);
	const float InnerRadius = FMath::Clamp(FanInnerRadius, 0.f, FMath::Max(OuterRadius - 1.f, 0.f));
	const FVector ZOffset(0.f, 0.f, 5.f);

	// 로컬 +X(액터 정면) 기준 좌우로 부채꼴을 펼침 — 스폰 시점 Rotation이 이미 플레이어 방향을 보고 있으므로
	// 여기서는 컴포넌트 상대공간에 순수하게 부채꼴만 만들면 됨 (별도 회전 보정 불필요)
	for (int32 i = 0; i < Segments; i++)
	{
		const float T0 = (float)i / (float)Segments;
		const float T1 = (float)(i + 1) / (float)Segments;
		const float AngleA = FMath::Lerp(-HalfAngleRad, HalfAngleRad, T0);
		const float AngleB = FMath::Lerp(-HalfAngleRad, HalfAngleRad, T1);

		const FVector DirA(FMath::Cos(AngleA), FMath::Sin(AngleA), 0.f);
		const FVector DirB(FMath::Cos(AngleB), FMath::Sin(AngleB), 0.f);

		const FVector InnerA = DirA * InnerRadius + ZOffset;
		const FVector InnerB = DirB * InnerRadius + ZOffset;
		const FVector OuterA = DirA * OuterRadius + ZOffset;
		const FVector OuterB = DirB * OuterRadius + ZOffset;

		AddFanQuad(Verts, Tris, Normals, UVs, Colors, Tangents, InnerA, InnerB, OuterB, OuterA, T0, T1);
	}

	FanMeshComp->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
}

bool AEchidnaFanZoneActor::IsActorInRing(const AActor* Actor, float InnerRadius, float OuterRadius) const
{
	if (!Actor) return false;

	FVector ToActor = Actor->GetActorLocation() - GetActorLocation();
	ToActor.Z = 0.f;
	const float Distance = ToActor.Size();
	if (Distance < InnerRadius || Distance > OuterRadius) return false;
	if (Distance < KINDA_SMALL_NUMBER) return true;

	const FVector Dir = ToActor / Distance;
	const float Dot = FVector::DotProduct(GetActorForwardVector(), Dir);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));

	return AngleDeg <= (FanAngle * 0.5f);
}

void AEchidnaFanZoneActor::ApplyRingDamage(float InnerRadius, float OuterRadius)
{
	UWorld* World = GetWorld();
	if (!World) return;

	APawn* InstigatorPawn = InstigatorController.IsValid() ? InstigatorController->GetPawn() : nullptr;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (InstigatorPawn) QueryParams.AddIgnoredActor(InstigatorPawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps, GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
		FCollisionShape::MakeSphere(OuterRadius), QueryParams);

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
		if (IsActorInRing(HitActor, InnerRadius, OuterRadius))
		{
			UGameplayStatics::ApplyDamage(HitActor, Damage, InstigatorController.Get(), this, UDamageType::StaticClass());

			// 부채꼴 판정에 맞으면 넉다운 — 장판 위치 반대 방향으로 뒤로 튕겨나가며 넘어짐.
			// 고리마다 여러 번 겹쳐 맞아도 착지 전까지만 재입력이 반영되고, 착지 후엔 ApplyKnockdown 내부에서 무시됨
			if (ALoACharacter* HitCharacter = Cast<ALoACharacter>(HitActor))
			{
				HitCharacter->ApplyKnockdown(GetActorLocation());
			}
		}
	}
}

void AEchidnaFanZoneActor::ApplyMeshColor(const FLinearColor& Color)
{
	if (!FanMeshComp) return;

	UMaterialInterface* Source = FanMeshComp->GetMaterial(0);
	if (!Source) return;

	UMaterialInstanceDynamic* MID = FanMeshComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Source);
	if (MID)
	{
		MID->SetVectorParameterValue(ColorParameterName, FLinearColor(Color.R, Color.G, Color.B, FanOpacity));
	}
}
