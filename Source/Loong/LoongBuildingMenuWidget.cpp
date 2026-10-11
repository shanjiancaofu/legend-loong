#include "LoongBuildingMenuWidget.h"

#include "LoongBuildingMenuButton.h"
#include "LoongBuildingSystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

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

	UVerticalBox* Root = Cast<UVerticalBox>(WidgetTree->RootWidget);
	if (!Root)
	{
		Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildingMenuRoot"));
		WidgetTree->RootWidget = Root;
	}
	Root->ClearChildren();
	BuildingList = Root;

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildingMenuTitle"));
	Title->SetText(NSLOCTEXT("LoongBuilding", "BuildingMenuTitle", "Buildings (1-4)"));
	Root->AddChildToVerticalBox(Title);

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
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s  [%dx%d]  <%s>"),
			*Definition.DisplayName.ToString(), Definition.GridWidth, Definition.GridHeight, CategoryText)));
		Button->SetContent(Label);
		Root->AddChildToVerticalBox(Button);
	}
}

void ULoongBuildingMenuWidget::HandleBuildingSelected(const FName BuildingId)
{
	if (BuildingSystem.IsValid())
	{
		BuildingSystem->SelectBuildingById(BuildingId);
	}
}
