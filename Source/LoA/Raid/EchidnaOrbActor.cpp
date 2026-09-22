#include "Raid/EchidnaOrbActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "LoACharacter.h"

AEchidnaOrbActor::AEchidnaOrbActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 거울 액터와 같은 컨벤션 — 엔진 기본 Sphere + M_MirrorLaser("Base Color" 파라미터)를 박아둬서
	// VFX 에셋을 따로 안 만들어도 스폰만 되면 무조건 보인다
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));

	OrbMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMeshComp"));
	OrbMeshComp->SetupAttachment(Root);
	OrbMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OrbMeshComp->SetCastShadow(false);
	if (SphereMeshFinder.Succeeded()) OrbMeshComp->SetStaticMesh(SphereMeshFinder.Object);
	if (MatFinder.Succeeded()) BaseMaterial = MatFinder.Object;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->SetupAttachment(Root);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AEchidnaOrbActor::BeginPlay()
{
	Super::BeginPlay();

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AEchidnaOrbActor::OnOverlapBegin);

	// EditDefaultsOnly 값은 BP 서브클래스에서 덮어쓸 수 있으므로 생성자가 아니라 여기서 적용
	OrbMeshComp->SetRelativeScale3D(FVector(OrbVisualScale));
	CollisionComp->SetSphereRadius(OrbCollisionRadius);

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* MID = OrbMeshComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
		{
			MID->SetVectorParameterValue(ColorParameterName, OrbColor);
		}
	}
}

void AEchidnaOrbActor::ApplyOverrides(float InVisualScale, float InCollisionRadius, float InSpeed, float InMaxRange)
{
	if (InVisualScale > 0.f)
	{
		OrbVisualScale = InVisualScale;
		if (OrbMeshComp) OrbMeshComp->SetRelativeScale3D(FVector(OrbVisualScale));
	}

	if (InCollisionRadius > 0.f)
	{
		OrbCollisionRadius = InCollisionRadius;
		if (CollisionComp) CollisionComp->SetSphereRadius(OrbCollisionRadius);
	}

	if (InSpeed > 0.f)    Speed = InSpeed;
	if (InMaxRange > 0.f) MaxRange = InMaxRange;
}

void AEchidnaOrbActor::Launch(const FVector& Direction, AController* InInstigator, float InDamage)
{
	FlyDirection = Direction.GetSafeNormal();
	InstigatorController = InInstigator;
	Damage = InDamage;
	TraveledDistance = 0.f;
	Phase = EEchidnaOrbPhase::Outgoing;
	AlreadyHit.Reset();
	bLaunched = true;

	if (!FlyDirection.IsNearlyZero())
	{
		SetActorRotation(FlyDirection.Rotation());
	}
}

void AEchidnaOrbActor::BeginReturn()
{
	Phase = EEchidnaOrbPhase::Returning;

	// 복귀 구간의 이동 거리를 따로 세기 위해 리셋 — 이 구간은 2*MaxRange를 간다
	TraveledDistance = 0.f;

	// 돌아올 때 다시 맞을 수 있어야 한다("6개 구체 모두 되돌아오니 주의") — 구간이 바뀌었으니 피격 기록 초기화
	AlreadyHit.Reset();
}

void AEchidnaOrbActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bLaunched || Phase == EEchidnaOrbPhase::Done) return;

	const float Step = Speed * DeltaTime;

	if (Phase == EEchidnaOrbPhase::Outgoing)
	{
		AddActorWorldOffset(FlyDirection * Step, true);
		TraveledDistance += Step;

		if (TraveledDistance >= MaxRange)
		{
			BeginReturn();
		}
		return;
	}

	// 복귀 — 왔던 경로를 그대로 되짚되 **시작 지점에서 멈추지 않고 관통해서 반대편까지** 간다.
	// 보스 기준 +MaxRange까지 갔다가 → 0(발사 지점)을 지나 → -MaxRange까지. 그래서 복귀 구간의
	// 이동 거리는 MaxRange가 아니라 2*MaxRange다
	AddActorWorldOffset(-FlyDirection * Step, true);
	TraveledDistance += Step;

	if (TraveledDistance >= MaxRange * 2.f)
	{
		Phase = EEchidnaOrbPhase::Done;
		SetLifeSpan(LifeAfterReturn);
	}
}

void AEchidnaOrbActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bLaunched || Phase == EEchidnaOrbPhase::Done) return;

	ALoACharacter* Character = Cast<ALoACharacter>(OtherActor);
	if (!Character) return;

	// 하트와 달리 맞아도 소멸하지 않는다 — 계속 날아가서 되돌아와야 하므로.
	// 대신 같은 구간에서 같은 대상이 중복으로 맞지는 않게 기록해둔다
	if (AlreadyHit.Contains(Character)) return;
	AlreadyHit.Add(Character);

	ApplyHit(Character);
}

void AEchidnaOrbActor::ApplyHit(AActor* Target)
{
	UGameplayStatics::ApplyDamage(Target, Damage, InstigatorController.Get(), this, UDamageType::StaticClass());

	if (bApplyKnockdownOnHit)
	{
		if (ALoACharacter* Character = Cast<ALoACharacter>(Target))
		{
			Character->ApplyKnockdown(GetActorLocation());
		}
	}
}
