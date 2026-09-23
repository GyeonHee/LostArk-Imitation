#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScreenFogWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 화면 전체를 덮는 연기 — 랜잡 패턴(광폭화 7분 40초)의 핑크 연기.
 * WBP 없이 C++만으로 위젯 트리(Overlay + 연기 Image 2장)를 만든다 — 에디터에서 BindWidget/바인딩을 걸 필요가 없게.
 * 두 장을 서로 다른 방향·속도로 천천히 흘려서 연기가 움직이는 것처럼 보이게 한다.
 * 불투명도는 SetFogOpacity로 조절하고, 페이드는 ALoAPlayerController가 Tick에서 보간한다.
 */
UCLASS()
class LOA_API UScreenFogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetFogOpacity(float Opacity);

	// 연기 텍스처 — 비어 있으면 핑크 단색으로 대체
	UPROPERTY(EditDefaultsOnly, Category = "Fog")
	TSoftObjectPtr<UTexture2D> FogTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/T_PinkFog.T_PinkFog")));

	UPROPERTY(EditDefaultsOnly, Category = "Fog")
	FLinearColor FallbackColor = FLinearColor(0.75f, 0.25f, 0.45f, 0.8f);

	// 연기가 흘러가는 폭 (px) — 텍스처 가운데 구멍(플레이어 주변만 보임)이 캐릭터에서 벗어나지 않게 작게
	UPROPERTY(EditDefaultsOnly, Category = "Fog")
	float DriftAmplitude = 25.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> FogLayerA;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FogLayerB;

	float Time = 0.f;
};
