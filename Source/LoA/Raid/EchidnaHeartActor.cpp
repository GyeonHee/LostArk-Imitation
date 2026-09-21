#include "Raid/EchidnaHeartActor.h"
#include "ProceduralMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "LoACharacter.h"

namespace
{
	// 옆면(테두리 두께) 쿼드 하나 추가 — HexArena/EchidnaFanZoneActor의 AddQuad와 동일한 패턴으로,
	// 감김 방향을 앞/뒤 양쪽 다 추가해서 실제 노멀 계산을 정교하게 안 해도 항상 양면이 보이게 함
	void AddHeartSideQuad(TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals,
		TArray<FVector2D>& UVs, TArray<FColor>& Colors, TArray<FProcMeshTangent>& Tangents,
		const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Normal)
	{
		const int32 Base = Verts.Num();
		Verts.Add(P0); Verts.Add(P1); Verts.Add(P2); Verts.Add(P3);

		for (int32 i = 0; i < 4; i++)
		{
			Normals.Add(Normal);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}

		UVs.Add(FVector2D(0.f, 0.f));
		UVs.Add(FVector2D(1.f, 0.f));
		UVs.Add(FVector2D(1.f, 1.f));
		UVs.Add(FVector2D(0.f, 1.f));

		// 정면 감김
		Tris.Add(Base + 0); Tris.Add(Base + 1); Tris.Add(Base + 2);
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 3);
		// 역방향 감김 (양면 렌더링)
		Tris.Add(Base + 0); Tris.Add(Base + 2); Tris.Add(Base + 1);
		Tris.Add(Base + 0); Tris.Add(Base + 3); Tris.Add(Base + 2);
	}
}

AEchidnaHeartActor::AEchidnaHeartActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 하트 모양은 텍스처가 아니라 ProceduralMeshComponent로 직접 압출해서 만듦(BuildHeartMesh, BeginPlay에서 호출) —
	// 거울/부채꼴 패턴과 동일한 컨벤션으로 기본 머티리얼(M_MirrorLaser, "Base Color" 파라미터)을 박아둬서
	// VFX 미할당이어도 스폰만 되면 무조건 보임. Opaque 솔리드라 반투명 관련 설정은 필요 없음
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));

	HeartMeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HeartMeshComp"));
	HeartMeshComp->SetupAttachment(Root);
	HeartMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeartMeshComp->SetCastShadow(false);

	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->SetupAttachment(Root);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AEchidnaHeartActor::BuildHeartMesh()
{
	if (!HeartMeshComp) return;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	const int32 Segments = FMath::Max(HeartSegments, 8);
	const float HalfThickness = FMath::Max(HeartThickness, 0.f) * 0.5f;
	const float CurveScale = 2.f; // 파라메트릭 하트 커브(대략 -17~16 범위)를 적당한 cm 크기로 변환

	// 파라메트릭 하트 커브: X=16*sin^3(t), Y=13*cos(t)-5*cos(2t)-2*cos(3t)-cos(4t) — 뾰족한 부분이 Y 최솟값
	// 쪽에 있는 기본형을 그대로 살려서 로컬 Y(좌우)-Z(상하) 평면에 "세워서" 배치(뾰족한 끝이 아래(-Z)를
	// 향하는 진짜 하트 모양). 두께는 로컬 X(전후) 방향으로 압출 — 즉 평평하게 누운 원반이 아니라 세워진
	// 카드처럼 정면(+X)을 보는 얇은 판. Tick()에서 이 컴포넌트를 로컬 Z축 기준으로 천천히 돌려서
	// "날아가면서 회전하는 카드" 연출을 만듦
	TArray<FVector2D> Boundary; // (LocalY, LocalZ)
	Boundary.Reserve(Segments);
	for (int32 i = 0; i < Segments; i++)
	{
		const float T = (2.f * PI * i) / Segments;
		const float CurveX = 16.f * FMath::Pow(FMath::Sin(T), 3.f);
		const float CurveY = 13.f * FMath::Cos(T) - 5.f * FMath::Cos(2.f * T) - 2.f * FMath::Cos(3.f * T) - FMath::Cos(4.f * T);
		Boundary.Add(FVector2D(CurveX, CurveY) * CurveScale);
	}

	// 정면(+X, 원점에서 팬 삼각분할) — 하트 곡선은 원점(0,0) 기준 스타컨벡스(star-convex)라 팬 분할로 충분
	for (int32 i = 0; i < Segments; i++)
	{
		const int32 Next = (i + 1) % Segments;
		const FVector PC(HalfThickness, 0.f, 0.f);
		const FVector P0(HalfThickness, Boundary[i].X, Boundary[i].Y);
		const FVector P1(HalfThickness, Boundary[Next].X, Boundary[Next].Y);

		const int32 Base = Verts.Num();
		Verts.Add(PC); Verts.Add(P0); Verts.Add(P1);
		for (int32 k = 0; k < 3; k++)
		{
			Normals.Add(FVector::ForwardVector);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::UpVector, false));
		}
		UVs.Add(FVector2D(0.5f, 0.5f));
		UVs.Add(FVector2D(0.f, 0.f));
		UVs.Add(FVector2D(1.f, 0.f));
		Tris.Add(Base + 0); Tris.Add(Base + 1); Tris.Add(Base + 2);
	}

	// 후면(-X, 감김 반대)
	for (int32 i = 0; i < Segments; i++)
	{
		const int32 Next = (i + 1) % Segments;
		const FVector PC(-HalfThickness, 0.f, 0.f);
		const FVector P0(-HalfThickness, Boundary[i].X, Boundary[i].Y);
		const FVector P1(-HalfThickness, Boundary[Next].X, Boundary[Next].Y);

		const int32 Base = Verts.Num();
		Verts.Add(PC); Verts.Add(P1); Verts.Add(P0);
		for (int32 k = 0; k < 3; k++)
		{
			Normals.Add(-FVector::ForwardVector);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::UpVector, false));
		}
		UVs.Add(FVector2D(0.5f, 0.5f));
		UVs.Add(FVector2D(1.f, 0.f));
		UVs.Add(FVector2D(0.f, 0.f));
		Tris.Add(Base + 0); Tris.Add(Base + 1); Tris.Add(Base + 2);
	}

	// 옆면(테두리 두께) — 변마다 쿼드 하나, 양면 감김이라 바깥 노멀 방향을 정확히 계산 안 해도 항상 보임
	for (int32 i = 0; i < Segments; i++)
	{
		const int32 Next = (i + 1) % Segments;
		const FVector Front0(HalfThickness, Boundary[i].X, Boundary[i].Y);
		const FVector Front1(HalfThickness, Boundary[Next].X, Boundary[Next].Y);
		const FVector Back0(-HalfThickness, Boundary[i].X, Boundary[i].Y);
		const FVector Back1(-HalfThickness, Boundary[Next].X, Boundary[Next].Y);

		const FVector2D EdgeDir = (Boundary[Next] - Boundary[i]).GetSafeNormal();
		const FVector OutNormal(0.f, EdgeDir.Y, -EdgeDir.X);

		AddHeartSideQuad(Verts, Tris, Normals, UVs, Colors, Tangents, Back0, Back1, Front1, Front0, OutNormal);
	}

	HeartMeshComp->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* MID = HeartMeshComp->CreateAndSetMaterialInstanceDynamicFromMaterial(0, BaseMaterial))
		{
			MID->SetVectorParameterValue(ColorParameterName, HeartColor);
		}
	}
}

