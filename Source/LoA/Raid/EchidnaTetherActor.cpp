#include "Raid/EchidnaTetherActor.h"
#include "Raid/EchidnaBoss.h"
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

	// 광폭화 중에 스폰된 패턴은 Tick 기반 진행(이동·추적·연출)이 보스와 같은 배율로 빨라진다.
	// 월드 타이머는 이 값을 따르지 않으므로 SetTimer 쪽은 시간을 CustomTimeDilation으로 나눠서 건다
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);
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
		GetWorldTimerManager().SetTimer(SnapTimerHandle, this, &AEchidnaTetherActor::PerformSnap, SnapDelay / CustomTimeDilation, false);
	}
	else
	{
		PerformSnap();
	}
}

void AEchidnaTetherActor::PerformSnap()
{
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
				if (bApplyPullOnHit)
				{
					HitCharacter->ApplyPull(PullTarget, PullStrength);
				}

				// 2갈래가 겹치는 구간에서 양쪽에 다 맞아도 매혹은 1스택만 쌓여야 한다.
				// 갈래들은 서로를 모르는 별개 액터라, 먼저 맞은 쪽이 걸어둔 기절을 신호로 삼아 중복을 막는다
				// (전방향 하트발사의 "기절 중 매혹 중복 축적 방지"와 동일한 방식).
				// **ApplyStun()을 부르기 전에 검사해야 한다** — 부르는 순간 bIsStunned가 true로 바뀌어 판정이 무의미해짐
				const bool bAlreadyStunned = HitCharacter->IsStunned();

				if (bApplyStunOnHit)
				{
					HitCharacter->ApplyStun(StunDuration);
				}

				if (CharmGaugeAmount > 0 && !(bApplyStunOnHit && bAlreadyStunned))
				{
					HitCharacter->AddCharmGauge(CharmGaugeAmount);
				}

				bDidHit = true;
			}
		}

		UE_LOG(LogLoA, Log, TEXT("[EchidnaTether] PerformSnap — %d명 당김 판정"), Unique.Num());
	}

	// 아무도 안 맞았거나, 애초에 끌기를 안 쓰는 설정(리본)이면 기다릴 이유가 없으므로 바로 종료 처리
	if (!bDidHit || !bApplyPullOnHit)
	{
		FinishSnap();
		return;
	}

	// 맞은 대상이 "멈췄다가 보스 앞까지 끌려오는" 동안은 아직 끝난 게 아니다 —
	// 여기서 기다리지 않으면 아직 끌려오는 중인데 다음 단계 장판이 터져버린다
	GetWorldTimerManager().SetTimer(ResolveTimerHandle, this, &AEchidnaTetherActor::FinishSnap, PullResolveDelay, false);
}

void AEchidnaTetherActor::FinishSnap()
{
	bSnapped = true;

	// 불투명하게 바뀐 실행 범위를 LifeAfterSnap 동안 그대로 보여준 뒤 소멸.
	// StateTree가 DidHit()을 읽기 전에 사라지면 "아무도 안 맞음"으로 오판하므로 소멸은 여기서야 예약한다
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
