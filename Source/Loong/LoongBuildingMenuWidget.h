#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoongBuildingMenuWidget.generated.h"

class ULoongBuildingSystemComponent;
class UVerticalBox;
class UComboBoxString;
class ULoongBuildingMenuButton;

UCLASS()
class LOONG_API ULoongBuildingMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	bool IsPointerOverPanel() const;
 void RefreshSelection();
 void InitializeForBuildingSystem(ULoongBuildingSystemComponent* InBuildingSystem);

protected:
	virtual void NativeConstruct() override;

private:
	void RebuildMenu();
 void RebuildEntries();
 UFUNCTION() void FilterChanged(FString Item, ESelectInfo::Type SelectionType);
 UPROPERTY(Transient) TObjectPtr<UWidget> PanelWidget;
 UPROPERTY(Transient) TObjectPtr<UComboBoxString> CategoryFilter;
 UPROPERTY(Transient) TArray<TObjectPtr<ULoongBuildingMenuButton>> EntryButtons;
 TArray<FName> EntryIds;

	UFUNCTION()
	void HandleBuildingSelected(FName BuildingId);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BuildingList;

	TWeakObjectPtr<ULoongBuildingSystemComponent> BuildingSystem;
};
