#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaClashPlatformActor.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 0줄 발악 페이즈의 격돌 발판 — AEchidnaFinalPhaseActor가 스폰한다.
 * 타일 위에 얇은 원판으로 솟아오르며 맥동하고, 플레이어가 InteractRadius 안에 서면 [G] 격돌 표시가 뜬다.
 * G 입력은 ALoAPlayerController::OnInteractKey가 받아 TryInteract로 넘긴다(NPC보다 먼저 확인).
 * 순수 비주얼이라 충돌 없음 — 클릭 이동·패턴 판정을 막지 않는다
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaClashPlatformActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaClashPlatformActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 플레이어가 발판 위(수평 거리 InteractRadius 이내)에 서 있는지 */
	bool IsPawnInRange(const APawn* Pawn) const;

	/** 컨트롤러가 G 입력 때 부른다 — 범위 안이고 아직 상호작용 전이면 OnInteracted를 쏘고 true */
	bool TryInteract(APawn* Pawn);

	/** 격돌이 시작되면 진행자가 끈다 — 표시를 숨기고 더 이상 G를 받지 않는다 */
	void SetInteractable(bool bInInteractable);
	bool IsInteractable() const { return bInteractable; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClashPlatformInteracted, APawn* /*Pawn*/);
	FOnClashPlatformInteracted OnInteracted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClashPlatform")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClashPlatform")
	TObjectPtr<UStaticMeshComponent> DiscMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClashPlatform")
	TObjectPtr<UWidgetComponent> PromptComp;

	// 이 수평 거리(cm) 안이면 발판 위로 본다 — 원판 반지름보다 살짝 크게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform")
	float InteractRadius = 170.f;

	// 원판 반지름·두께 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	float DiscRadius = 140.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	float DiscThickness = 8.f;

	// 스폰 후 0 → 원래 크기로 솟아오르는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	float RiseDuration = 0.5f;

	// M_MirrorLaser "Base Color" (HDR — 1보다 크면 블룸). 맥동은 PulseMin~1배 밝기 사이를 오간다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	TObjectPtr<UMaterialInterface> DiscMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	FLinearColor DiscColor = FLinearColor(6.f, 4.2f, 1.2f, 0.75f);

	// 플레이어가 위에 서 있을 때 — 더 밝게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	FLinearColor DiscInRangeColor = FLinearColor(12.f, 9.f, 3.f, 0.9f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	float PulseSpeed = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	float PulseMin = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform|Visual")
	FName ColorParameterName = TEXT("Base Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashPlatform")
	FText PromptActionText = NSLOCTEXT("FinalPhase", "ClashPrompt", "격돌");

private:
	void ApplyDiscScale(float Alpha);
	void SetPromptVisible(bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiscMID;

	float Elapsed = 0.f;
	bool bInteractable = true;
	bool bPromptVisible = false;
	bool bPlayerInRange = false;
};
