#include "UI/IdentityGaugeWidget.h"
#include "Character/Sorceress.h"
#include "Character/SorceressIdentityComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	TAutoConsoleVariable<float> CVarIdentityOffsetY(
		TEXT("LoA.Identity.OffsetY"), 0.f,
		TEXT("소서리스 아이덴티티 틀을 화면 아래에서 띄울 거리 (슬레이트 단위)"));
	TAutoConsoleVariable<float> CVarIdentityScale(
		TEXT("LoA.Identity.Scale"), 1.f,
		TEXT("소서리스 아이덴티티 틀 크기 배율 (1 = 틀 텍스처 226×175)"));

	// ── 틀 텍스처(T_IdentityArt 226×175 — T_IdentityGlow도 같은 크기·위치) 안의 좌표 ──
	const FVector2f IdFrame_Size(226.f, 175.f);
	const FVector2f IdFrame_ZCenter(48.5f, 65.f);
	const FVector2f IdFrame_XCenter(183.f, 65.f);
	constexpr float IdFrame_IconRadius = 20.5f;
	const FVector2f IdFrame_BarMin(59.f, 140.f);    // 게이지 바 안쪽 (비워 둔 곳)
	const FVector2f IdFrame_BarSize(112.f, 11.f);

	constexpr float IdRing_DegreesPerSecond = 300.f;  // 해방 빛 회전 속도
	constexpr float IdRing_TailDegrees = 110.f;       // 빛 꼬리 길이
	constexpr int32 IdRing_Segments = 24;

	/** 볼록 다각형을 꼭짓점 색 그대로 (0번 기준 부채꼴 삼각분할) */
	void IdPaint_Quad(TArray<FSlateVertex>& Verts, TArray<SlateIndex>& Indices, const FSlateRenderTransform& Xform,
		const FVector2f (&P)[4], const FColor (&C)[4])
	{
		const SlateIndex Base = static_cast<SlateIndex>(Verts.Num());
		for (int32 i = 0; i < 4; ++i)
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, P[i], FVector2f(0.5f, 0.5f), C[i]));
		}
		Indices.Append({ Base, static_cast<SlateIndex>(Base + 1), static_cast<SlateIndex>(Base + 2),
			Base, static_cast<SlateIndex>(Base + 2), static_cast<SlateIndex>(Base + 3) });
	}

	/** 채운 원 (아이콘 어둡게 덮기) */
	void IdPaint_Disc(TArray<FSlateVertex>& Verts, TArray<SlateIndex>& Indices, const FSlateRenderTransform& Xform,
		FVector2f Center, float Radius, const FColor& Color)
	{
		const SlateIndex Base = static_cast<SlateIndex>(Verts.Num());
		Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, Center, FVector2f(0.5f, 0.5f), Color));
		constexpr int32 N = 32;
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = 2.f * PI * i / N;
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, Center + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius, FVector2f(0.5f, 0.5f), Color));
		}
		for (int32 i = 1; i <= N; ++i)
		{
			Indices.Append({ Base, static_cast<SlateIndex>(Base + i), static_cast<SlateIndex>(Base + i + 1) });
		}
	}

	/** 원 테두리를 따라 도는 빛 한 줄기 — 머리(HeadDeg)가 가장 밝고 꼬리 쪽(+각도)으로 사라진다. 화면은 y가 아래라 각도가 줄면 반시계 */
	void IdPaint_Comet(TArray<FSlateVertex>& Verts, TArray<SlateIndex>& Indices, const FSlateRenderTransform& Xform,
		FVector2f Center, float Radius, float HalfWidth, float HeadDeg, FLinearColor HeadColor, FLinearColor TailColor)
	{
		const SlateIndex Base = static_cast<SlateIndex>(Verts.Num());
		for (int32 i = 0; i <= IdRing_Segments; ++i)
		{
			const float T = static_cast<float>(i) / IdRing_Segments;           // 0 = 머리, 1 = 꼬리 끝
			const float A = FMath::DegreesToRadians(HeadDeg + IdRing_TailDegrees * T);
			const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
			FLinearColor Color = FLinearColor::LerpUsingHSV(HeadColor, TailColor, T);
			Color.A = FMath::Lerp(HeadColor.A, 0.f, T * T);
			const float W = HalfWidth * (1.f - 0.5f * T);
			const FColor C = Color.ToFColor(true);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, Center + Dir * (Radius - W), FVector2f(0.5f, 0.5f), C));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, Center + Dir * (Radius + W), FVector2f(0.5f, 0.5f), C));
		}
		for (int32 i = 0; i < IdRing_Segments; ++i)
		{
			const SlateIndex A = static_cast<SlateIndex>(Base + i * 2);
			Indices.Append({ A, static_cast<SlateIndex>(A + 1), static_cast<SlateIndex>(A + 3), A, static_cast<SlateIndex>(A + 3), static_cast<SlateIndex>(A + 2) });
		}
	}
}

