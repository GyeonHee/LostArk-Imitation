#include "UI/ScreenFogWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UScreenFogWidget::RebuildWidget()
{
	// WBP가 없으니 트리를 여기서 직접 만든다 (Super가 RootWidget으로 Slate 위젯을 만들기 전에)
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FogRoot"));
		WidgetTree->RootWidget = Root;

		UTexture2D* Texture = FogTexture.LoadSynchronous();

		auto MakeLayer = [&](const TCHAR* Name, float Scale) -> UImage*
		{
			UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
			if (Texture)
			{
				Image->SetBrushFromTexture(Texture, false);
			}
			else
			{
				Image->SetColorAndOpacity(FallbackColor);
			}
			if (UOverlaySlot* OverlaySlot = Root->AddChildToOverlay(Image))
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			}
			// 흘러갈 때 화면 가장자리가 비지 않도록 살짝 크게
			Image->SetRenderScale(FVector2D(Scale));
			Image->SetRenderTransformPivot(FVector2D(0.5f));
			return Image;
		};

		// 텍스처 가운데 구멍 크기가 곧 "보이는 범위" — 스케일을 크게 주면 구멍도 커지므로 흘러갈 여유만큼만
		FogLayerA = MakeLayer(TEXT("FogLayerA"), 1.06f);
		FogLayerB = MakeLayer(TEXT("FogLayerB"), 1.1f);
		if (FogLayerB)
		{
			// 두 번째 장은 좌우로 뒤집어 무늬가 겹치지 않게 (구멍은 가운데라 그대로 겹침) + 조금 옅게
			FogLayerB->SetRenderScale(FVector2D(-1.1f, 1.1f));
			FogLayerB->SetRenderOpacity(0.5f);
		}
	}

	// 클릭 이동 게임이라 연기가 마우스 입력을 먹으면 안 된다 (HUD 위젯 규칙과 동일)
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UScreenFogWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;

	if (FogLayerA)
	{
		FogLayerA->SetRenderTranslation(FVector2D(FMath::Sin(Time * 0.21f), FMath::Cos(Time * 0.17f)) * DriftAmplitude);
	}
	if (FogLayerB)
	{
		FogLayerB->SetRenderTranslation(FVector2D(FMath::Cos(Time * 0.13f), FMath::Sin(Time * 0.19f)) * -DriftAmplitude);
	}
}

void UScreenFogWidget::SetFogOpacity(float Opacity)
{
	SetRenderOpacity(FMath::Clamp(Opacity, 0.f, 1.f));
}
