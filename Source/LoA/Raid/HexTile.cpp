#include "Raid/HexTile.h"
#include "Raid/HexArena.h"
#include "Raid/EchidnaBigFlowerActor.h"
#include "LoACharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AHexTile::AHexTile()
{
	PrimaryActorTick.bCanEverTick = false;

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TileMesh->SetCollisionProfileName(TEXT("BlockAll"));
	SetRootComponent(TileMesh);

	OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
	OverlapBox->SetupAttachment(RootComponent);
	OverlapBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// Z로 넉넉하게 잡아둠 (타일 메시 두께/피벗 위치를 몰라도 캐릭터 캡슐과 확실히 겹치도록 -50~250 범위 커버)
	OverlapBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	OverlapBox->SetRelativeLocation(FVector(0.f, 0.f, 100.f));

	OverlapBox->OnComponentBeginOverlap.AddDynamic(this, &AHexTile::HandleBeginOverlap);
	OverlapBox->OnComponentEndOverlap.AddDynamic(this, &AHexTile::HandleEndOverlap);

	// 파란 테두리 — 평소엔 숨김. 루트(TileMesh)에 붙어 타일 회전(TileYaw)을 그대로 따른다
	HighlightMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HighlightMesh"));
	HighlightMesh->SetupAttachment(RootComponent);
	HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightMesh->SetCastShadow(false);
	HighlightMesh->SetVisibility(false);

	// 부채꼴 장판과 같은 Translucent+Unlit 머티리얼 — 에셋 할당 없이도 바로 보이게
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightMatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (HighlightMatFinder.Succeeded())
	{
		HighlightMesh->SetMaterial(0, HighlightMatFinder.Object);
	}

	// 활성 오염 장판 기본값 (M_HexTileTinted의 Tint=빨강 인스턴스) — BP에서 따로 안 넣어도 바로 동작하게
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ActivePoopMatFinder(
		TEXT("/Game/LostArk/Level/Echidna/_GENERATED/User/MI_HexTile_PoopActive.MI_HexTile_PoopActive"));
	if (ActivePoopMatFinder.Succeeded())
	{
		ActivePoopMaterial = ActivePoopMatFinder.Object;
	}
}

void AHexTile::InitTile(AHexArena* InOwnerArena, const FIntPoint& InCoord, float TileRadius)
{
	OwnerArena = InOwnerArena;
	Coord = InCoord;
	TileInRadius = TileRadius;

	// 실제 헥스보다 살짝 작게 잡아 이웃 타일 오버랩 박스와 겹치지 않도록 함 (XY만 조정, Z는 생성자 값 유지)
	const float Extent = TileRadius * 0.85f;
	OverlapBox->SetBoxExtent(FVector(Extent, Extent, 150.f));

	SetTileType(EHexTileType::Normal);
}

void AHexTile::SetTileType(EHexTileType NewType)
{
	const bool bTypeActuallyChanged = (TileType != NewType);
	TileType = NewType;

	// 오염이 아니게 되면 활성 상태도 같이 꺼진다 (다시 오염될 땐 비활성부터)
	if (TileType != EHexTileType::PoopZone)
	{
		bPoopActive = false;
	}

	ApplyTileMaterial();
	RefreshBorder();
	RefreshPoopTicking();
	RefreshFlower();

	if (bTypeActuallyChanged)
	{
		OnTileTypeChanged(NewType);

		if (OwnerArena)
		{
			OwnerArena->NotifyTileTypeChanged(Coord);
		}
	}
}

namespace
{
	// 앞/뒤 양쪽 감김으로 추가 — 노멀 방향과 무관하게 항상 보이게 (HexArena·부채꼴 장판과 같은 방식)
	void AddHighlightQuad(TArray<FVector>& Verts, TArray<int32>& Tris,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C, D });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Tris.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}
}

