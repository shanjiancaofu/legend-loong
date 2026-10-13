#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoongCombatTypes.h"
#include "LoongCombatWidget.generated.h"
class ALoongCombatDirector; class UTextBlock; class UVerticalBox; class ULoongBuildingMenuButton;
UCLASS() class LOONG_API ULoongCombatWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Setup(ALoongCombatDirector* InDirector);
protected:
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
 UFUNCTION() void UpgradeClicked(FName Id);
 UFUNCTION() void ReturnClicked();
 TWeakObjectPtr<ALoongCombatDirector> Director;
 UPROPERTY() TObjectPtr<UTextBlock> HUD;
 UPROPERTY() TObjectPtr<UVerticalBox> Cards;
 ELoongCombatState DisplayedState=ELoongCombatState::Preparing;
 TArray<FName> DisplayedChoices;
 void RefreshCards();
};
