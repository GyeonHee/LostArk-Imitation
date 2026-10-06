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
	// 판정 순간의 줄기 연출(뻗기 → 빨려 들어가기)에만 틱을 쓴다 — 평소엔 꺼둠
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

	StrikeMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StrikeMeshComp"));
	StrikeMeshComp->SetupAttachment(Root);
	StrikeMeshComp->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
	StrikeMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StrikeMeshComp->SetCastShadow(false);
	StrikeMeshComp->SetVisibility(false);
	if (PlaneMeshFinder.Succeeded()) StrikeMeshComp->SetStaticMesh(PlaneMeshFinder.Object);
	if (DefaultMatFinder.Succeeded()) StrikeMeshComp->SetMaterial(0, DefaultMatFinder.Object);
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
	// 판정이 실제로 발동하는 순간 — 예고 장판은 숨기고 핑크 줄기가 판정 길이만큼 뻗어 나간다
	if (ZoneMeshComp)
	{
		ZoneMeshComp->SetVisibility(false);
	}
	if (StrikeMeshComp)
	{
		if (UMaterialInterface* Source = StrikeMeshComp->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* MID = StrikeMeshComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Source))
			{
				MID->SetVectorParameterValue(ColorParameterName, FLinearColor(StrikeColor.R, StrikeColor.G, StrikeColor.B, StrikeOpacity));
			}
		}
		StrikeElapsed = 0.f;
		SetStrikeLength(0.f);
		SetActorTickEnabled(true);
	}

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
	// 줄기가 아직 빨려 들어가는 중이면 그게 끝날 때까지는 남아 있어야 한다 (리본처럼 판정 직후 바로 끝나는 경우)
	const float RemainingStrike = StrikeElapsed >= 0.f ? FMath::Max(GetStrikeTotalDuration() - StrikeElapsed, 0.f) / FMath::Max(CustomTimeDilation, 0.01f) : 0.f;
	SetLifeSpan(FMath::Max(LifeAfterSnap, RemainingStrike + 0.05f));
}

void AEchidnaTetherActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (StrikeElapsed < 0.f) return;

	// DeltaTime은 CustomTimeDilation(광폭화)이 이미 곱해진 값 — 연출도 같이 빨라진다
	StrikeElapsed += DeltaTime;

	float Alpha;
	if (StrikeElapsed < StrikeExtendDuration)
	{
		// 뻗기 — 빠르게 튀어나갔다가 끝에서 감속 (ease-out)
		Alpha = FMath::InterpEaseOut(0.f, 1.f, StrikeElapsed / StrikeExtendDuration, 2.f);
	}
	else if (StrikeElapsed < StrikeExtendDuration + StrikeHoldDuration)
	{
		Alpha = 1.f;
	}
	else
	{
		// 빨려 들어가기 — 처음엔 천천히, 갈수록 빠르게 (ease-in)
		const float T = StrikeRetractDuration > 0.f ? (StrikeElapsed - StrikeExtendDuration - StrikeHoldDuration) / StrikeRetractDuration : 1.f;
		Alpha = 1.f - FMath::InterpEaseIn(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f), 2.f);
	}

	SetStrikeLength(TetherRange * Alpha);

	if (StrikeElapsed >= GetStrikeTotalDuration())
	{
		SetStrikeLength(0.f);
		StrikeElapsed = -1.f;
		SetActorTickEnabled(false);
	}
}

void AEchidnaTetherActor::SetStrikeLength(float Length)
{
	if (!StrikeMeshComp) return;

	// 뿌리는 항상 원점(보스 쪽)에 고정하고 끝만 움직인다 — Plane은 100x100(cm) 중심 기준이라 길이 절반만큼 앞으로
	if (Length < 1.f)
	{
		StrikeMeshComp->SetVisibility(false);
		return;
	}
	StrikeMeshComp->SetVisibility(true);
	StrikeMeshComp->SetRelativeLocation(FVector(Length * 0.5f, 0.f, 6.f));
	StrikeMeshComp->SetRelativeScale3D(FVector(Length / 100.f, (TetherHalfWidth * 2.f) / 100.f, 1.f));
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
