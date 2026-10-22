#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MinimapWidget.generated.h"

class AHexArena;
class AEchidnaBoss;
class ALobbyZoneActor;
class AInteractableNPCActor;
class UTexture2D;

/**
 * 화면 우상단 미니맵 — 육각 아레나 타일 + 보스(빨간 마름모) + 플레이어(초록 화살표).
 * WBP·텍스처 없이 NativePaint에서 Slate 커스텀 버텍스로 직접 그린다 (에디터에서 바인딩을 걸 필요가 없게 — ScreenFogWidget과 같은 방침).
 * 방향은 탑다운 카메라 기준 — 화면 위쪽이 미니맵 위쪽이라 화면과 미니맵이 항상 같은 방향으로 보인다.
 * 타일은 전부 같은 색(오염 장판 등 타일 상태는 표시하지 않음). 보스가 사라진 동안(숨김)은 보스 아이콘도 숨긴다.
 * 연기(ZOrder -1)보다 위에 올려서 그네·랜잡 패턴 중에도 보스 위치가 보인다.
 * 아레나가 없는 맵(대기 지역)은 바닥 사각형 + 구역(정비소·입장, 테두리 색) + NPC(노란 점) + 플레이어를 그린다.
 */
UCLASS()
class LOA_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 미니맵 한 변 크기 / 화면 우상단 모서리에서 떨어진 거리 (슬레이트 단위)
	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	float MapSize = 220.f;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	FVector2D ScreenMargin = FVector2D(20.f, 20.f);

	// 테두리 안쪽 여백 (슬레이트 단위)
	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	float MapPadding = 12.f;

	// 타일 크기 비율 — 1보다 작으면 타일 사이에 틈이 보인다
	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	float TileInset = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	float BossIconSize = 9.f;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap")
	float PlayerIconSize = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Color")
	FLinearColor BackgroundColor = FLinearColor(0.35f, 0.03f, 0.18f, 0.75f);

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Color")
	FLinearColor TileColor = FLinearColor(0.85f, 0.55f, 0.62f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Color")
	FLinearColor BossColor = FLinearColor(0.95f, 0.05f, 0.05f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Color")
	FLinearColor PlayerColor = FLinearColor(0.2f, 0.95f, 0.35f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Color")
	FLinearColor IconOutlineColor = FLinearColor::White;

	// ── 아레나가 없는 맵(대기 지역) ──
	// 바닥 중심과 한 변의 절반 (Lv_EchidnaLobby 바닥 = ±2000)
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	FVector2D LobbyWorldCenter = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	float LobbyHalfExtent = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	float NPCIconSize = 5.f;

	// NPC 전용 아이콘(MinimapIcon)의 세로 크기 — 가로는 텍스처 비율
	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	float NPCTextureIconSize = 22.f;

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	FLinearColor LobbyBackgroundColor = FLinearColor(0.04f, 0.05f, 0.07f, 0.8f);

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	FLinearColor LobbyFloorColor = FLinearColor(0.42f, 0.4f, 0.36f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "Minimap|Lobby")
	FLinearColor NPCColor = FLinearColor(1.f, 0.85f, 0.2f, 1.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// 레벨에서 한 번 찾아 캐시 (NativePaint는 const라 NativeTick에서 찾는다)
	TWeakObjectPtr<AHexArena> Arena;
	TWeakObjectPtr<AEchidnaBoss> Boss;

	// 대기 지역 구역·NPC (처음 한 번 모은다 — 전부 레벨에 배치된 액터라 늘지 않음)
	TArray<TWeakObjectPtr<ALobbyZoneActor>> Zones;
	TArray<TWeakObjectPtr<AInteractableNPCActor>> NPCs;

	// NPC별 아이콘 브러시 (NPCs와 같은 순서, 아이콘이 없으면 텍스처 비움 → 노란 점)
	TArray<FSlateBrush> NPCBrushes;

	// 브러시가 가리키는 텍스처를 GC가 치우지 않게 붙잡아 둠 (브러시 배열은 UPROPERTY가 아니라서)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> NPCIconTextures;
	bool bGatheredLobbyActors = false;
};
