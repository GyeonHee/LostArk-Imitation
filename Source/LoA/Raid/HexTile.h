#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexTile.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInterface;
class AHexArena;
class ALoACharacter;

UENUM(BlueprintType)
enum class EHexTileType : uint8
{
	Normal,
	PoopZone,
	Flower
};

// 헥스 아레나의 개별 타일. AHexArena가 타일 좌표(q,r)마다 하나씩 스폰해서 관리한다.
// 오버랩 판정(캐릭터가 밟았는지)과 타입별 비주얼 교체를 타일 스스로 담당하고,
// 인접 타일 조건(예: 똥장판에 둘러싸임) 판정은 전체 그리드를 아는 AHexArena가 수행한다.
UCLASS(Blueprintable)
class LOA_API AHexTile : public AActor
{
	GENERATED_BODY()

public:
	AHexTile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	TObjectPtr<UStaticMeshComponent> TileMesh;

	// 캐릭터가 타일을 밟았는지 감지하는 오버랩 전용 콜리전 (TileMesh는 걷기용 Block 콜리전 유지)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	TObjectPtr<UBoxComponent> OverlapBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	EHexTileType TileType = EHexTileType::Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	TObjectPtr<AHexArena> OwnerArena;

	// 타입별 비주얼 — 메시는 고정(TileMesh 컴포넌트에 직접 할당), 머티리얼만 타입에 따라 교체
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Visual")
	TObjectPtr<UMaterialInterface> NormalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Visual")
	TObjectPtr<UMaterialInterface> PoopMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Visual")
	TObjectPtr<UMaterialInterface> FlowerMaterial;

	// 똥장판 위에 서 있는 동안의 틱 효과
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|PoopZone")
	float PoopTickInterval = 1.f;

	// 초당 플레이어 최대체력(10만)의 2% — 밟고 버티면 아프지만 즉사는 아닌 수준
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|PoopZone")
	float PoopTickDamage = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|PoopZone")
	int32 PoopCharmGaugePerTick = 1;

	// AHexArena가 스폰 직후 호출 — 좌표/오너/오버랩 박스 크기를 확정한다
	void InitTile(AHexArena* InOwnerArena, const FIntPoint& InCoord, float TileRadius);

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileType(EHexTileType NewType);

	EHexTileType GetTileType() const { return TileType; }

	// 타입이 바뀔 때마다 호출 — 블룸/이펙트 등 연출은 BP에서 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "Tile")
	void OnTileTypeChanged(EHexTileType NewType);

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ApplyPoopTick();

private:
	FTimerHandle PoopTickTimerHandle;
	TWeakObjectPtr<ALoACharacter> OverlappingCharacter;
};