TSharedRef<SWidget> UIdentityGaugeWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("IdentityRoot"));
		WidgetTree->RootWidget = Root;

		Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IdentityPanel"));
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
		{
			S->SetAnchors(FAnchors(0.5f, 1.f));
			S->SetAlignment(FVector2D(0.5f, 1.f));
			S->SetAutoSize(true);
		}
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("IdentityLayers"));
		Panel->SetContent(Layers);

		// 불빛(아래) → 틀(위)
		GlowImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IdentityGlow"));
		if (UMaterialInterface* Material = GlowMaterial.LoadSynchronous())
		{
			GlowMID = UMaterialInstanceDynamic::Create(Material, this);
			GlowImage->SetBrushFromMaterial(GlowMID);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Identity] 불빛 머티리얼 없음: %s"), *GlowMaterial.ToString());
		}
		if (UOverlaySlot* S = Layers->AddChildToOverlay(GlowImage))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		FrameImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IdentityFrame"));
		if (UTexture2D* Texture = FrameTexture.LoadSynchronous())
		{
			FrameImage->SetBrushFromTexture(Texture);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Identity] 틀 텍스처 없음: %s"), *FrameTexture.ToString());
		}
		if (UOverlaySlot* S = Layers->AddChildToOverlay(FrameImage))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		LoadedLock = LockTexture.LoadSynchronous();
		if (LoadedLock)
		{
			LockBrush.SetResourceObject(LoadedLock);
			LockBrush.ImageSize = FVector2D(LoadedLock->GetSizeX(), LoadedLock->GetSizeY());
		}
	}

	// 클릭 이동 게임이라 마우스 입력을 먹으면 안 된다 (HUD 위젯 규칙)
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

USorceressIdentityComponent* UIdentityGaugeWidget::FindIdentity() const
{
	const ASorceress* Sorceress = Cast<ASorceress>(GetOwningPlayerPawn());
	return Sorceress ? Sorceress->Identity.Get() : nullptr;
}

float UIdentityGaugeWidget::GetScale() const
{
	return FMath::Max(0.1f, CVarIdentityScale.GetValueOnGameThread());
}

float UIdentityGaugeWidget::GetOffsetY() const
{
	return CVarIdentityOffsetY.GetValueOnGameThread();
}

void UIdentityGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Panel) return;

	const USorceressIdentityComponent* Identity = FindIdentity();
	Panel->SetVisibility(Identity ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	const float Scale = GetScale();
	const float OffsetY = GetOffsetY();
	if (Scale != ShownScale || OffsetY != ShownOffsetY)
	{
		ShownScale = Scale;
		ShownOffsetY = OffsetY;
		Panel->SetWidthOverride(IdFrame_Size.X * Scale);
		Panel->SetHeightOverride(IdFrame_Size.Y * Scale);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Panel->Slot))
		{
			S->SetPosition(FVector2D(0.f, -OffsetY));
		}
	}

	if (!Identity) return;
	const ESorceressMagickState State = Identity->GetState();
	if (State == ESorceressMagickState::Release)
	{
		RingAngle = FMath::Fmod(RingAngle - IdRing_DegreesPerSecond * InDeltaTime, 360.f);
	}

	// 불빛 — 평소엔 그대로(불투명), 가득 차면 밝게 맥동, 강화는 살짝 보라, 해방은 흰빛 + 보랏빛이 노이즈를 따라 섞인다
	const float Ratio = Identity->GetGauge() / LoASorceressIdentity::MaxGauge;
	const bool bFull = State == ESorceressMagickState::None && Ratio >= 1.f - KINDA_SMALL_NUMBER;
	FullPulseTime = bFull ? FullPulseTime + InDeltaTime : 0.f;
	float Target = 1.f;
	FLinearColor Tint(1.f, 1.f, 1.f, 1.f);
	FLinearColor Tint2 = Tint;
	if (bFull)
	{
		Target = 1.2f + 0.2f * FMath::Sin(FullPulseTime * 5.f);
	}
	else if (State == ESorceressMagickState::Release)
	{
		Target = 1.45f;
		// 파랑과 보라를 반반 섞되 보라가 더 눈에 띄게 — 불빛 텍스처가 파랑(R이 B의 1/4쯤)이라 보라 쪽은 R을 크게 올려야 보라로 보인다
		Tint = FLinearColor(1.1f, 1.25f, 1.7f, 1.f);
		Tint2 = FLinearColor(2.8f, 0.5f, 1.3f, 1.f);
	}
	else if (State == ESorceressMagickState::Amplify)
	{
		Target = 1.1f;
		Tint = FLinearColor(1.05f, 0.9f, 1.25f, 1.f);
		Tint2 = Tint;
	}
	GlowStrength = FMath::FInterpTo(GlowStrength, Target, InDeltaTime, 6.f);
	if (GlowMID)
	{
		GlowMID->SetScalarParameterValue(TEXT("Strength"), GlowStrength);
		GlowMID->SetVectorParameterValue(TEXT("Tint"), Tint);
		GlowMID->SetVectorParameterValue(TEXT("Tint2"), Tint2);
	}
}

