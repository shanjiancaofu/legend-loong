#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "LoongBuildingMenuButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoongBuildingMenuButtonClicked, FName, BuildingId);

UCLASS()
class LOONG_API ULoongBuildingMenuButton : public UButton
{
    GENERATED_BODY()

public:
    ULoongBuildingMenuButton();

    UPROPERTY(BlueprintAssignable, Category = "Loong|Building|Menu")
    FLoongBuildingMenuButtonClicked OnBuildingClicked;

    void SetBuildingId(FName InBuildingId);

private:
    UPROPERTY(Transient)
    FName BuildingId = NAME_None;

    UFUNCTION()
    void HandleClicked();
};