void AEchidnaHeartActor::BeginPlay()
{
	Super::BeginPlay();

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AEchidnaHeartActor::OnOverlapBegin);

	// EditDefaultsOnly 프로퍼티는 BP 서브클래스에서 오버라이드될 수 있으므로 생성자가 아니라 여기서 적용
	HeartMeshComp->SetRelativeScale3D(FVector(HeartVisualScale));
	CollisionComp->SetSphereRadius(HeartCollisionRadius);

	BuildHeartMesh();
}

void AEchidnaHeartActor::Launch(const FVector& Direction, AController* InInstigator, float InSpeed, float InDamage,
	float InStunDuration, int32 InCharmGaugeAmount)
{
	FlyDirection = Direction.GetSafeNormal();
	InstigatorController = InInstigator;
	Speed = InSpeed;
	Damage = InDamage;
	StunDuration = InStunDuration;
	CharmGaugeAmount = InCharmGaugeAmount;
	bLaunched = true;

	if (!FlyDirection.IsNearlyZero())
	{
		SetActorRotation(FlyDirection.Rotation());
	}
}

void AEchidnaHeartActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bLaunched || bHasHit) return;

	// 세워진 카드 모양 메시를 로컬 Z축(세로) 기준으로 천천히 돌려서 "날아가면서 회전하는" 연출 —
	// 예고 마커(Launch 전)는 이 Tick 자체가 여기서 걸러져서 회전 없이 세워진 채로 고정됨
	if (HeartMeshComp && !FMath::IsNearlyZero(HeartSpinSpeed))
	{
		HeartMeshComp->AddLocalRotation(FRotator(0.f, HeartSpinSpeed * DeltaTime, 0.f));
	}

	const FVector DeltaMove = FlyDirection * Speed * DeltaTime;
	AddActorWorldOffset(DeltaMove, true);
	TraveledDistance += DeltaMove.Size();

	if (TraveledDistance >= MaxRange)
	{
		Destroy();
	}
}

void AEchidnaHeartActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bLaunched || bHasHit) return;

	ALoACharacter* Character = Cast<ALoACharacter>(OtherActor);
	if (!Character) return;

	bHasHit = true;
	ApplyHit(Character);
	Destroy();
}

void AEchidnaHeartActor::ApplyHit(AActor* Target)
{
	UGameplayStatics::ApplyDamage(Target, Damage, InstigatorController.Get(), this, UDamageType::StaticClass());

	if (ALoACharacter* Character = Cast<ALoACharacter>(Target))
	{
		// 이미 기절 중(직전 하트에 맞아 3초간 아무것도 못 하는 상태)이면 매혹 게이지는 추가로 안 쌓이게 함 —
		// ApplyStun() 호출 전에 먼저 체크해야 함(호출하면 bIsStunned가 true로 바뀌어버림)
		const bool bAlreadyStunned = Character->IsStunned();

		Character->ApplyStun(StunDuration);

		if (!bAlreadyStunned)
		{
			Character->AddCharmGauge(CharmGaugeAmount);
		}
	}
}
