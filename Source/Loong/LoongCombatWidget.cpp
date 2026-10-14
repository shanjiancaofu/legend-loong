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
#include "LoongElementDebug.h"

void ULoongCombatWidget::Setup(ALoongCombatDirector* InDirector)
{
    Director = InDirector;
    auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    HUD = WidgetTree->ConstructWidget<UTextBlock>();
    auto* H = Root->AddChildToCanvas(HUD);
    H->SetAutoSize(true);
    H->SetPosition(FVector2D(20, 20));
    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(580);
    auto* B = WidgetTree->ConstructWidget<UBorder>();
    B->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.95f));
    B->SetPadding(FMargin(16));
    Cards = WidgetTree->ConstructWidget<UVerticalBox>();
    B->SetContent(Cards);
    Size->SetContent(B);
    auto* S = Root->AddChildToCanvas(Size);
    S->SetAutoSize(true);
    S->SetAnchors(FAnchors(0.5, 0.5));
    S->SetAlignment(FVector2D(0.5, 0.5));
    BuildElementTestPanel(Root);
    RefreshCards();
}

void ULoongCombatWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!Director.IsValid() || !HUD)
        return;
    HUD->SetText(FText::FromString(FString::Printf(
        TEXT("生命 %.0f / %.0f    等级 %d    经验 %d / %d\n剩余 %.0f 秒    击杀 %d"),
        Director->PlayerHealth ? Director->PlayerHealth->CurrentHealth : 0,
        Director->PlayerHealth ? Director->PlayerHealth->MaxHealth : 0, Director->Level, Director->Experience,
        Director->ExperienceThreshold(),
        FMath::Max(0.f, Director->Definition->DurationSeconds - Director->Elapsed), Director->Kills)));
    if (Director->State == ELoongCombatState::Results && bOwnedElementSettings)
        RestoreElementTestSettings();
    if (ElementPanelSize)
        ElementPanelSize->SetWidthOverride(FMath::Clamp(Geometry.GetLocalSize().X * 0.34f, 260.f, 400.f));
    if (ElementStatusSize)
        ElementStatusSize->SetHeightOverride(FMath::Clamp(Geometry.GetLocalSize().Y - 210.f, 100.f, 260.f));
    ElementRefreshTime -= DeltaTime;
    if (ElementRefreshTime <= 0.f)
    {
        ElementRefreshTime = 0.2f;
        RefreshElementTestPanel();
    }
    if (DisplayedState != Director->State || DisplayedChoices != Director->UpgradeChoices)
        RefreshCards();
}

void ULoongCombatWidget::RefreshCards()
{
    if (!Director.IsValid() || !Cards)
        return;
    DisplayedState = Director->State;
    DisplayedChoices = Director->UpgradeChoices;
    Cards->ClearChildren();
    Cards->GetParent()->SetVisibility(ESlateVisibility::Collapsed);
    if (DisplayedState != ELoongCombatState::UpgradeSelection && DisplayedState != ELoongCombatState::Results)
        return;
    Cards->GetParent()->SetVisibility(ESlateVisibility::Visible);
    auto* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(
        DisplayedState == ELoongCombatState::UpgradeSelection
            ? TEXT("升级：选择一项强化")
            : FString::Printf(TEXT("%s\n击杀 %d    等级 %d    测试奖励 %d"),
                              Director->Result.bVictory ? TEXT("胜利") : TEXT("失败"), Director->Result.Kills,
                              Director->Result.Level, Director->Result.TestReward)));
    Cards->AddChildToVerticalBox(Title);
    if (DisplayedState == ELoongCombatState::UpgradeSelection)
    {
        for (FName Id : DisplayedChoices)
        {
            const auto* U = Director->Definition->Upgrades.FindByPredicate(
                [Id](const FLoongUpgradeDefinition& D)
                {
                    return D.UpgradeId == Id;
                });
            if (!U)
                continue;
            auto* Button = WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();
            Button->SetBuildingId(
                FName(*FString::Printf(TEXT("%d|%s"), Director->UpgradeGeneration, *Id.ToString())));
            Button->OnBuildingClicked.AddDynamic(this, &ULoongCombatWidget::UpgradeClicked);
            auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
            Text->SetText(
                FText::FromString(U->DisplayName.ToString() + TEXT(" — ") + U->Description.ToString()));
            Text->SetAutoWrapText(true);
            Button->SetContent(Text);
            Cards->AddChildToVerticalBox(Button);
        }
    }
    else
    {
        auto* Button = WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(TEXT("返回基地")));
        Button->SetContent(Text);
        Button->OnClicked.AddDynamic(this, &ULoongCombatWidget::ReturnClicked);
        Cards->AddChildToVerticalBox(Button);
    }
}

