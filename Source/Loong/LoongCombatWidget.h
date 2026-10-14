#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoongCombatTypes.h"
#include "LoongCombatWidget.generated.h"
class ALoongCombatDirector;
class UTextBlock;
class UVerticalBox;
class ULoongBuildingMenuButton;
class USizeBox;
UCLASS()
class LOONG_API ULoongCombatWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(ALoongCombatDirector* InDirector);

protected:
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    UFUNCTION()
    void UpgradeClicked(FName Id);

    UFUNCTION()
    void ReturnClicked();
    TWeakObjectPtr<ALoongCombatDirector> Director;

    UPROPERTY()
    TObjectPtr<UTextBlock> HUD;

    UPROPERTY()
    TObjectPtr<UVerticalBox> Cards;
    ELoongCombatState DisplayedState = ELoongCombatState::Preparing;
    TArray<FName> DisplayedChoices;
    void RefreshCards();
    void BuildElementTestPanel(class UCanvasPanel* Root);
    void RefreshElementTestPanel();
    void RestoreElementTestSettings();
    UFUNCTION()
    void ToggleElementPanel();
    UFUNCTION()
    void ToggleElementVisualization();
    UFUNCTION()
    void ToggleElementTestMode();
    UFUNCTION()
    void StopElementTest();
    UPROPERTY()
    TObjectPtr<UVerticalBox> ElementPanel;
    UPROPERTY()
    TObjectPtr<UTextBlock> ElementStatus;
    UPROPERTY()
    TObjectPtr<UTextBlock> DebugButtonText;
    UPROPERTY()
    TObjectPtr<UTextBlock> TestButtonText;
    UPROPERTY()
    TObjectPtr<USizeBox> ElementPanelSize;
    UPROPERTY()
    TObjectPtr<USizeBox> ElementStatusSize;
    bool bElementPanelOpen = false;
    bool bOwnedElementSettings = false;
    bool bInitialElementDebug = false;
    bool bInitialElementTest = false;
    float ElementRefreshTime = 0.f;
};
