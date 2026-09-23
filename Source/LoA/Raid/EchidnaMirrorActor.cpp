#include "Raid/EchidnaMirrorActor.h"
#include "Raid/EchidnaBoss.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LoACharacter.h"
#include "LoA.h"

AEchidnaMirrorActor::AEchidnaMirrorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// 거울 프롭 메시가 따로 임포트돼 있지 않아서(EchidnaModling/other/Mirror엔 텍스처만 있음),
	// 엔진 기본 Cylinder를 납작하게 눌러 원반(거울) 모양으로 흉내내고 M_EchidnaMirrorSurface(거울 텍스처로 만든
	// 실제 머티리얼)를 입힘 — 구(Sphere) + 단색 대신 "동그랗고 거울처럼 보이는" 형태
	MirrorMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMeshComp"));
	MirrorMeshComp->SetupAttachment(Root);
	MirrorMeshComp->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	MirrorMeshComp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // 원판의 납작한 면이 정면(로컬 +X)을 보도록 눕힘
	MirrorMeshComp->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.15f));
	MirrorMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MirrorMeshComp->SetCastShadow(false);
	if (CylinderMeshFinder.Succeeded()) MirrorMeshComp->SetStaticMesh(CylinderMeshFinder.Object);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MirrorSurfaceMatFinder(TEXT("/Game/LostArk/Raid/Echidna/Pattern/M_EchidnaMirrorSurface.M_EchidnaMirrorSurface"));
	if (MirrorSurfaceMatFinder.Succeeded()) MirrorMeshComp->SetMaterial(0, MirrorSurfaceMatFinder.Object);

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

	// 광폭화 중에 스폰된 패턴은 Tick 기반 진행(이동·추적·연출)이 보스와 같은 배율로 빨라진다.
	// 월드 타이머는 이 값을 따르지 않으므로 SetTimer 쪽은 시간을 CustomTimeDilation으로 나눠서 건다
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);
}

void AEchidnaMirrorActor::Activate(float InTickDamage, AController* InInstigator)
{
	TickDamage = InTickDamage;
	InstigatorController = InInstigator;
	ElapsedTracking = 0.f;
	CurrentDamageTick = 0;
	bActivated = true;
	SkyGuidedElapsed = 0.f;
	bStopRequested = false;

	// 하늘 유도 모드는 예고(Tracking/반투명 예상범위) 단계 자체가 없음 — 스폰 즉시 "발사 중" 상태로 들어가
	// 상시로 데미지 틱을 반복하다가 StopRepeating()이 불릴 때만 멈춤. 다른 패턴들의 Tracking→Firing
	// 사이클과는 완전히 별개 경로라 여기서 바로 처리하고 return
	if (bSkyGuidedMode)
	{
		Phase = EEchidnaMirrorPhase::Firing;
		ApplyPhaseVisuals(FiringZoneMaterial, FiringColor, FiringOpacity);
		UpdateZoneTransform(SkyGuidedBeamRange);
		UpdateMirrorBodyRotation();

		ApplyLaserDamageTick();
		if (LaserDamageTickInterval > 0.f)
		{
			GetWorldTimerManager().SetTimer(
				DamageTimerHandle, this, &AEchidnaMirrorActor::ApplyLaserDamageTick, LaserDamageTickInterval / CustomTimeDilation, true);
		}

		UE_LOG(LogLoA, Log, TEXT("[EchidnaMirror] Activate(SkyGuided) — 상시 발사 시작 Loc=%s"), *GetActorLocation().ToString());
		return;
	}

	Phase = EEchidnaMirrorPhase::Tracking;
	ApplyPhaseVisuals(TrackingZoneMaterial, TrackingColor, ZoneOpacity);

	if (bLockDirectionOnSpawn)
	{
		// 고정 스포크 모드 — 플레이어를 조준하지 않고 스폰 Rotation 그대로, 장판 길이도 처음부터 MaxRange 고정
		UpdateZoneTransform(MaxRange);
	}
	// 스폰 직후 첫 프레임부터 플레이어 방향으로 즉시 정렬 + 장판 표시 (bSkyGuidedMode는 위에서 이미 return됨)
	else if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
	{
		FVector ToPlayer = PlayerChar->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			SetActorRotation(ToPlayer.GetSafeNormal().Rotation());
		}
		UpdateZoneTransform(FMath::Min(ToPlayer.Size(), MaxRange));
	}
	else
	{
		UpdateZoneTransform(MaxRange * 0.5f);
	}

	UpdateMirrorBodyRotation();

	UE_LOG(LogLoA, Log, TEXT("[EchidnaMirror] Activate — Loc=%s TrackingDuration=%.1f FiringDuration=%.1f LockDirection=%s SkyGuided=%s"),
		*GetActorLocation().ToString(), TrackingDuration, FiringDuration,
		bLockDirectionOnSpawn ? TEXT("true") : TEXT("false"), bSkyGuidedMode ? TEXT("true") : TEXT("false"));
}

void AEchidnaMirrorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Activate()가 호출되기 전까지는(예: "8거울"에서 미리 스폰만 해두고 대기 중인 스포크) 아무 것도 진행하지 않음
	if (!bActivated) return;

	// 하늘 유도 모드는 상시 발사 상태(Activate에서 바로 Firing으로 진입)라 아래 "Tracking 중에만" 게이트를
	// 절대 안 타므로, 이동/조준/몸체회전을 전부 여기서 Phase와 무관하게 직접 처리함.
	// 스폰 직후 SkyGuidedFollowDelay 동안은 제자리 유지, 그 뒤로는 플레이어의 X/Y를 향해
	// "플레이어 이동속도 x SkyGuidedFollowSpeedRatio" 속도로 서서히 따라감(순간이동 아님) —
	// 그래야 거리를 벌리면 실제로 따돌릴 수 있어서 무조건 맞는 판정이 안 됨. Z는 스폰 높이 그대로 유지
	if (bSkyGuidedMode)
	{
		SkyGuidedElapsed += DeltaTime;
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
		{
			if (SkyGuidedElapsed >= SkyGuidedFollowDelay)
			{
				const FVector PlayerLoc = PlayerChar->GetActorLocation();
				const FVector CurrentLoc = GetActorLocation();
				const FVector TargetLoc(PlayerLoc.X, PlayerLoc.Y, CurrentLoc.Z);

				float FollowSpeed = 400.f;
				if (const UCharacterMovementComponent* PlayerMovement = PlayerChar->GetCharacterMovement())
				{
					FollowSpeed = PlayerMovement->MaxWalkSpeed * SkyGuidedFollowSpeedRatio;
				}

				SetActorLocation(FMath::VInterpConstantTo(CurrentLoc, TargetLoc, DeltaTime, FollowSpeed));
			}

			// 실제 레이저 조준(액터 Rotation) — 항상 "나를 보고 있어야" 하므로 발사 중에도 계속 갱신
			const FVector ToPlayer = PlayerChar->GetActorLocation() - GetActorLocation(); // 수직 성분 유지(위→아래 각도)
			if (!ToPlayer.IsNearlyZero())
			{
				const FRotator TargetRotation = ToPlayer.GetSafeNormal().Rotation();
				SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), TargetRotation, DeltaTime, SkyGuidedAimRotationSpeed));
			}
			UpdateZoneTransform(SkyGuidedBeamRange);
		}

		// 몸체 회전은 지연/이동 여부와 무관하게 매 틱 갱신 — 스폰 직후 대기 중에도 이미 플레이어를 쳐다보고 있음
		UpdateMirrorBodyRotation();
		return;
	}

	if (Phase != EEchidnaMirrorPhase::Tracking)
	{
		return;
	}

	if (!bLockDirectionOnSpawn)
	{
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
		{
			FVector ToPlayer = PlayerChar->GetActorLocation() - GetActorLocation();
			ToPlayer.Z = 0.f;
			if (!ToPlayer.IsNearlyZero())
			{
				// 즉시 스냅하지 않고 일정 각속도로 따라가게 — 대시 같은 순간이동에도 즉시 안 꺾임
				const FRotator TargetRotation = ToPlayer.GetSafeNormal().Rotation();
				SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), TargetRotation, DeltaTime, TrackingRotationSpeed));
			}
			UpdateZoneTransform(FMath::Min(ToPlayer.Size(), MaxRange));
			UpdateMirrorBodyRotation();
		}
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

void AEchidnaMirrorActor::UpdateMirrorBodyRotation()
{
	if (!MirrorMeshComp) return;

	if (bSkyGuidedMode)
	{
		// 몸체는 액터 자체의 회전(조준용, 발사 중엔 고정됨)에 의존하지 않고 그때그때 플레이어 방향을
		// 직접 계산해서 씀 — 그래야 발사로 조준이 잠겨있는 동안에도, 이동하면서도 항상 플레이어 쪽을
		// "쳐다보는" 것처럼 보임(4거울이 추적 중 계속 플레이어를 향해 도는 것과 같은 느낌).
		// 실제 레이저 조준(액터 Rotation)은 건드리지 않으므로 발사 방향 고정 로직에는 영향 없음
		float BodyYaw = GetActorRotation().Yaw;
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(this, 0))
		{
			FVector ToPlayer = PlayerChar->GetActorLocation() - GetActorLocation();
			ToPlayer.Z = 0.f;
			if (!ToPlayer.IsNearlyZero())
			{
				BodyYaw = ToPlayer.Rotation().Yaw;
			}
		}

		// 액터 자체는 플레이어를 조준하느라 거의 수직으로(Pitch≈-90도) 아래를 보게 되는데, 몸체까지 그 Pitch를
		// 그대로 물려받으면 "서 있는" 원판이 도로 눕는 것처럼 보임 — 그래서 Pitch는 고정 90도(세워진 자세)로 둠
		MirrorMeshComp->SetWorldRotation(FRotator(90.f, BodyYaw, 0.f));
	}
	else
	{
		// 일반 모드 — 액터가 수평으로만 회전하므로 상대 회전 고정값 그대로 두면 항상 정면을 보고 서 있음
		MirrorMeshComp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	}
}

