#include "UI/MinimapWidget.h"
#include "Raid/HexArena.h"
#include "Raid/HexTile.h"
#include "Raid/EchidnaBoss.h"
#include "LoACharacter.h"
#include "Lobby/LobbyZoneActor.h"
#include "Lobby/InteractableNPCActor.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** 채워진 볼록 다각형 하나 (0번 꼭짓점 기준 부채꼴 삼각분할) */
	void AddConvexPolygon(TArray<FSlateVertex>& Verts, TArray<SlateIndex>& Indices, const FSlateRenderTransform& Xform,
		const TArray<FVector2f>& Points, const FColor& Color)
	{
		if (Points.Num() < 3) return;
		const SlateIndex Base = static_cast<SlateIndex>(Verts.Num());
		for (const FVector2f& P : Points)
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Xform, P, FVector2f(0.5f, 0.5f), Color));
		}
		for (int32 i = 1; i + 1 < Points.Num(); ++i)
		{
			Indices.Append({ Base, static_cast<SlateIndex>(Base + i), static_cast<SlateIndex>(Base + i + 1) });
		}
	}
}

TSharedRef<SWidget> UMinimapWidget::RebuildWidget()
{
	// WBP가 없으니 빈 루트만 만든다 — 실제 그림은 전부 NativePaint
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MinimapRoot"));
	}

	// 클릭 이동 게임이라 미니맵이 마우스 입력을 먹으면 안 된다 (HUD 위젯 규칙과 동일)
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 아레나 타일은 아레나 BeginPlay에서 스폰되므로, 못 찾았으면 매 틱 다시 시도
	if (!Arena.IsValid())
	{
		Arena = Cast<AHexArena>(UGameplayStatics::GetActorOfClass(this, AHexArena::StaticClass()));
	}
	if (!Boss.IsValid())
	{
		Boss = Cast<AEchidnaBoss>(UGameplayStatics::GetActorOfClass(this, AEchidnaBoss::StaticClass()));
	}

	// 아레나가 없는 맵이면 구역·NPC를 한 번 모은다
	if (!Arena.IsValid() && !bGatheredLobbyActors && GetWorld())
	{
		bGatheredLobbyActors = true;
		for (TActorIterator<ALobbyZoneActor> It(GetWorld()); It; ++It) Zones.Add(*It);
		for (TActorIterator<AInteractableNPCActor> It(GetWorld()); It; ++It)
		{
			NPCs.Add(*It);
			FSlateBrush& Brush = NPCBrushes.AddDefaulted_GetRef();
			if (UTexture2D* Icon = It->MinimapIcon.LoadSynchronous())
			{
				NPCIconTextures.Add(Icon);
				Brush.SetResourceObject(Icon);
				Brush.ImageSize = FVector2D(Icon->GetSizeX(), Icon->GetSizeY());
			}
		}
	}
}

