#include "LoongBuildingConfirmWidget.h"
#include "LoongBuildingSystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "LoongBuildingMenuButton.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Framework/Application/SlateApplication.h"

void ULoongBuildingConfirmWidget::InitializeOperations(ULoongBuildingSystemComponent* InSystem)
{
 BuildingSystem = InSystem; bOperations = true; RebuildContent();
}
void ULoongBuildingConfirmWidget::InitializeDeletePrompt(ULoongBuildingSystemComponent* InSystem, FText InBuildingName)
{
 BuildingSystem = InSystem; BuildingName = InBuildingName; bOperations = false; RebuildContent();
}
void ULoongBuildingConfirmWidget::NativeConstruct()
{
 Super::NativeConstruct();
}
void ULoongBuildingConfirmWidget::RebuildContent()
{
 if (!WidgetTree) return;
 UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas; Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
 USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(); Panel->SetWidthOverride(bOperations ? 280.f : 360.f); PanelWidget = Panel;
 UBorder* Background = WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor(0.04f,0.04f,0.04f,0.95f)); Background->SetPadding(FMargin(12));
 UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Root); Panel->SetContent(Background);
 auto* CanvasSlot = Canvas->AddChildToCanvas(Panel); CanvasSlot->SetAutoSize(true);
 CanvasSlot->SetAnchors(bOperations ? FAnchors(1.f,0.5f) : FAnchors(0.5f,0.5f));
 CanvasSlot->SetAlignment(bOperations ? FVector2D(1,0.5f) : FVector2D(0.5f,0.5f)); CanvasSlot->SetPosition(bOperations ? FVector2D(-16,0) : FVector2D::ZeroVector);
 UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(); Text->SetAutoWrapText(true);
 if (bOperations && BuildingSystem.IsValid())
 {
  const auto D = BuildingSystem->GetSelectedDefinition();
  const UEnum* Categories = StaticEnum<ELoongBuildingCategory>();
  Text->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %d x %d"), *D.DisplayName.ToString(), *Categories->GetDisplayNameTextByValue(static_cast<int64>(D.BuildingCategory)).ToString(), D.GridWidth, D.GridHeight)));
 }
 else Text->SetText(FText::Format(NSLOCTEXT("LoongBuilding", "DeletePrompt", "Delete {0}? This cannot be undone."), BuildingName));
 Root->AddChildToVerticalBox(Text);
 auto Button = [this,Root](const FText& Label)
 {
  UButton* B = WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();
  UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(Label); B->SetContent(T); Root->AddChildToVerticalBox(B); return B;
 };
 if (bOperations)
 {
  Button(NSLOCTEXT("LoongBuilding","Move","Move (M)"))->OnClicked.AddDynamic(this,&ULoongBuildingConfirmWidget::MoveClicked);
  Button(NSLOCTEXT("LoongBuilding","Delete","Demolish (Delete)"))->OnClicked.AddDynamic(this,&ULoongBuildingConfirmWidget::DeleteClicked);
  Button(NSLOCTEXT("LoongBuilding","Clear","Deselect"))->OnClicked.AddDynamic(this,&ULoongBuildingConfirmWidget::ClearClicked);
 }
 else
 {
  Button(NSLOCTEXT("LoongBuilding","Confirm","Confirm Delete"))->OnClicked.AddDynamic(this,&ULoongBuildingConfirmWidget::ConfirmClicked);
  Button(NSLOCTEXT("LoongBuilding","Cancel","Cancel (Esc)"))->OnClicked.AddDynamic(this,&ULoongBuildingConfirmWidget::CancelClicked);
 }
}
bool ULoongBuildingConfirmWidget::IsPointerOverPanel() const { return PanelWidget && FSlateApplication::IsInitialized() && PanelWidget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos()); }
void ULoongBuildingConfirmWidget::MoveClicked() { if(BuildingSystem.IsValid()) BuildingSystem->BeginMoveSelectedBuilding(); }
void ULoongBuildingConfirmWidget::DeleteClicked() { if(BuildingSystem.IsValid()) BuildingSystem->RequestDeleteSelectedBuilding(); }
void ULoongBuildingConfirmWidget::ClearClicked() { if(BuildingSystem.IsValid()) BuildingSystem->ClearSelectedBuilding(); }
void ULoongBuildingConfirmWidget::ConfirmClicked() { if(BuildingSystem.IsValid()) BuildingSystem->ConfirmDeleteSelectedBuilding(); }
void ULoongBuildingConfirmWidget::CancelClicked() { if(BuildingSystem.IsValid()) BuildingSystem->CancelDeletePrompt(); }
