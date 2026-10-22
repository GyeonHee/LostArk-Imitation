#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableNPCActor.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimationAsset;

/**
 * 대기 지역 NPC 공통 — 상점·재련 NPC가 상속한다. 지금은 캐릭터 에셋이 없어 엔진 기본 마네킹(SKM_Quinn_Simple + MM_Idle).
 * 플레이어가 InteractRadius 안에 들어오면 옆에 [G] 대화하기가 뜨고, 그 상태거나 마우스를 올리면 몸 테두리가 빛난다(오버레이 머티리얼).
 * 거리 안에서 G 또는 NPC를 우클릭하면 메뉴가 열린다 — 판정·공통 연출(붙잡힘·카메라·HUD 숨김·하단 대화창)은
 * ALoAPlayerController(UpdateNPCInteraction / OpenNPCMenu)가 하고, 어떤 창을 띄울지는 자식 클래스 종류로 고른다
 */
UCLASS(Abstract)
class LOA_API AInteractableNPCActor : public AActor
{
	GENERATED_BODY()

public:
	AInteractableNPCActor();

	// ── 표시 ──
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText NPCTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText NPCName;

	// 메뉴를 열었을 때 화면 아래 대화창에 나오는 대사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText GreetingLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	float NameHeight = 215.f;

	// ── 상호작용 ──
	// 이 거리(수평, cm) 안이면 [G] 대화하기 + 테두리 발광, G·우클릭으로 메뉴 열기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Interact", meta = (ClampMin = "50"))
	float InteractRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Interact")
	TObjectPtr<UMaterialInterface> HighlightMaterial;

	// 테두리 색 (HDR — 1보다 크면 블룸)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Interact")
	FLinearColor HighlightColor = FLinearColor(1.f, 0.78f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Interact")
	float HighlightIntensity = 12.f;

	// 메뉴를 연 동안의 카메라 — 가까이 + 낮은 각도. Yaw는 NPC가 화면 오른쪽에 오도록 컨트롤러가 계산
	// (예전 AShopNPCActor::ShopCameraArmLength/Pitch — DefaultEngine.ini CoreRedirects로 BP 값 유지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Camera")
	float MenuCameraArmLength = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Camera")
	float MenuCameraPitch = -30.f;

	// 미니맵 아이콘 — 비우면 미니맵에 노란 점 (상점·재련 NPC는 생성자에서 기본값)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Minimap")
	TSoftObjectPtr<class UTexture2D> MinimapIcon;

	// ── 컨트롤러가 부름 ──
	void SetHighlighted(bool bHighlighted);
	void SetPromptVisible(bool bVisible);
	bool IsPawnInRange(const APawn* Pawn) const;

	USkeletalMeshComponent* GetMesh() const { return MeshComp; }

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "NPC")
	TObjectPtr<UCapsuleComponent> CapsuleComp;

	UPROPERTY(VisibleAnywhere, Category = "NPC")
	TObjectPtr<USkeletalMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "NPC")
	TObjectPtr<UWidgetComponent> NameComp;

	UPROPERTY(VisibleAnywhere, Category = "NPC")
	TObjectPtr<UWidgetComponent> PromptComp;

	UPROPERTY(EditAnywhere, Category = "NPC")
	TObjectPtr<UAnimationAsset> IdleAnimation;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighlightMID;

	bool bHighlighted = false;
	bool bPromptVisible = false;
};
