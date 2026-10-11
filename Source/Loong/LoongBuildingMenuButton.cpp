#include "LoongBuildingMenuButton.h"

void ULoongBuildingMenuButton::SetBuildingId(const FName InBuildingId)
{
	BuildingId = InBuildingId;
	OnClicked.RemoveAll(this);
	OnClicked.AddDynamic(this, &ULoongBuildingMenuButton::HandleClicked);
}

void ULoongBuildingMenuButton::HandleClicked()
{
	OnBuildingClicked.Broadcast(BuildingId);
}
