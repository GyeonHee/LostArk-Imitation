#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetHighlightComponent.generated.h"

class UMeshComponent;
class UPrimitiveComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 적(보스·허수아비)에 붙는 표시 컴포넌트.
 *  - 커서 아웃라인: 컨트롤러가 커서 아래 대상을 골라 SetHovered → 메시 CustomDepth 스텐실 OutlineStencilValue(2)
 *    → 플레이어 카메라의 후처리 M_PP_TargetOutline이 그 가장자리를 주황으로 그림 (r.CustomDepth=3 필요)
 *  - 피격 발광: PlayHitFlash → 오버레이 머티리얼(M_MirrorLaser, "Base Color" 알파 = 불투명도)을 흰색으로 덮었다 서서히 뺌.
 *    다른 오버레이(보스 카운터 발광)가 이미 걸려 있으면 건드리지 않음
 * 대상 메시는 BeginPlay에서 ACharacter면 GetMesh(), 아니면 스켈레탈 → 스태틱 메시. 커서 판정은 그 메시의 보이는 모양(TraceCursorRay)
 */
UCLASS(ClassGroup = (LoA), meta = (BlueprintSpawnableComponent))
class LOA_API UTargetHighlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetHighlightComponent();

	/** 후처리 머티리얼이 보는 스텐실 값 — M_PP_TargetOutline 코드의 s와 같아야 함 (1은 HexArena 프리뷰가 씀) */
	static constexpr int32 OutlineStencilValue = 2;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetHovered(bool bInHovered);
	bool IsHovered() const { return bHovered; }

	UFUNCTION(BlueprintCallable, Category = "Highlight")
	void PlayHitFlash();

	/** 커서 판정 — 카메라에서 나간 커서 광선이 보이는 몸에 닿으면 true + 거리.
	 *  스태틱 메시 = 삼각형(복합 충돌) / 물리 에셋 있는 스켈레탈 = 그 바디 / 없으면(에키드나) 뼈마디 선분에서 CursorBoneRadius 안 */
	bool TraceCursorRay(const FVector& Start, const FVector& End, float& OutDistance) const;

	/** 지금 월드에 있는 모든 대상 (컨트롤러 커서 판정용) */
	static const TArray<TWeakObjectPtr<UTargetHighlightComponent>>& GetAll() { return AllTargets; }

	// 물리 에셋 없는 스켈레탈 메시의 커서 판정 굵기 — 뼈마디 선분에서 이 거리(cm, 메시 배율 곱함) 안이면 몸
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight|Cursor", meta = (ClampMin = "1"))
	float CursorBoneRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight|HitFlash")
	TObjectPtr<UMaterialInterface> HitFlashMaterial;

	// 알파 = 최대 불투명도. HDR 흰색이라 블룸으로 살짝 번짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight|HitFlash")
	FLinearColor HitFlashColor = FLinearColor(2.f, 2.f, 2.f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight|HitFlash")
	FName HitFlashColorParameterName = TEXT("Base Color");

	// 최대 밝기에서 0까지 빠지는 시간 (초) — 연타 중엔 맞을 때마다 다시 최대로
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Highlight|HitFlash", meta = (ClampMin = "0.01"))
	float HitFlashDuration = 0.15f;

private:
	void EndHitFlash();
	bool TraceBones(const FVector& Start, const FVector& End, float& OutDistance) const;

	TWeakObjectPtr<UMeshComponent> TargetMesh;
	bool bTraceBones = false;	// 물리 에셋 없는 스켈레탈 메시

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitFlashMID;

	float HitFlashRemaining = 0.f;
	bool bHovered = false;

	static TArray<TWeakObjectPtr<UTargetHighlightComponent>> AllTargets;
};
