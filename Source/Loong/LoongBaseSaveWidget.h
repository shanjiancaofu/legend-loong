#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoongBaseSaveWidget.generated.h"
class ULoongBaseSaveSubsystem;
class ULoongBuildingSystemComponent;
class UTextBlock;
class UWidget;
class UButton;

/** Native player-facing save controls. No Widget Blueprint required. */
UCLASS()
class LOONG_API ULoongBaseSaveWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void InitializePanel(ULoongBuildingSystemComponent* InSystem);
 bool IsPointerOverPanel() const;
 bool IsConfirmationOpen() const { return PendingAction != NAME_None; }
 void CancelConfirmation();
protected:
 virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
 void BuildPanel();
 void RefreshStatus();
 void RequestAction(FName Action);
 ULoongBaseSaveSubsystem* Service() const;
 UFUNCTION() void SaveClicked();
 UFUNCTION() void LoadClicked();
 UFUNCTION() void StatusClicked();
 UFUNCTION() void ClearClicked();
 UFUNCTION() void DeleteClicked();
 UFUNCTION() void ConfirmClicked();
 UFUNCTION() void CancelClicked();
 TWeakObjectPtr<ULoongBuildingSystemComponent> BuildingSystem;
 FName PendingAction;
 FText Feedback;
 FString LastServiceStatus;
 float RefreshElapsed = 0.f;
 bool bCachedHasSave = false;
 UPROPERTY(Transient) TObjectPtr<UWidget> Panel;
 UPROPERTY(Transient) TObjectPtr<UWidget> Confirmation;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptText;
 UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ActionButtons;
};
