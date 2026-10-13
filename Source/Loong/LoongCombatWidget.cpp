#include "LoongCombatWidget.h"
#include "LoongCombatDemo.h"
#include "LoongBuildingMenuButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
void ULoongCombatWidget::Setup(ALoongCombatDirector* InDirector)
{
 Director=InDirector;auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Root;
 HUD=WidgetTree->ConstructWidget<UTextBlock>();auto* H=Root->AddChildToCanvas(HUD);H->SetAutoSize(true);H->SetPosition(FVector2D(20,20));
 auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(580);auto* B=WidgetTree->ConstructWidget<UBorder>();B->SetBrushColor(FLinearColor(0.03f,0.04f,0.06f,0.95f));B->SetPadding(FMargin(16));Cards=WidgetTree->ConstructWidget<UVerticalBox>();B->SetContent(Cards);Size->SetContent(B);auto* S=Root->AddChildToCanvas(Size);S->SetAutoSize(true);S->SetAnchors(FAnchors(0.5,0.5));S->SetAlignment(FVector2D(0.5,0.5));RefreshCards();
}
void ULoongCombatWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);if(!Director.IsValid() || !HUD)return;
 HUD->SetText(FText::FromString(FString::Printf(TEXT("生命 %.0f / %.0f    等级 %d    经验 %d / %d\n剩余 %.0f 秒    击杀 %d"),Director->PlayerHealth?Director->PlayerHealth->CurrentHealth:0,Director->PlayerHealth?Director->PlayerHealth->MaxHealth:0,Director->Level,Director->Experience,Director->ExperienceThreshold(),FMath::Max(0.f,Director->Definition->DurationSeconds-Director->Elapsed),Director->Kills)));
 if(DisplayedState!=Director->State || DisplayedChoices!=Director->UpgradeChoices)RefreshCards();
}
void ULoongCombatWidget::RefreshCards()
{
 if(!Director.IsValid() || !Cards)return;DisplayedState=Director->State;DisplayedChoices=Director->UpgradeChoices;Cards->ClearChildren();Cards->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
 if(DisplayedState!=ELoongCombatState::UpgradeSelection && DisplayedState!=ELoongCombatState::Results)return;
 Cards->GetParent()->SetVisibility(ESlateVisibility::Visible);auto* Title=WidgetTree->ConstructWidget<UTextBlock>();
 Title->SetText(FText::FromString(DisplayedState==ELoongCombatState::UpgradeSelection?TEXT("升级：选择一项强化"):FString::Printf(TEXT("%s\n击杀 %d    等级 %d    测试奖励 %d"),Director->Result.bVictory?TEXT("胜利"):TEXT("失败"),Director->Result.Kills,Director->Result.Level,Director->Result.TestReward)));Cards->AddChildToVerticalBox(Title);
 if(DisplayedState==ELoongCombatState::UpgradeSelection)
 {
  for(FName Id:DisplayedChoices){const auto* U=Director->Definition->Upgrades.FindByPredicate([Id](const FLoongUpgradeDefinition& D){return D.UpgradeId==Id;});if(!U)continue;auto* Button=WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();Button->SetBuildingId(FName(*FString::Printf(TEXT("%d|%s"),Director->UpgradeGeneration,*Id.ToString())));Button->OnBuildingClicked.AddDynamic(this,&ULoongCombatWidget::UpgradeClicked);auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(U->DisplayName.ToString()+TEXT(" — ")+U->Description.ToString()));Text->SetAutoWrapText(true);Button->SetContent(Text);Cards->AddChildToVerticalBox(Button);}
 }
 else{auto* Button=WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(TEXT("返回基地")));Button->SetContent(Text);Button->OnClicked.AddDynamic(this,&ULoongCombatWidget::ReturnClicked);Cards->AddChildToVerticalBox(Button);}
}
void ULoongCombatWidget::UpgradeClicked(FName Id){if(Director.IsValid()){FString Generation,Upgrade; if(Id.ToString().Split(TEXT("|"),&Generation,&Upgrade))Director->ChooseUpgrade(FName(*Upgrade),FCString::Atoi(*Generation));}RefreshCards();}
void ULoongCombatWidget::ReturnClicked(){if(Director.IsValid())Director->ReturnToBase();}
