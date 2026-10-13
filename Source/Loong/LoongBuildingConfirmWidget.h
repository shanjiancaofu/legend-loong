#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoongBuildingConfirmWidget.generated.h"

class ULoongBuildingSystemComponent;

UCLASS()
class LOONG_API ULoongBuildingConfirmWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool IsPointerOverPanel() const;
    void InitializeOperations(ULoongBuildingSystemComponent* InSystem);
    void InitializeDeletePrompt(ULoongBuildingSystemComponent* InSystem, FText InBuildingName);

protected:
    virtual void NativeConstruct() override;

private:
    void RebuildContent();

    UFUNCTION()
    void MoveClicked();

    UFUNCTION()
    void DeleteClicked();

    UFUNCTION()
    void ClearClicked();

    UFUNCTION()
    void ConfirmClicked();

    UFUNCTION()
    void CancelClicked();
    TWeakObjectPtr<ULoongBuildingSystemComponent> BuildingSystem;
    FText BuildingName;
    bool bOperations = false;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> PanelWidget;
};