void AHexTile::BuildHighlightMesh()
{
	if (TileInRadius <= 0.f) return;

	// 타일 액터는 TileYaw(30도)만큼 돌아 있고 메시 자체는 Flat-top이라, 로컬 공간에서 꼭짓점은 0/60/120...도에 있다
	const float CornerRadius = TileInRadius * 2.f / FMath::Sqrt(3.f);
	const float InnerRadius = FMath::Max(0.f, CornerRadius - HighlightWidth);

	// 타일 윗면 높이 — 메시 피벗 위치를 몰라도 되게 실제 바운즈 꼭대기를 로컬 Z로 환산
	const float TopZ = TileMesh->Bounds.Origin.Z + TileMesh->Bounds.BoxExtent.Z - GetActorLocation().Z + HighlightZOffset;

	TArray<FVector> Verts;
	TArray<int32> Tris;

	for (int32 i = 0; i < 6; ++i)
	{
		const float AngA = FMath::DegreesToRadians(60.f * i);
		const float AngB = FMath::DegreesToRadians(60.f * (i + 1));
		const FVector DirA(FMath::Cos(AngA), FMath::Sin(AngA), 0.f);
		const FVector DirB(FMath::Cos(AngB), FMath::Sin(AngB), 0.f);

		const FVector OuterA = DirA * CornerRadius + FVector(0.f, 0.f, TopZ);
		const FVector OuterB = DirB * CornerRadius + FVector(0.f, 0.f, TopZ);
		const FVector InnerA = DirA * InnerRadius + FVector(0.f, 0.f, TopZ);
		const FVector InnerB = DirB * InnerRadius + FVector(0.f, 0.f, TopZ);

		// 바닥 링
		AddHighlightQuad(Verts, Tris, InnerA, OuterA, OuterB, InnerB);

		// 외곽선을 따라 선 낮은 띠 — 탑다운 카메라에서 "빛나는 테두리"로 보이게
		if (HighlightWallHeight > 0.f)
		{
			const FVector Up(0.f, 0.f, HighlightWallHeight);
			AddHighlightQuad(Verts, Tris, OuterA, OuterB, OuterB + Up, OuterA + Up);
		}
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Verts.Num());

	HighlightMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);

	// PMC는 섹션을 만든 뒤에 머티리얼을 다시 넣어줘야 확실히 유지된다 (부채꼴 장판에서 겪은 문제)
	if (!HighlightMID)
	{
		if (UMaterialInterface* BaseMat = HighlightMesh->GetMaterial(0))
		{
			HighlightMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		}
	}
	if (HighlightMID)
	{
		HighlightMesh->SetMaterial(0, HighlightMID);
	}
}

void AHexTile::SetHighlighted(bool bNewHighlighted)
{
	if (bHighlighted == bNewHighlighted) return;
	bHighlighted = bNewHighlighted;

	RefreshBorder();
	OnHighlightChanged(bHighlighted);
}

void AHexTile::SetPoopActive(bool bNewActive)
{
	if (TileType != EHexTileType::PoopZone) bNewActive = false;
	if (bPoopActive == bNewActive) return;
	bPoopActive = bNewActive;

	ApplyTileMaterial();
	RefreshBorder();
	RefreshPoopTicking();
}

void AHexTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 붙어 있는 액터는 타일과 같이 사라지지 않으므로 직접 정리
	if (IsValid(SpawnedFlower))
	{
		SpawnedFlower->Destroy();
	}
	SpawnedFlower = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AHexTile::RefreshFlower()
{
	const bool bWantFlower = TileType == EHexTileType::Flower;

	if (!bWantFlower)
	{
		if (IsValid(SpawnedFlower))
		{
			SpawnedFlower->Destroy();
		}
		SpawnedFlower = nullptr;
		return;
	}

	if (IsValid(SpawnedFlower)) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 타일 윗면 — 메시 피벗과 무관하게 바운즈 꼭대기 기준 (테두리와 같은 방식)
	FVector Top = GetActorLocation();
	Top.Z = TileMesh->Bounds.Origin.Z + TileMesh->Bounds.BoxExtent.Z;

	UClass* ClassToSpawn = FlowerActorClass ? FlowerActorClass.Get() : AEchidnaBigFlowerActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedFlower = World->SpawnActor<AEchidnaBigFlowerActor>(ClassToSpawn, Top, FRotator::ZeroRotator, SpawnParams);
	if (SpawnedFlower)
	{
		SpawnedFlower->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AHexTile::ApplyTileMaterial()
{
	UMaterialInterface* MaterialForType = NormalMaterial;
	if (bDangerFlash && ActivePoopMaterial)
	{
		MaterialForType = ActivePoopMaterial;
	}
	else if (TileType == EHexTileType::PoopZone)
	{
		MaterialForType = (bPoopActive && ActivePoopMaterial) ? ActivePoopMaterial.Get() : PoopMaterial.Get();
	}
	else if (TileType == EHexTileType::Flower)
	{
		// 꽃 타일 바닥은 항상 오염 장판(비활성)과 똑같이 — 꽃은 위에 얹힌 AEchidnaBigFlowerActor가 그린다.
		// 예전엔 별도 FlowerMaterial(불 텍스처)이 있었는데, BP 값을 비워도 PIE에서 옛 값이 남아 불이 나와서 필드 자체를 없앰
		MaterialForType = PoopMaterial;
	}

	if (MaterialForType)
	{
		TileMesh->SetMaterial(0, MaterialForType);
	}
}

void AHexTile::RefreshBorder()
{
	const bool bShowRed = bDangerFlash || IsPoopActive();
	const bool bShow = bShowRed || bLinkHighlighted || bHighlighted;

	if (bShow)
	{
		BuildHighlightMesh();
		if (HighlightMID)
		{
			const FLinearColor Color = bShowRed ? ActivePoopBorderColor : (bLinkHighlighted ? LinkBorderColor : HighlightColor);
			HighlightMID->SetVectorParameterValue(HighlightColorParameterName, Color);
		}
	}
	HighlightMesh->SetVisibility(bShow);
}

void AHexTile::SetLinkHighlighted(bool bNewLinkHighlighted)
{
	if (bLinkHighlighted == bNewLinkHighlighted) return;
	bLinkHighlighted = bNewLinkHighlighted;
	RefreshBorder();
}

void AHexTile::SetDangerFlash(bool bNewDangerFlash)
{
	if (bDangerFlash == bNewDangerFlash) return;
	bDangerFlash = bNewDangerFlash;
	ApplyTileMaterial();
	RefreshBorder();
}

void AHexTile::RefreshPoopTicking()
{
	if (IsPoopActive() && OverlappingCharacter.IsValid())
	{
		if (!GetWorldTimerManager().IsTimerActive(PoopTickTimerHandle))
		{
			ApplyPoopTick();
			GetWorldTimerManager().SetTimer(PoopTickTimerHandle, this, &AHexTile::ApplyPoopTick, PoopTickInterval, true);
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(PoopTickTimerHandle);
	}
}

void AHexTile::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 타입과 무관하게 누가 서 있는지는 항상 기억해둔다 — 서 있는 도중에 장판이 활성화되면 그 즉시 틱이 시작돼야 하므로
	ALoACharacter* Character = Cast<ALoACharacter>(OtherActor);
	if (!Character) return;

	OverlappingCharacter = Character;
	RefreshPoopTicking();
}

void AHexTile::HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor != OverlappingCharacter.Get()) return;

	OverlappingCharacter = nullptr;
	RefreshPoopTicking();
}

void AHexTile::ApplyPoopTick()
{
	ALoACharacter* Character = OverlappingCharacter.Get();
	if (!Character || !IsPoopActive()) return;

	Character->AddCharmGauge(PoopCharmGaugePerTick);
	Character->ReceiveDamage(PoopTickDamage);
}
