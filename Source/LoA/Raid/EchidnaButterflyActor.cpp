#include "Raid/EchidnaButterflyActor.h"
#include "Raid/EchidnaBoss.h"
#include "Raid/HexArena.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "Components/SphereComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 앞/뒤 양쪽 감김 삼각형 (다른 PMC 액터들과 같은 방식)
	void AddWingTri(TArray<FVector>& Verts, TArray<int32>& Tris, const FVector& A, const FVector& B, const FVector& C)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 1 });
	}
}

AEchidnaButterflyActor::AEchidnaButterflyActor()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	SetRootComponent(CollisionComp);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComp->SetGenerateOverlapEvents(true);

	WingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WingMesh"));
	WingMesh->SetupAttachment(CollisionComp);
	WingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WingMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		WingMesh->SetMaterial(0, MatFinder.Object);
	}
}

void AEchidnaButterflyActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — 비행·방향 전환이 Tick 기반이라 이 값으로 자동 가속
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	// EditDefaultsOnly 값은 BP 오버라이드가 반영된 뒤(BeginPlay)에 적용
	CollisionComp->SetSphereRadius(CollisionRadius);

	if (UMaterialInterface* BaseMat = WingMesh->GetMaterial(0))
	{
		WingMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		WingMID->SetVectorParameterValue(ColorParameterName, WingColor);
	}
	BuildWings();

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AEchidnaButterflyActor::OnOverlap);
}

void AEchidnaButterflyActor::Activate(AHexArena* InArena, float InStunDuration)
{
	Arena = InArena;
	StunDuration = InStunDuration;
	FlapTime = FMath::FRand();	// 나비마다 날갯짓 박자가 다르게

	const float Yaw = FMath::FRandRange(0.f, 360.f);
	SetFlyDirection(FRotator(0.f, Yaw, 0.f).Vector());
	bActive = true;
}

void AEchidnaButterflyActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 날갯짓 — 날개 폭(Y)만 흔든다
	FlapTime += DeltaTime;
	const float Flap = 0.25f + 0.75f * FMath::Abs(FMath::Cos(FlapTime * FlapFrequency * PI));
	WingMesh->SetRelativeScale3D(FVector(1.f, Flap, 1.f));

	if (!bActive) return;

	WanderElapsed += DeltaTime;
	if (WanderElapsed >= WanderInterval)
	{
		WanderElapsed = 0.f;
		const float Turn = FMath::FRandRange(-WanderAngle, WanderAngle);
		SetFlyDirection(FRotator(0.f, Turn, 0.f).RotateVector(FlyDirection));
	}

	// 다음 위치가 아레나 밖이면 안쪽으로 방향을 바꾸고 이번 틱은 제자리
	const FVector Next = GetActorLocation() + FlyDirection * Speed * DeltaTime;
	FIntPoint Coord;
	if (AHexArena* A = Arena.Get(); A && !A->WorldToTileCoord(Next, Coord))
	{
		PickDirectionTowardCenter();
		return;
	}

	SetActorLocation(Next);
}

void AEchidnaButterflyActor::PickDirectionTowardCenter()
{
	AHexArena* A = Arena.Get();
	FVector Center;
	if (!A || !A->GetTileTopLocation(FIntPoint(0, 0), Center))
	{
		SetFlyDirection(-FlyDirection);
		return;
	}

	// 중심 쪽 ±45도 안에서 랜덤 — 벽에 닿을 때마다 딱 중심으로만 모이지 않게
	const FVector ToCenter = (Center - GetActorLocation()).GetSafeNormal2D();
	SetFlyDirection(FRotator(0.f, FMath::FRandRange(-45.f, 45.f), 0.f).RotateVector(ToCenter));
	WanderElapsed = 0.f;
}

void AEchidnaButterflyActor::SetFlyDirection(const FVector& NewDir)
{
	const FVector Dir2D = NewDir.GetSafeNormal2D();
	if (Dir2D.IsNearlyZero()) return;
	FlyDirection = Dir2D;
	SetActorRotation(FRotator(0.f, FlyDirection.Rotation().Yaw, 0.f));
}

void AEchidnaButterflyActor::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bActive) return;

	ALoACharacter* Character = Cast<ALoACharacter>(OtherActor);
	if (!Character) return;

	// 이미 기절 중이면 통과 — 나비가 겹쳐 지나갈 때마다 10초가 계속 갱신되면 패턴 내내 못 움직인다
	if (Character->IsStunned()) return;

	Character->ApplyStun(StunDuration);
	UE_LOG(LogLoA, Log, TEXT("[Butterfly] %s 기절 %.1f초"), *Character->GetName(), StunDuration);

	bActive = false;
	Destroy();
}

void AEchidnaButterflyActor::BuildWings()
{
	// 로컬 +X가 비행 방향. 날개 두 장(좌/우)을 위에서 보이는 수평 판으로 — 앞날개가 크고 뒷날개가 작은 나비 실루엣
	const float S = WingSize;
	TArray<FVector> Verts;
	TArray<int32> Tris;

	for (const float Side : { 1.f, -1.f })
	{
		const FVector Root(0.f, 0.f, 0.f);
		// 앞날개
		AddWingTri(Verts, Tris, Root, FVector(0.55f * S, 0.55f * S * Side, 0.f), FVector(0.1f * S, 1.0f * S * Side, 0.f));
		AddWingTri(Verts, Tris, Root, FVector(0.1f * S, 1.0f * S * Side, 0.f), FVector(-0.15f * S, 0.6f * S * Side, 0.f));
		// 뒷날개
		AddWingTri(Verts, Tris, Root, FVector(-0.15f * S, 0.6f * S * Side, 0.f), FVector(-0.6f * S, 0.45f * S * Side, 0.f));
	}
	// 몸통
	AddWingTri(Verts, Tris, FVector(0.35f * S, 0.f, 0.5f), FVector(-0.45f * S, 0.06f * S, 0.5f), FVector(-0.45f * S, -0.06f * S, 0.5f));

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Verts.Num());

	WingMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (WingMID)
	{
		WingMesh->SetMaterial(0, WingMID);
	}
}
