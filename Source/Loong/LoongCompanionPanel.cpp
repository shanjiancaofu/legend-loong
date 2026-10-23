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
#include "Components/ScrollBox.h"
#include "Components/ButtonSlot.h"

void ULoongCombatWidget::BuildCompanionPanel(UCanvasPanel* Root)
{
    CompanionPanelSize = WidgetTree->ConstructWidget<USizeBox>();
    CompanionPanelSize->SetWidthOverride(310);
    auto* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetPadding(FMargin(8));
    Border->SetBrushColor(FLinearColor(0.025f, 0.04f, 0.055f, 0.95f));
    CompanionPanelSize->SetContent(Border);
    auto* List = WidgetTree->ConstructWidget<UVerticalBox>();
    Border->SetContent(List);
    auto* Position = Root->AddChildToCanvas(CompanionPanelSize);
    Position->SetAutoSize(true);
    Position->SetAnchors(FAnchors(0, 1));
    Position->SetAlignment(FVector2D(0, 1));
    Position->SetPosition(FVector2D(16, -16));
    CompanionStatus = WidgetTree->ConstructWidget<UTextBlock>();
    CompanionStatus->SetAutoWrapText(true);
    auto Font = CompanionStatus->GetFont();
    Font.Size = 12;
    CompanionStatus->SetFont(Font);
    List->AddChildToVerticalBox(CompanionStatus);
    auto* ScrollSize = WidgetTree->ConstructWidget<USizeBox>();
    ScrollSize->SetHeightOverride(180);
    auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    ScrollSize->SetContent(Scroll);
    auto* Buttons = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Buttons);
    List->AddChildToVerticalBox(ScrollSize);
    auto Add = [this, Buttons](FName Action, const FString& Label)
    {
        auto* Button = WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();
        Button->SetBuildingId(Action);
        Button->OnBuildingClicked.AddDynamic(this, &ULoongCombatWidget::CompanionAction);
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        Text->SetAutoWrapText(true);
        Text->SetJustification(ETextJustify::Center);
        auto Font = Text->GetFont();
        Font.Size = 13;
        Text->SetFont(Font);
        Button->SetContent(Text);
        if (auto* Slot = Cast<UButtonSlot>(Text->Slot))
        {
            Slot->SetPadding(FMargin(5, 3));
            Slot->SetHorizontalAlignment(HAlign_Fill);
        }
        Buttons->AddChildToVerticalBox(Button);
        CompanionButtons.Add(Button);
    };
    Add(TEXT("Ride"), TEXT("F · 上龙 / 下龙"));
    Add(TEXT("Recall"), TEXT("召回伴战"));
    Add(TEXT("Unequip"), TEXT("卸下实体伴战"));
    Add(TEXT("Skill0"), TEXT("1 · 指挥 / 释放技能"));
    Add(TEXT("Skill1"), TEXT("2 · 指挥 / 释放技能"));
    Add(TEXT("Skill2"), TEXT("3 · 指挥 / 释放技能"));
    if (Director.IsValid() && Director->Definition)
    {
        TSet<FName> Seen;
        auto Presets = Director->Definition->Dragons;
        Presets.Append(Director->Definition->ElementDragonPresets);
        for (const auto& Preset : Presets)
            if (!Preset.DragonId.IsNone() && !Seen.Contains(Preset.DragonId))
            {
                Seen.Add(Preset.DragonId);
                Add(FName(*(TEXT("Equip:") + Preset.DragonId.ToString())),
                    TEXT("试装 / 切换：") + Preset.DragonId.ToString());
            }
    }
    RefreshCompanionPanel();
}

void ULoongCombatWidget::CompanionAction(FName Action)
{
    if (!Director.IsValid() || !Director->IsFighting())
        return;
    const FString Name = Action.ToString();
    if (Name.StartsWith(TEXT("Equip:")))
        Director->SelectCompanion(FName(*Name.Mid(6)));
    else if (Action == TEXT("Ride"))
        Director->ToggleMount();
    else if (Action == TEXT("Recall"))
        Director->RecallCompanion();
    else if (Action == TEXT("Unequip"))
        Director->UnequipCompanions();
    else if (Name.StartsWith(TEXT("Skill")))
        Director->CommandCompanionSkill(FCString::Atoi(*Name.Mid(5)));
    RefreshCompanionPanel();
}

void ULoongCombatWidget::RefreshCompanionPanel()
{
    if (CompanionStatus && Director.IsValid())
        CompanionStatus->SetText(
            FText::FromString(TEXT("伴战与骑乘原型（不保存试装）\n") + Director->GetCompanionStatus()));
    for (auto& Button : CompanionButtons)
        if (Button)
            Button->SetIsEnabled(Director.IsValid() && Director->IsFighting());
}

bool ULoongCombatWidget::IsPointerOverControls() const
{
    return (CompanionPanelSize && CompanionPanelSize->IsHovered()) ||
           (ElementPanelSize && ElementPanelSize->IsHovered()) ||
           (Cards && Cards->GetParent() && Cards->GetParent()->IsVisible() &&
            Cards->GetParent()->IsHovered());
}
