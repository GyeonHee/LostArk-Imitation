#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexTile.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class AHexArena;
class ALoACharacter;
class AEchidnaBigFlowerActor;

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

	// 비활성 오염 장판 (레이드 시작 시 깔리는 상태 — 밟아도 아무 일 없음, 살짝 핑크빛)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Visual")
	TObjectPtr<UMaterialInterface> PoopMaterial;

	// 활성 오염 장판 (특정 패턴이 켠 뒤 — 밟으면 매혹+데미지, 빨간색 + 빨간 테두리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Visual")
	TObjectPtr<UMaterialInterface> ActivePoopMaterial;

	// Flower 타입이 되면 타일 위에 스폰하는 큰 꽃 — 비워두면 네이티브 AEchidnaBigFlowerActor
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Visual")
	TSubclassOf<AEchidnaBigFlowerActor> FlowerActorClass;

	// 오염 장판 활성 여부 — PoopZone이어도 false면 밟아도 틱 효과가 없다(비활성 = 레이드 시작 상태).
	// 활성화는 패턴이 SetPoopActive(true) 또는 AHexArena::SetAllPoopTilesActive(true)로 켠다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|PoopZone")
	bool bPoopActive = false;

	// 활성 오염 장판 위에 서 있는 동안의 틱 효과
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|PoopZone")
	float PoopTickInterval = 1.f;

	// 초당 플레이어 최대체력(10만)의 2% — 밟고 버티면 아프지만 즉사는 아닌 수준
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|PoopZone")
	float PoopTickDamage = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|PoopZone")
	int32 PoopCharmGaugePerTick = 1;

	// ── 테두리 표시 ──────────────────────────────────────────
	// 타일 실제 헥스 외곽선을 따라 ProceduralMesh로 링(바닥) + 낮은 벽(빛나는 띠)을 그린다. 두 가지 용도:
	//  - 파란 테두리(bHighlighted): 레이드 시작 시 2칸 — 반정산 패턴용 정보. 타일 타입과 독립
	//  - 빨간 테두리: 활성 오염 장판이면 자동. 파란 테두리보다 우선
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|Highlight")
	TObjectPtr<UProceduralMeshComponent> HighlightMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|Highlight")
	bool bHighlighted = false;

	// Translucent+Unlit인 M_MirrorLaser의 "Base Color"에 들어가므로 1보다 큰 값이 곧 발광(블룸) 세기
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	FLinearColor HighlightColor = FLinearColor(0.15f, 0.35f, 6.f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	FLinearColor ActivePoopBorderColor = FLinearColor(6.f, 0.2f, 0.1f, 0.9f);

	// 거울잇기 — 빛 덩어리를 받은 플레이어의 타일 (노란 테두리)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|Highlight")
	bool bLinkHighlighted = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	FLinearColor LinkBorderColor = FLinearColor(6.f, 4.5f, 0.3f, 0.9f);

	// 거울잇기 실패 — 전 타일이 잠깐 빨간 장판처럼 보임 (바닥 ActivePoopMaterial + 빨간 테두리). 타일 타입은 안 바뀜
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|Highlight")
	bool bDangerFlash = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	FName HighlightColorParameterName = TEXT("Base Color");

	// 테두리 두께 (헥스 외곽선에서 안쪽으로, cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	float HighlightWidth = 25.f;

	// 외곽선을 따라 세우는 빛나는 띠의 높이 (cm, 0이면 바닥 링만)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	float HighlightWallHeight = 30.f;

	// 타일 윗면에서 띄우는 높이 (Z-fighting 방지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tile|Highlight")
	float HighlightZOffset = 3.f;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// AHexArena가 스폰 직후 호출 — 좌표/오너/오버랩 박스 크기를 확정한다
	void InitTile(AHexArena* InOwnerArena, const FIntPoint& InCoord, float TileRadius);

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileType(EHexTileType NewType);

	UFUNCTION(BlueprintCallable, Category = "Tile|Highlight")
	void SetHighlighted(bool bNewHighlighted);

	UFUNCTION(BlueprintCallable, Category = "Tile|Highlight")
	void SetLinkHighlighted(bool bNewLinkHighlighted);

	UFUNCTION(BlueprintCallable, Category = "Tile|Highlight")
	void SetDangerFlash(bool bNewDangerFlash);

	// PoopZone일 때만 의미 있음 — 켜면 빨갛게 바뀌고 그 위에 서 있던 캐릭터에게 즉시 틱이 들어가기 시작한다
	UFUNCTION(BlueprintCallable, Category = "Tile|PoopZone")
	void SetPoopActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "Tile|PoopZone")
	bool IsPoopActive() const { return TileType == EHexTileType::PoopZone && bPoopActive; }

	UFUNCTION(BlueprintPure, Category = "Tile|Highlight")
	bool IsHighlighted() const { return bHighlighted; }

	// 연출 훅 — 파티클 등을 BP에서 더 얹고 싶을 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Tile|Highlight")
	void OnHighlightChanged(bool bNewHighlighted);

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

	void BuildHighlightMesh();

	// 타입·활성 상태에 맞는 타일 머티리얼 적용
	void ApplyTileMaterial();

	// 테두리 표시/색 결정 — 위험 점멸·활성 오염(빨강) > 거울잇기(노랑) > 파란 테두리 > 숨김
	void RefreshBorder();

	// 활성 오염 장판이고 캐릭터가 위에 있으면 틱 시작, 아니면 정지
	void RefreshPoopTicking();

private:
	FTimerHandle PoopTickTimerHandle;

	// InitTile에서 받는 헥스 내접원 반지름(= 변까지 거리). 꼭짓점 반지름은 이 값 * 2/√3
	float TileInRadius = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighlightMID;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaBigFlowerActor> SpawnedFlower;

	// Flower 타입이면 꽃 스폰, 아니면 제거
	void RefreshFlower();
	TWeakObjectPtr<ALoACharacter> OverlappingCharacter;
};
