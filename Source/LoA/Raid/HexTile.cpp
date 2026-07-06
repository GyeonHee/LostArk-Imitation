#include "Raid/HexTile.h"
#include "Raid/HexArena.h"
#include "LoACharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"

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
}

void AHexTile::InitTile(AHexArena* InOwnerArena, const FIntPoint& InCoord, float TileRadius)
{
	OwnerArena = InOwnerArena;
	Coord = InCoord;

	// 실제 헥스보다 살짝 작게 잡아 이웃 타일 오버랩 박스와 겹치지 않도록 함 (XY만 조정, Z는 생성자 값 유지)
	const float Extent = TileRadius * 0.85f;
	OverlapBox->SetBoxExtent(FVector(Extent, Extent, 150.f));

	SetTileType(EHexTileType::Normal);
}

void AHexTile::SetTileType(EHexTileType NewType)
{
	const bool bTypeActuallyChanged = (TileType != NewType);
	TileType = NewType;

	UMaterialInterface* MaterialForType = NormalMaterial;
	if (NewType == EHexTileType::PoopZone) MaterialForType = PoopMaterial;
	else if (NewType == EHexTileType::Flower) MaterialForType = FlowerMaterial;

	UE_LOG(LogTemp, Warning, TEXT("[HexTile] (%d,%d) SetTileType -> %d (MaterialForType=%s)"),
		Coord.X, Coord.Y, (int32)NewType, *GetNameSafe(MaterialForType));

	if (MaterialForType)
	{
		TileMesh->SetMaterial(0, MaterialForType);
	}

	if (TileType != EHexTileType::PoopZone)
	{
		GetWorldTimerManager().ClearTimer(PoopTickTimerHandle);
		OverlappingCharacter = nullptr;
	}

	if (bTypeActuallyChanged)
	{
		OnTileTypeChanged(NewType);

		if (OwnerArena)
		{
			OwnerArena->NotifyTileTypeChanged(Coord);
		}
	}
}

void AHexTile::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Warning, TEXT("[HexTile] (%d,%d) BeginOverlap with %s (TileType=%d)"),
		Coord.X, Coord.Y, *GetNameSafe(OtherActor), (int32)TileType);

	if (TileType != EHexTileType::PoopZone) return;

	ALoACharacter* Character = Cast<ALoACharacter>(OtherActor);
	if (!Character) return;

	OverlappingCharacter = Character;
	ApplyPoopTick();
	GetWorldTimerManager().SetTimer(PoopTickTimerHandle, this, &AHexTile::ApplyPoopTick, PoopTickInterval, true);
}

void AHexTile::HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (OtherActor != OverlappingCharacter.Get()) return;

	GetWorldTimerManager().ClearTimer(PoopTickTimerHandle);
	OverlappingCharacter = nullptr;
}

void AHexTile::ApplyPoopTick()
{
	ALoACharacter* Character = OverlappingCharacter.Get();
	if (!Character) return;

	Character->AddCharmGauge(PoopCharmGaugePerTick);
	Character->ReceiveDamage(PoopTickDamage);
}
