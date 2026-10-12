#include "LoongBuildingMenuWidget.h"

#include "LoongBuildingMenuButton.h"
#include "LoongBuildingSystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
#include "Framework/Application/SlateApplication.h"

void ULoongBuildingMenuWidget::InitializeForBuildingSystem(ULoongBuildingSystemComponent* InBuildingSystem)
{
	BuildingSystem = InBuildingSystem;
	RebuildMenu();
}

void ULoongBuildingMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildMenu();
}

void ULoongBuildingMenuWidget::RebuildMenu()
{
	if (!WidgetTree || !BuildingSystem.IsValid())
	{
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingMenuRoot"));
		WidgetTree->RootWidget = Root;
 Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	Root->ClearChildren();
	USizeBox* Panel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildingMenuPanel"));
	Panel->SetWidthOverride(280.0f); Panel->SetHeightOverride(520.0f);
 PanelWidget = Panel;
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BuildingMenuScroll"));
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildingMenuList"));
	Scroll->AddChild(List); Panel->AddChild(Scroll);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.0f, 0.5f, 0.0f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.0f, 0.5f));
		PanelSlot->SetPosition(FVector2D(16.0f, 0.0f));
 PanelSlot->SetAutoSize(true);
	}
	BuildingList = List;

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildingMenuTitle"));
	Title->SetText(NSLOCTEXT("LoongBuilding", "BuildingMenuTitle", "Buildings (1-4)"));
	List->AddChildToVerticalBox(Title);

CategoryFilter = WidgetTree->ConstructWidget<UComboBoxString>();
 for (const TCHAR* Category : {TEXT("All"), TEXT("Housing"), TEXT("Production"), TEXT("Utility"), TEXT("Decoration"), TEXT("Other")}) CategoryFilter->AddOption(Category);

	CategoryFilter->SetSelectedOption(TEXT("All"));
	CategoryFilter->OnSelectionChanged.AddDynamic(this, &ULoongBuildingMenuWidget::FilterChanged);
	List->AddChildToVerticalBox(CategoryFilter);
	BuildingList = WidgetTree->ConstructWidget<UVerticalBox>();
	List->AddChildToVerticalBox(BuildingList);
	RebuildEntries();
}
void ULoongBuildingMenuWidget::RebuildEntries()
{
 if (!BuildingList || !BuildingSystem.IsValid()) return;
 BuildingList->ClearChildren(); EntryButtons.Empty(); EntryIds.Empty();
 for (const FLoongBuildingDefinition& Definition : BuildingSystem->BuildingDefinitions)
	{
		if (!Definition.IsUsable())
		{
			continue;
		}

		ULoongBuildingMenuButton* Button = WidgetTree->ConstructWidget<ULoongBuildingMenuButton>(
			ULoongBuildingMenuButton::StaticClass(), NAME_None);
		Button->SetBuildingId(Definition.BuildingId);
		Button->OnBuildingClicked.AddDynamic(this, &ULoongBuildingMenuWidget::HandleBuildingSelected);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), NAME_None);
		const TCHAR* CategoryText = TEXT("Other");
		switch (Definition.BuildingCategory)
		{
		case ELoongBuildingCategory::Housing: CategoryText = TEXT("Housing"); break;
		case ELoongBuildingCategory::Production: CategoryText = TEXT("Production"); break;
		case ELoongBuildingCategory::Utility: CategoryText = TEXT("Utility"); break;
		case ELoongBuildingCategory::Decoration: CategoryText = TEXT("Decoration"); break;
		default: break;
		}
		if (CategoryFilter && CategoryFilter->GetSelectedOption() != TEXT("All") && CategoryFilter->GetSelectedOption() != CategoryText) continue;

 Label->SetAutoWrapText(true);
 Label->SetText(FText::FromString(FString::Printf(TEXT("%s  [%dx%d]  <%s>"),
			*Definition.DisplayName.ToString(), Definition.GridWidth, Definition.GridHeight, CategoryText)));
		Button->SetContent(Label);
 Button->SetToolTipText(Definition.Description);
 Button->SetIsEnabled(Definition.bUnlocked);
		BuildingList->AddChildToVerticalBox(Button);
 EntryButtons.Add(Button); EntryIds.Add(Definition.BuildingId);
	}
 RefreshSelection();
}

void ULoongBuildingMenuWidget::HandleBuildingSelected(const FName BuildingId)
{
	if (BuildingSystem.IsValid())
	{
		BuildingSystem->SelectBuildingById(BuildingId);
	}
}

bool ULoongBuildingMenuWidget::IsPointerOverPanel() const
{
 if (CategoryFilter && CategoryFilter->IsOpen()) return true;
 return PanelWidget && FSlateApplication::IsInitialized() && PanelWidget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}
void ULoongBuildingMenuWidget::RefreshSelection()
{
 if (!BuildingSystem.IsValid()) return;
 for (int32 i=0; i<EntryButtons.Num(); ++i)
  EntryButtons[i]->SetBackgroundColor(EntryIds[i] == BuildingSystem->GetSelectedBuildingId() ? FLinearColor(0.15f,0.7f,0.3f,1) : FLinearColor(0.2f,0.2f,0.2f,1));
}
void ULoongBuildingMenuWidget::FilterChanged(FString Item, ESelectInfo::Type SelectionType) { RebuildEntries(); if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport(); }