int32 UIdentityGaugeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const USorceressIdentityComponent* Identity = FindIdentity();
	if (!Identity || !Panel) return LayerId;

	// 틀 왼쪽 위 = 화면 아래 가운데 기준 (캔버스 슬롯과 같은 배치)
	const float Scale = GetScale();
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2f Size(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y));
	const FVector2f Origin((Size.X - IdFrame_Size.X * Scale) * 0.5f, Size.Y - GetOffsetY() - IdFrame_Size.Y * Scale);
	auto ToLocal = [&](FVector2f P) { return Origin + P * Scale; };

	const FSlateRenderTransform& Xform = AllottedGeometry.GetAccumulatedRenderTransform();
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	auto ToColor = [Opacity](FLinearColor C) { C.A *= Opacity; return C.ToFColor(true); };

	const ESorceressMagickState State = Identity->GetState();
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;

	// ── 게이지 채움 — 위 밝게 / 가운데 / 아래 어둡게. 해방은 흰빛이 날 만큼 밝다 ──
	float Ratio = 0.f;
	FLinearColor Top, Mid, Bottom;
	if (State == ESorceressMagickState::None)
	{
		Ratio = Identity->GetGauge() / LoASorceressIdentity::MaxGauge;
		const bool bFull = Ratio >= 1.f - KINDA_SMALL_NUMBER;
		Top    = bFull ? FLinearColor(0.70f, 0.82f, 1.f) : FLinearColor(0.45f, 0.60f, 1.f);
		Mid    = bFull ? FLinearColor(0.40f, 0.58f, 1.f) : FLinearColor(0.18f, 0.34f, 1.f);
		Bottom = bFull ? FLinearColor(0.18f, 0.32f, 0.9f) : FLinearColor(0.08f, 0.16f, 0.7f);
	}
	else
	{
		Ratio = Identity->GetStateDuration() > 0.f ? Identity->GetStateRemaining() / Identity->GetStateDuration() : 0.f;
		const bool bRelease = State == ESorceressMagickState::Release;
		Top    = bRelease ? FLinearColor(1.f, 1.f, 1.f) : FLinearColor(0.70f, 0.75f, 1.f);
		Mid    = bRelease ? FLinearColor(0.86f, 0.92f, 1.f) : FLinearColor(0.45f, 0.50f, 1.f);
		Bottom = bRelease ? FLinearColor(0.50f, 0.66f, 1.f) : FLinearColor(0.22f, 0.25f, 0.85f);
	}
	Ratio = FMath::Clamp(Ratio, 0.f, 1.f);
	if (Ratio > 0.f)
	{
		const FVector2f Min = ToLocal(IdFrame_BarMin);
		const float W = IdFrame_BarSize.X * Ratio * Scale;
		const float H = IdFrame_BarSize.Y * Scale;
		const float MidY = Min.Y + H * 0.45f;
		{
			const FVector2f P[4] = { Min, Min + FVector2f(W, 0.f), FVector2f(Min.X + W, MidY), FVector2f(Min.X, MidY) };
			const FColor C[4] = { ToColor(Top), ToColor(Top), ToColor(Mid), ToColor(Mid) };
			IdPaint_Quad(Verts, Indices, Xform, P, C);
		}
		{
			const FVector2f P[4] = { FVector2f(Min.X, MidY), FVector2f(Min.X + W, MidY), Min + FVector2f(W, H), Min + FVector2f(0.f, H) };
			const FColor C[4] = { ToColor(Mid), ToColor(Mid), ToColor(Bottom), ToColor(Bottom) };
			IdPaint_Quad(Verts, Indices, Xform, P, C);
		}
		// 끝머리 빛
		const float TipW = FMath::Min(W, 3.f * Scale);
		const FVector2f TipMin(Min.X + W - TipW, Min.Y);
		const FVector2f P[4] = { TipMin, TipMin + FVector2f(TipW, 0.f), TipMin + FVector2f(TipW, H), TipMin + FVector2f(0.f, H) };
		const FColor Tip = ToColor(FLinearColor(1.f, 1.f, 1.f, 0.85f));
		const FColor C[4] = { ToColor(FLinearColor(1.f, 1.f, 1.f, 0.f)), Tip, Tip, ToColor(FLinearColor(1.f, 1.f, 1.f, 0.f)) };
		IdPaint_Quad(Verts, Indices, Xform, P, C);
	}

	// ── 아이콘 어둡게 — Z: 환류(잠김)·게이지 부족, X: 쿨타임·게이지 부족 ──
	const bool bReleaseBlocked = Identity->IsReleaseBlocked();
	const bool bCanRelease = State == ESorceressMagickState::None && Identity->GetGauge() >= LoASorceressIdentity::MinRelease;
	const float IconR = IdFrame_IconRadius * Scale;
	if (bReleaseBlocked)
	{
		IdPaint_Disc(Verts, Indices, Xform, ToLocal(IdFrame_ZCenter), IconR, ToColor(FLinearColor(0.f, 0.f, 0.f, 0.6f)));
	}
	else if (!bCanRelease && State == ESorceressMagickState::None)
	{
		IdPaint_Disc(Verts, Indices, Xform, ToLocal(IdFrame_ZCenter), IconR, ToColor(FLinearColor(0.f, 0.f, 0.f, 0.4f)));
	}
	const float BlinkCooldown = Identity->GetBlinkCooldownRemaining();
	const bool bBlinkShort = Identity->GetGauge() < Identity->GetBlinkCost() - KINDA_SMALL_NUMBER;
	if (BlinkCooldown > 0.f || bBlinkShort)
	{
		IdPaint_Disc(Verts, Indices, Xform, ToLocal(IdFrame_XCenter), IconR, ToColor(FLinearColor(0.f, 0.f, 0.f, BlinkCooldown > 0.f ? 0.6f : 0.4f)));
	}

	// ── 해방 — Z 테두리를 반시계로 도는, 서로 마주 본 빛 두 줄기 (넓고 옅은 빛 + 가늘고 밝은 심) ──
	if (State == ESorceressMagickState::Release)
	{
		const FVector2f C = ToLocal(IdFrame_ZCenter);
		const float R = (IdFrame_IconRadius + 0.5f) * Scale;
		for (const float Offset : { 0.f, 180.f })
		{
			IdPaint_Comet(Verts, Indices, Xform, C, R, 3.6f * Scale, RingAngle + Offset,
				FLinearColor(0.55f, 0.75f, 1.f, 0.45f * Opacity), FLinearColor(0.25f, 0.4f, 1.f, 0.f));
			IdPaint_Comet(Verts, Indices, Xform, C, R, 1.3f * Scale, RingAngle + Offset,
				FLinearColor(1.f, 1.f, 1.f, Opacity), FLinearColor(0.5f, 0.7f, 1.f, 0.f));
		}
	}

	if (Verts.Num() > 0)
	{
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Verts, Indices, nullptr, 0, 0);
	}

	// ── 100% = 해방 준비 — Z 테두리에 맥동하는 빛 고리 (98%처럼 거의 찬 바와 구분되게) ──
	if (State == ESorceressMagickState::None && !bReleaseBlocked && Identity->GetGauge() >= LoASorceressIdentity::MaxGauge - KINDA_SMALL_NUMBER)
	{
		const FVector2f C = ToLocal(IdFrame_ZCenter);
		const float R = (IdFrame_IconRadius + 0.5f) * Scale;
		TArray<FVector2f> Circle;
		for (int32 i = 0; i <= 48; ++i)
		{
			const float A = 2.f * PI * i / 48.f;
			Circle.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		const float Pulse = 0.55f + 0.45f * FMath::Sin(FullPulseTime * 5.f);
		const FPaintGeometry LineGeometry = AllottedGeometry.ToPaintGeometry();
		FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, LineGeometry, Circle, ESlateDrawEffect::None,
			FLinearColor(0.45f, 0.7f, 1.f, 0.5f * Pulse * Opacity), true, 5.f * Scale);
		FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, LineGeometry, Circle, ESlateDrawEffect::None,
			FLinearColor(0.9f, 0.95f, 1.f, Pulse * Opacity), true, 1.6f * Scale);
	}

	// ── 환류 자물쇠 ──
	if (bReleaseBlocked && LockBrush.GetResourceObject())
	{
		const FVector2f LockSize(18.f * Scale, 18.f * Scale);
		FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
			AllottedGeometry.ToPaintGeometry(LockSize, FSlateLayoutTransform(ToLocal(IdFrame_ZCenter) - LockSize * 0.5f)),
			&LockBrush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}

	// ── 점멸 쿨타임 초 ──
	if (BlinkCooldown > 0.f)
	{
		const FString Text = FString::FromInt(FMath::CeilToInt(BlinkCooldown));
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(12.f * Scale));
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		const FVector2D Measured = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		const FVector2f TextSize(static_cast<float>(Measured.X), static_cast<float>(Measured.Y));
		FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(ToLocal(IdFrame_XCenter) - TextSize * 0.5f)),
			Text, Font, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}
	return LayerId;
}