void ULoongCombatWidget::UpgradeClicked(FName Id)
{
    if (Director.IsValid())
    {
        FString Generation, Upgrade;
        if (Id.ToString().Split(TEXT("|"), &Generation, &Upgrade))
            Director->ChooseUpgrade(FName(*Upgrade), FCString::Atoi(*Generation));
    }
    RefreshCards();
}

void ULoongCombatWidget::ReturnClicked()
{
    if (Director.IsValid())
        Director->ReturnToBase();
}

void ULoongCombatWidget::BuildElementTestPanel(UCanvasPanel* Root)
{
#if !UE_BUILD_SHIPPING
    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    ElementPanelSize = Size;
    Size->SetWidthOverride(400.f);
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetPadding(FMargin(10));
    Background->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.05f, 0.95f));
    auto* Container = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Container);
    Size->SetContent(Background);
    auto* PanelSlot = Root->AddChildToCanvas(Size);
    PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
    PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
    PanelSlot->SetPosition(FVector2D(-16.f, 16.f));
    PanelSlot->SetAutoSize(true);
    auto AddButton = [this](UVerticalBox* Parent, const TCHAR* Label, UTextBlock*& Text)
    {
        auto* Button = WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();
        Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label));
        auto Font = Text->GetFont();
        Font.Size = 14;
        Text->SetFont(Font);
        Text->SetAutoWrapText(true);
        Button->SetContent(Text);
        if (auto* Slot = Cast<UButtonSlot>(Text->Slot))
            Slot->SetPadding(FMargin(6.f, 4.f));
        Parent->AddChildToVerticalBox(Button);
        return Button;
    };
    UTextBlock* Title = nullptr;
    AddButton(Container, TEXT("元素测试（展开 / 收起）"), Title)
        ->OnClicked.AddDynamic(this, &ULoongCombatWidget::ToggleElementPanel);
    ElementPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    Container->AddChildToVerticalBox(ElementPanel);
    auto* Notice = WidgetTree->ConstructWidget<UTextBlock>();
    auto NoticeFont = Notice->GetFont();
    NoticeFont.Size = 12;
    Notice->SetFont(NoticeFont);
    Notice->SetAutoWrapText(true);
    Notice->SetText(FText::FromString(TEXT("高触发模式用于观察，结束后请恢复正常战斗。")));
    ElementPanel->AddChildToVerticalBox(Notice);
    UTextBlock* DebugText = nullptr;
    AddButton(ElementPanel, TEXT("开启状态可视化"), DebugText)
        ->OnClicked.AddDynamic(this, &ULoongCombatWidget::ToggleElementVisualization);
    DebugButtonText = DebugText;
    UTextBlock* TestText = nullptr;
    AddButton(ElementPanel, TEXT("开启高触发测试"), TestText)
        ->OnClicked.AddDynamic(this, &ULoongCombatWidget::ToggleElementTestMode);
    TestButtonText = TestText;
    UTextBlock* StopText = nullptr;
    AddButton(ElementPanel, TEXT("结束测试：恢复正常"), StopText)
        ->OnClicked.AddDynamic(this, &ULoongCombatWidget::StopElementTest);
    auto* StatusBox = WidgetTree->ConstructWidget<USizeBox>();
    ElementStatusSize = StatusBox;
    StatusBox->SetHeightOverride(260.f);
    auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    ElementStatus = WidgetTree->ConstructWidget<UTextBlock>();
    ElementStatus->SetAutoWrapText(true);
    auto Font = ElementStatus->GetFont();
    Font.Size = 12;
    ElementStatus->SetFont(Font);
    Scroll->AddChild(ElementStatus);
    StatusBox->SetContent(Scroll);
    ElementPanel->AddChildToVerticalBox(StatusBox);
    ElementPanel->SetVisibility(ESlateVisibility::Collapsed);
