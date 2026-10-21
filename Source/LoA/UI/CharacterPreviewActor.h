#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterPreviewActor.generated.h"

class ACharacter;
class UAnimationAsset;
class UPointLightComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

/**
 * 캐릭터 정보 창(P) 가운데의 3D 캐릭터 미리보기.
 * 맵 한참 위(PreviewLocation)에 플레이어 메시 복사본을 세워 Idle을 틀고, SceneCapture로 렌더 타깃에 찍어 UImage에 띄운다.
 *  - 실제 플레이어를 찍지 않는 이유: 걷기·넉다운 같은 게임 상태와 무관하게 항상 서 있는 모습이어야 해서
 *  - 조명은 자체 포인트 라이트 3개 + 라이팅 채널 1 — 메시와 라이트만 채널 1이라 레벨의 태양·다른 액터와 서로 안 섞인다.
 *    스카이라이트는 채널을 무시해서 캡처 ShowFlag로 끈다 → 어느 맵에서 열어도 밝기가 같다
 *  - 노출은 수동 고정 — 자동 노출이면 거의 빈 화면이라 캐릭터가 하얗게 날아간다
 *  - 메시만 찍어서(ShowOnly) 나머지는 검은 배경. 반짝임 방지 3종: 반사광 0(하얀 점), FinalToneCurveHDR+16비트(8비트
 *    디더링), TAA 켜둠(선·윤곽 지글거림). SSAO 등 화면 공간 효과는 끔
 *  - 창이 열려 있을 때만 매 프레임 캡처(SetPreviewActive)
 * 밝기 조절: 콘솔 LoA.Preview.Exposure(노출 보정 EV), LoA.Preview.Light(라이트 배율) → 창을 다시 열면 반영
 */
UCLASS()
class LOA_API ACharacterPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ACharacterPreviewActor();

	/** 플레이어 메시·머티리얼을 복사하고 Idle 재생 */
	void SetupFromCharacter(const ACharacter* Source);

	/** 창이 열리고 닫힐 때 — 열려 있는 동안만 매 프레임 캡처 */
	void SetPreviewActive(bool bActive);

	/** 미리보기 영역을 끌어 돌릴 때 (도 단위) */
	void AddPreviewYaw(float DeltaYaw);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	// 맵 위 아무것도 없는 곳 — 메인 카메라에 안 걸리고 KillZ와도 멀게 위쪽
	static constexpr float PreviewHeight = 60000.f;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyTuning();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> CaptureComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> RimLight;

	UPROPERTY()
	TObjectPtr<UAnimationAsset> IdleAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	// 라이트 기본 세기(cd) — LoA.Preview.Light 배율을 곱한다.
	// 수동 노출(EV 0) 기준이라 생각보다 훨씬 세야 한다: 300/250/250은 완전히 검었고, 그 50배를 PIE 캡처로 확인해 정함
	float KeyIntensity = 15000.f;
	float FillIntensity = 12500.f;
	float RimIntensity = 12500.f;
};