int32 UMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const AHexArena* ArenaPtr = Arena.Get();
	const bool bArenaMode = ArenaPtr && ArenaPtr->TileMap.Num() > 0;
	const APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->PlayerCameraManager || (ArenaPtr && !bArenaMode))
	{
		// 아레나는 있는데 타일이 아직 안 스폰됐으면 기다린다
		return LayerId;
	}

	// ── 미니맵 영역 (위젯은 화면 전체, 그중 우상단 정사각형) ──
	const FVector2f WidgetSize(AllottedGeometry.GetLocalSize());
	const FVector2f MapOrigin(WidgetSize.X - ScreenMargin.X - MapSize, ScreenMargin.Y);
	const FVector2f MapCenter = MapOrigin + FVector2f(MapSize * 0.5f);

	// ── 월드 → 미니맵 변환 ──
	// 위쪽 = 평소 카메라 전방(수평 성분), 오른쪽 = 평소 카메라 오른쪽. NPC 대화·패턴 연출로 카메라가 돌아도
	// 미니맵은 돌지 않는다 — 플레이어의 기본 카메라 각도(오버라이드 전 BeginPlay 값)로 고정
	const ALoACharacter* ViewChar = Cast<ALoACharacter>(PC->GetPawn());
	const float CamYaw = ViewChar ? ViewChar->GetDefaultCameraRotation().Yaw : PC->PlayerCameraManager->GetCameraRotation().Yaw;
	const FVector CamFwd = FRotator(0.f, CamYaw, 0.f).Vector();
	const FVector CamRight = FRotator(0.f, CamYaw + 90.f, 0.f).Vector();

	// 타일 꼭짓점 반지름(외접원) = 내접원 반지름 × 2/√3 — TileSpacing이 flat-to-flat(내접원 지름)
	const float HexRadius = bArenaMode ? ArenaPtr->TileSpacing / FMath::Sqrt(3.f) : 0.f;

	FVector WorldCenter(LobbyWorldCenter.X, LobbyWorldCenter.Y, 0.f);
	float WorldExtent = LobbyHalfExtent;
	if (bArenaMode)
	{
		if (!ArenaPtr->GetTileTopLocation(FIntPoint(0, 0), WorldCenter))
		{
			WorldCenter = ArenaPtr->GetActorLocation();
		}

		// 아레나 전체가 들어가도록 가장 먼 타일 중심 + 꼭짓점 반지름으로 배율 결정
		WorldExtent = HexRadius;
		for (const TPair<FIntPoint, TObjectPtr<AHexTile>>& Pair : ArenaPtr->TileMap)
		{
			if (const AHexTile* Tile = Pair.Value.Get())
			{
				WorldExtent = FMath::Max(WorldExtent, FVector::Dist2D(Tile->GetActorLocation(), WorldCenter) + HexRadius);
			}
		}
	}
	const float Scale = (MapSize * 0.5f - MapPadding) / FMath::Max(WorldExtent, 1.f);

	auto ToMap = [&](const FVector& World) -> FVector2f
	{
		const FVector D = World - WorldCenter;
		return MapCenter + FVector2f(FVector::DotProduct(D, CamRight), -FVector::DotProduct(D, CamFwd)) * Scale;
	};
	auto ToMapDir = [&](const FVector& Dir) -> FVector2f
	{
		return FVector2f(FVector::DotProduct(Dir, CamRight), -FVector::DotProduct(Dir, CamFwd)).GetSafeNormal();
	};

	const FSlateRenderTransform& Xform = AllottedGeometry.GetAccumulatedRenderTransform();
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	auto ToColor = [Opacity](FLinearColor C) { C.A *= Opacity; return C.ToFColor(true); };

	// ── 배경 ──
	FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(MapSize, MapSize), FSlateLayoutTransform(MapOrigin)),
		WhiteBrush, ESlateDrawEffect::None, (bArenaMode ? BackgroundColor : LobbyBackgroundColor) * FLinearColor(1.f, 1.f, 1.f, Opacity));

	if (bArenaMode)
	{
		// ── 타일 ──
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		TArray<FVector2f> Hex;
		Hex.SetNum(6);

		const FColor Color = ToColor(TileColor);
		for (const TPair<FIntPoint, TObjectPtr<AHexTile>>& Pair : ArenaPtr->TileMap)
		{
			const AHexTile* Tile = Pair.Value.Get();
			if (!Tile) continue;

			// 타일 액터가 TileYaw만큼 돌아 있으므로 꼭짓점 각도 = 타일 Yaw + 60도 간격 (HexTile 테두리와 같은 규칙)
			const FVector Center = Tile->GetActorLocation();
			const float BaseYaw = Tile->GetActorRotation().Yaw;
			for (int32 i = 0; i < 6; ++i)
			{
				const FVector Corner = Center + FRotator(0.f, BaseYaw + 60.f * i, 0.f).Vector() * HexRadius * TileInset;
				Hex[i] = ToMap(Corner);
			}
			AddConvexPolygon(Verts, Indices, Xform, Hex, Color);
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Verts, Indices, nullptr, 0, 0);
	}
	else
	{
		// ── 대기 지역: 바닥 → 구역 → NPC ──
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		const float E = LobbyHalfExtent;
		AddConvexPolygon(Verts, Indices, Xform, {
			ToMap(WorldCenter + FVector(E, -E, 0.f)), ToMap(WorldCenter + FVector(E, E, 0.f)),
			ToMap(WorldCenter + FVector(-E, E, 0.f)), ToMap(WorldCenter + FVector(-E, -E, 0.f)) }, ToColor(LobbyFloorColor));

		// 구역 — 실제 테두리와 같은 정N각형. 테두리 색은 HDR(발광용)이라 최댓값으로 나눠 0~1로 맞춘다
		TArray<TPair<TArray<FVector2f>, FLinearColor>> ZoneOutlines;
		for (const TWeakObjectPtr<ALobbyZoneActor>& ZonePtr : Zones)
		{
			const ALobbyZoneActor* Zone = ZonePtr.Get();
			if (!Zone) continue;

			FLinearColor Border = Zone->BorderColor;
			const float MaxC = FMath::Max3(Border.R, Border.G, Border.B);
			if (MaxC > 1.f) { Border.R /= MaxC; Border.G /= MaxC; Border.B /= MaxC; }
			Border.A = 1.f;

			const int32 Sides = FMath::Clamp(Zone->BorderSides, 3, 32);
			const float Yaw0 = Zone->GetActorRotation().Yaw + Zone->BorderAngleOffset;
			TArray<FVector2f> Poly;
			for (int32 i = 0; i < Sides; ++i)
			{
				Poly.Add(ToMap(Zone->GetActorLocation() + FRotator(0.f, Yaw0 + 360.f * i / Sides, 0.f).Vector() * Zone->ZoneRadius));
			}
			AddConvexPolygon(Verts, Indices, Xform, Poly, ToColor(FLinearColor(Border.R, Border.G, Border.B, 0.35f)));
			// Poly[0]을 그대로 넘기면 Add가 배열을 늘리며 원본이 옮겨질 수 있어 엔진이 assert — 복사본으로 닫는다
			const FVector2f FirstPoint = Poly[0];
			Poly.Add(FirstPoint);
			ZoneOutlines.Add({ MoveTemp(Poly), Border });
		}

		// NPC — 아이콘이 없는 NPC만 작은 원 (아이콘은 플레이어 아래 레이어에 따로 그림)
		for (int32 n = 0; n < NPCs.Num(); ++n)
		{
			const AInteractableNPCActor* NPC = NPCs[n].Get();
			if (!NPC || NPCBrushes[n].GetResourceObject()) continue;
			const FVector2f P = ToMap(NPC->GetActorLocation());
			TArray<FVector2f> Dot;
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = 2.f * PI * i / 10.f;
				Dot.Add(P + FVector2f(FMath::Cos(A), FMath::Sin(A)) * NPCIconSize);
			}
			AddConvexPolygon(Verts, Indices, Xform, Dot, ToColor(NPCColor));
		}
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Verts, Indices, nullptr, 0, 0);

		const FPaintGeometry OutlineGeometry = AllottedGeometry.ToPaintGeometry();
		for (const TPair<TArray<FVector2f>, FLinearColor>& Outline : ZoneOutlines)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, OutlineGeometry, Outline.Key, ESlateDrawEffect::None,
				Outline.Value * FLinearColor(1.f, 1.f, 1.f, Opacity), true, 1.5f);
		}

		// NPC 아이콘 — 위치 중심에 세로 NPCTextureIconSize로
		++LayerId;
		for (int32 n = 0; n < NPCs.Num(); ++n)
		{
			const AInteractableNPCActor* NPC = NPCs[n].Get();
			const FSlateBrush& Brush = NPCBrushes[n];
			if (!NPC || !Brush.GetResourceObject()) continue;

			const float Aspect = Brush.ImageSize.Y > 0.f ? Brush.ImageSize.X / Brush.ImageSize.Y : 1.f;
			const FVector2f IconSize(NPCTextureIconSize * Aspect, NPCTextureIconSize);
			const FVector2f P = ToMap(NPC->GetActorLocation()) - IconSize * 0.5f;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
				AllottedGeometry.ToPaintGeometry(IconSize, FSlateLayoutTransform(P)),
				&Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
		}
	}

	const FPaintGeometry LineGeometry = AllottedGeometry.ToPaintGeometry();

	// ── 보스 — 빨간 마름모 (사라져 있는 동안은 숨김) ──
	if (const AEchidnaBoss* BossPtr = Boss.Get(); BossPtr && !BossPtr->IsHidden())
	{
		const FVector2f P = ToMap(BossPtr->GetActorLocation());
		const float S = BossIconSize;
		const TArray<FVector2f> Diamond = { P + FVector2f(0.f, -S), P + FVector2f(S, 0.f), P + FVector2f(0.f, S), P + FVector2f(-S, 0.f) };

		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		AddConvexPolygon(Verts, Indices, Xform, Diamond, ToColor(BossColor));
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Verts, Indices, nullptr, 0, 0);

		TArray<FVector2f> Outline = Diamond;
		Outline.Add(Diamond[0]);
		FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, LineGeometry, Outline, ESlateDrawEffect::None,
			IconOutlineColor * FLinearColor(1.f, 1.f, 1.f, Opacity), true, 1.5f);
	}

	// ── 플레이어 — 바라보는 방향을 가리키는 초록 화살표 ──
	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		const FVector2f P = ToMap(Pawn->GetActorLocation());
		const FVector2f Fwd = ToMapDir(Pawn->GetActorForwardVector());
		const FVector2f Side(-Fwd.Y, Fwd.X);
		const float S = PlayerIconSize;
		const TArray<FVector2f> Arrow = { P + Fwd * S * 1.2f, P - Fwd * S * 0.8f + Side * S * 0.8f, P - Fwd * S * 0.4f, P - Fwd * S * 0.8f - Side * S * 0.8f };

		// 화살표는 오목(꼬리 가운데가 들어감)이라 볼록한 두 삼각형으로 나눠 그린다
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indices;
		AddConvexPolygon(Verts, Indices, Xform, { Arrow[0], Arrow[1], Arrow[2] }, ToColor(PlayerColor));
		AddConvexPolygon(Verts, Indices, Xform, { Arrow[0], Arrow[2], Arrow[3] }, ToColor(PlayerColor));
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, ++LayerId, Handle, Verts, Indices, nullptr, 0, 0);

		TArray<FVector2f> Outline = Arrow;
		Outline.Add(Arrow[0]);
		FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, LineGeometry, Outline, ESlateDrawEffect::None,
			IconOutlineColor * FLinearColor(1.f, 1.f, 1.f, Opacity), true, 1.5f);
	}

	return LayerId;
}
