#include "UI/InventoryDragOperation.h"
#include "UI/InventoryWidget.h"

void UInventoryDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	if (UInventoryWidget* Inventory = SourceInventory.Get())
	{
		Inventory->HandleDragCancelled(SourceIndex);
	}
}