void AEchidnaMirrorActor::BeginFiring()
{
	Phase = EEchidnaMirrorPhase::Firing;

	// 방향 고정, 장판 길이를 고정 사거리로 고정 (하늘 유도 모드는 SkyGuidedBeamRange, 나머지는 MaxRange)
	UpdateZoneTransform(GetEffectiveBeamRange());

	ApplyPhaseVisuals(FiringZoneMaterial, FiringColor, FiringOpacity);

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
			LaserDamageTickInterval / CustomTimeDilation, true);
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

	// 실제 판정 박스 길이 — 하늘 유도 모드는 SkyGuidedBeamRange로 짧게 고정(그 밖에 있으면 안 맞음),
	// 그 외 패턴은 기존처럼 MaxRange. Zone 비주얼(UpdateZoneTransform)과 반드시 같은 값을 써야 눈에 보이는
	// 장판 길이와 실제 판정 범위가 일치함
	const float BeamRange = GetEffectiveBeamRange();
	const FVector Origin = GetActorLocation();
	const FVector Direction = GetActorForwardVector();
	const FVector BoxCenter = Origin + Direction * (BeamRange * 0.5f);
	const FCollisionShape Box = FCollisionShape::MakeBox(
		FVector(BeamRange * 0.5f, BeamHalfWidth, BeamHalfHeight));

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps, BoxCenter, GetActorRotation().Quaternion(),
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
		Box, QueryParams);

	DrawDebugBox(World, BoxCenter,
		FVector(BeamRange * 0.5f, BeamHalfWidth, BeamHalfHeight),
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

	// 하늘 유도 모드는 틱 수 제한 없이 StopRepeating()이 불릴 때까지 무한 반복 발사
	if (bSkyGuidedMode)
	{
		if (bStopRequested)
		{
			FinishFiring();
		}
		return;
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

void AEchidnaMirrorActor::ApplyPhaseVisuals(UMaterialInterface* BaseMaterial, const FLinearColor& Color, float InZoneOpacity)
{
	// 장판(ZoneMeshComp)만 단계별 색/불투명도를 입힘 — 거울 몸체(MirrorMeshComp)는 생성자에서 지정한
	// M_EchidnaMirrorSurface(실제 거울 텍스처) 그대로 유지. 예전엔 몸체도 같이 틴트했는데,
	// 그러면 매번 이 함수가 몸체 머티리얼을 BaseMaterial(레이저용 M_MirrorLaser) 기반 MID로 덮어써서
	// 거울 텍스처가 안 보이게 됨
	if (!ZoneMeshComp) return;

	// FiringZoneMaterial이 미설정(None)인 경우 "Tracking 단계가 먼저 실행되어 이미 올바른 머티리얼(보통
	// TrackingZoneMaterial=M_MirrorLaser)로 세팅해뒀을 것"을 가정하고 현재 컴포넌트 머티리얼을 썼는데,
	// 하늘 유도 모드는 Tracking을 건너뛰고 바로 Firing부터 시작하므로 그 가정이 깨져 생성자 기본값
	// (BasicShapeMaterial, Base Color 파라미터 없음)을 그대로 쓰게 되는 문제가 있었음 —
	// TrackingZoneMaterial을 2차 fallback으로 둬서 항상 올바른(파라미터 있는) 머티리얼을 쓰도록 보정
	UMaterialInterface* Source = BaseMaterial;
	if (!Source) Source = TrackingZoneMaterial.Get();
	if (!Source) Source = ZoneMeshComp->GetMaterial(0);
	if (!Source) return;

	UMaterialInstanceDynamic* MID = ZoneMeshComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Source);
	if (MID)
	{
		MID->SetVectorParameterValue(ColorParameterName, FLinearColor(Color.R, Color.G, Color.B, InZoneOpacity));
	}
}
