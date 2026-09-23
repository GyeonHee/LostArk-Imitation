#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaBigFlowerActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 오염 장판에 둘러싸인 타일(Flower)에 피는 큰 꽃 — 순수 비주얼.
 * 에셋 없이 ProceduralMesh로 꽃잎 여러 겹을 만든다(바깥 겹일수록 크고 눕고, 안쪽일수록 작고 선다).
 * 스폰되면 BloomDuration 동안 0에서 커지며 피고, 이후 천천히 돈다.
 * 매혹 오라(주변 1칸 1초마다 매혹 스택)는 이 액터가 아니라 AHexArena가 담당한다.
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaBigFlowerActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaBigFlowerActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flower")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flower")
	TObjectPtr<UProceduralMeshComponent> FlowerMesh;

	// 가장 바깥 꽃잎 길이 (cm) — 타일 내접원 반지름(약 260)보다 조금 작게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape")
	float PetalLength = 230.f;

	// 꽃잎 폭 = 길이 × 이 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape")
	float PetalWidthRatio = 0.55f;

	// 꽃잎이 오목하게 말려 올라가는 정도 (cm, 꽃잎 끝 기준)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape")
	float PetalCup = 40.f;

	// 겹 수와 바깥 겹 꽃잎 수 (안쪽으로 갈수록 1장씩 줄어듦)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape", meta = (ClampMin = "1", ClampMax = "5"))
	int32 LayerCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape", meta = (ClampMin = "3"))
	int32 OuterPetalCount = 7;

	// 바닥에서 꽃이 떠 있는 높이 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape")
	float HoverHeight = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Motion")
	float BloomDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Motion")
	float SpinSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Motion")
	float BobAmplitude = 8.f;

	// 겹별 색 (바깥 → 안쪽) + 꽃술 — M_MirrorLaser "Base Color", 1보다 크면 발광
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Visual")
	TArray<FLinearColor> LayerColors;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Visual")
	FLinearColor CenterColor = FLinearColor(3.5f, 2.6f, 1.2f, 0.95f);

	// 꽃 아래 바닥에 펼쳐지는 잎 (0이면 없음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Shape")
	int32 LeafCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Visual")
	FLinearColor LeafColor = FLinearColor(0.25f, 1.1f, 0.3f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flower|Visual")
	FName ColorParameterName = TEXT("Base Color");

private:
	void BuildFlower();
	void BuildLayer(int32 LayerIndex);
	void BuildCenter();
	void BuildLeaves();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SectionMIDs;

	float Age = 0.f;
};
