#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoongBuildingMenuWidget.generated.h"

class ULoongBuildingSystemComponent;
class UVerticalBox;

UCLASS()
class LOONG_API ULoongBuildingMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForBuildingSystem(ULoongBuildingSystemComponent* InBuildingSystem);

protected:
	virtual void NativeConstruct() override;

private:
	void RebuildMenu();

	UFUNCTION()
	void HandleBuildingSelected(FName BuildingId);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BuildingList;

	TWeakObjectPtr<ULoongBuildingSystemComponent> BuildingSystem;
};
