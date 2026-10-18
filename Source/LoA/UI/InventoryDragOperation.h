#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryDragOperation.generated.h"

class UInventoryWidget;

/**
 * 소지품 칸 드래그 — 다른 칸 위에 놓으면 그 칸의 NativeOnDrop이 받는다.
 * 아무 위젯도 안 받으면(DragCancelled) 커서가 HUD 배틀아이템 칸 위인지 컨트롤러에 물어 등록한다 — HUD는 HitTestInvisible이라 드랍을 못 받기 때문
 */
UCLASS()
class LOA_API UInventoryDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	int32 SourceIndex = INDEX_NONE;
	TWeakObjectPtr<UInventoryWidget> SourceInventory;

	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
};