#endif
}

void ULoongCombatWidget::ToggleElementPanel()
{
    bElementPanelOpen = !bElementPanelOpen;
    if (ElementPanel)
        ElementPanel->SetVisibility(bElementPanelOpen ? ESlateVisibility::Visible
                                                      : ESlateVisibility::Collapsed);
    RefreshElementTestPanel();
}

void ULoongCombatWidget::ToggleElementVisualization()
{
    if (!Director.IsValid() || Director->State == ELoongCombatState::Results)
        return;
    if (!bOwnedElementSettings)
    {
        bInitialElementDebug = LoongElementDebug::Enabled();
        bInitialElementTest = LoongElementDebug::TestMode();
        bOwnedElementSettings = true;
    }
    LoongElementDebug::SetEnabled(!LoongElementDebug::Enabled());
    RefreshElementTestPanel();
}

void ULoongCombatWidget::ToggleElementTestMode()
{
    if (!Director.IsValid() || Director->State == ELoongCombatState::Results)
        return;
    if (!bOwnedElementSettings)
    {
        bInitialElementDebug = LoongElementDebug::Enabled();
        bInitialElementTest = LoongElementDebug::TestMode();
        bOwnedElementSettings = true;
    }
    LoongElementDebug::SetTestMode(!LoongElementDebug::TestMode());
    RefreshElementTestPanel();
}

void ULoongCombatWidget::RestoreElementTestSettings()
{
    if (!bOwnedElementSettings)
        return;
    LoongElementDebug::SetEnabled(bInitialElementDebug);
    LoongElementDebug::SetTestMode(bInitialElementTest);
    bOwnedElementSettings = false;
}

void ULoongCombatWidget::StopElementTest()
{
    RestoreElementTestSettings();
    RefreshElementTestPanel();
}

void ULoongCombatWidget::RefreshElementTestPanel()
{
    if (DebugButtonText)
        DebugButtonText->SetText(FText::FromString(LoongElementDebug::Enabled()
                                                       ? TEXT("状态可视化：已开启（点击关闭）")
                                                       : TEXT("状态可视化：已关闭（点击开启）")));
    if (TestButtonText)
        TestButtonText->SetText(FText::FromString(LoongElementDebug::TestMode()
                                                      ? TEXT("高触发测试：已开启（点击关闭）")
                                                      : TEXT("高触发测试：已关闭（点击开启）")));
    if (ElementStatus && bElementPanelOpen && Director.IsValid())
    {
        ALoongCombatEnemy* Observed = nullptr;
        const FVector Focus =
            IsValid(Director->Player) ? Director->Player->GetActorLocation() : Director->GetActorLocation();
        for (const auto& Enemy : Director->Enemies)
            if (IsValid(Enemy) && IsValid(Enemy->Health) && Enemy->Health->IsAlive() &&
                IsValid(Enemy->Elements) && Enemy->Elements->GetEffectCount() > 0 &&
                (!Observed || FVector::DistSquared2D(Enemy->GetActorLocation(), Focus) <
                                  FVector::DistSquared2D(Observed->GetActorLocation(), Focus)))
                Observed = Enemy;
        const FString TargetDetails =
            Observed ? FString::Printf(TEXT("\n\n当前观察目标：%s\n%s"), *Observed->GetName(),
                                       *Observed->Elements->BuildDebugText())
                     : TEXT("\n\n附近暂无生效的元素目标。");
        ElementStatus->SetText(FText::FromString(TEXT("火：橙红　冰：青色　风：绿色") + TargetDetails +
                                                 TEXT("\n\n配置摘要：\n") +
                                                 Director->GetElementDebugSummary()));
    }
}

void ULoongCombatWidget::NativeDestruct()
{
    RestoreElementTestSettings();
    Super::NativeDestruct();
}
