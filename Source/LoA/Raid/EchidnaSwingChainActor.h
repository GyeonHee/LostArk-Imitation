#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaSwingChainActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AHexArena;
class ALoACharacter;

UENUM()
enum class EEchidnaSwingChainResult : uint8
{
	Pending,
	Broken,		// 제한 시간 안에 보스 반대편 타일에 도착 — 파훼
	Failed		// 못 끊음 — 매혹 3스택과 같은 매혹 상태
};

/**
 * 그네 패턴의 보스↔플레이어 사슬.
 * 보스 기준 정반대 타일(아레나 중심 대칭, (q,r) → (-q,-r))에 노란 테두리를 켜고, 플레이어가 그 타일을 밟으면 끊어진다.
 * ChainDuration(5초) 안에 못 끊으면 플레이어에게 최대 매혹 스택을 줘서 매혹 상태로 만든다.
 * 사슬 모양은 엔진 기본 Cylinder를 두 끝점 사이로 매 틱 늘려 그린다 — 남은 시간이 줄수록 색이 빨개진다.
 * 광폭화 규칙: BeginPlay에서 CustomTimeDilation, 진행은 전부 Tick 기반
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaSwingChainActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaSwingChainActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Activate(AActor* InBoss, ALoACharacter* InPlayer, AHexArena* InArena, const FIntPoint& InBreakCoord);

	EEchidnaSwingChainResult GetResult() const { return Result; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SwingChain")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SwingChain")
	TObjectPtr<UStaticMeshComponent> ChainMesh;

	// 이 시간(초) 안에 못 끊으면 매혹
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingChain")
	float ChainDuration = 5.f;

	// 사슬 굵기 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingChain|Visual")
	float ChainThickness = 18.f;

	// 두 끝점 높이 — 액터 위치(캡슐 중심) 기준 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingChain|Visual")
	float EndpointZOffset = 20.f;

	// 시작 색 → 시간이 다 되어갈수록 끝 색 (M_MirrorLaser "Base Color", 1보다 크면 발광)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingChain|Visual")
	FLinearColor StartColor = FLinearColor(4.f, 0.8f, 2.5f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingChain|Visual")
	FLinearColor EndColor = FLinearColor(8.f, 0.3f, 0.3f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SwingChain|Visual")
	FName ColorParameterName = TEXT("Base Color");

private:
	void UpdateChainTransform();
	void Resolve(EEchidnaSwingChainResult NewResult);
	void SetBreakTileHighlighted(bool bHighlighted);

	TWeakObjectPtr<AActor> Boss;
	TWeakObjectPtr<ALoACharacter> Player;
	TWeakObjectPtr<AHexArena> Arena;
	FIntPoint BreakCoord = FIntPoint::ZeroValue;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ChainMID;

	EEchidnaSwingChainResult Result = EEchidnaSwingChainResult::Pending;
	bool bActive = false;
	float Elapsed = 0.f;
};
