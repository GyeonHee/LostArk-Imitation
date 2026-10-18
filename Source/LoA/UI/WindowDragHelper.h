#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"

/**
 * CanvasPanel 안의 창(Border 등)을 마우스로 끄는 공용 로직 — 소지품·상점 창이 같이 쓴다.
 * 루트 위젯의 NativeOnMouseButtonDown/Move/Up에서 Begin/Move/End를 부르고, Begin이 true면 마우스를 캡처할 것.
 * 창 슬롯의 앵커·정렬이 무엇이든 창이 화면 밖으로 나가지 않게 막는다.
 */
struct FLoAWindowDrag
{
	bool bDragging = false;
	FVector2D StartMouse = FVector2D::ZeroVector;    // 루트 위젯 로컬 좌표
	FVector2D StartPosition = FVector2D::ZeroVector; // 창 CanvasSlot 위치

	bool Begin(UWidget* Window, const FGeometry& RootGeometry, const FPointerEvent& MouseEvent)
	{
		const UCanvasPanelSlot* WindowSlot = Window ? Cast<UCanvasPanelSlot>(Window->Slot) : nullptr;
		if (!WindowSlot) return false;
		bDragging = true;
		StartMouse = RootGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		StartPosition = WindowSlot->GetPosition();
		return true;
	}

	void Move(UWidget* Window, const FGeometry& RootGeometry, const FPointerEvent& MouseEvent) const
	{
		UCanvasPanelSlot* WindowSlot = (bDragging && Window) ? Cast<UCanvasPanelSlot>(Window->Slot) : nullptr;
		if (!WindowSlot) return;
		const FVector2D Mouse = RootGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		FVector2D NewPosition = StartPosition + (Mouse - StartMouse);
		ClampToScreen(Window, RootGeometry, NewPosition);
		WindowSlot->SetPosition(NewPosition);
	}

	void End() { bDragging = false; }

	// 창 왼쪽 위 = 앵커 * 화면 + 위치 - 정렬 * 창 크기 → 그게 [0, 화면 - 창 크기] 안에 들도록 위치를 자른다
	static void ClampToScreen(const UWidget* Window, const FGeometry& RootGeometry, FVector2D& InOutPosition)
	{
		const UCanvasPanelSlot* WindowSlot = Window ? Cast<UCanvasPanelSlot>(Window->Slot) : nullptr;
		if (!WindowSlot) return;
		const FVector2D Screen = RootGeometry.GetLocalSize();
		const FVector2D Size = Window->GetDesiredSize();
		if (Screen.X <= 0.f || Screen.Y <= 0.f) return;

		const FVector2D Anchor = WindowSlot->GetAnchors().Minimum;
		const FVector2D Align = WindowSlot->GetAlignment();
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const double Min = Align[Axis] * Size[Axis] - Anchor[Axis] * Screen[Axis];
			const double Max = Screen[Axis] - Size[Axis] + Align[Axis] * Size[Axis] - Anchor[Axis] * Screen[Axis];
			// 창이 화면보다 크면 왼쪽/위(헤더)가 보이는 쪽을 우선
			InOutPosition[Axis] = Max < Min ? Min : FMath::Clamp(InOutPosition[Axis], Min, Max);
		}
	}
};
