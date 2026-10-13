#include "LoongBaseSaveWidget.h"
#include "LoongBaseSaveSubsystem.h"
#include "LoongBuildingSystemComponent.h"
#include "LoongBuildingMenuButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"

void ULoongBaseSaveWidget::InitializePanel(ULoongBuildingSystemComponent* InSystem)
{
 BuildingSystem = InSystem; BuildPanel(); if(auto* S=Service())bCachedHasSave=S->HasSave(); RefreshStatus();
}
ULoongBaseSaveSubsystem* ULoongBaseSaveWidget::Service() const
{
 if (!BuildingSystem.IsValid() || !BuildingSystem->GetWorld() || !BuildingSystem->GetWorld()->GetGameInstance()) return nullptr;
 return BuildingSystem->GetWorld()->GetGameInstance()->GetSubsystem<ULoongBaseSaveSubsystem>();
}
void ULoongBaseSaveWidget::BuildPanel()
{
 if (!WidgetTree) return;
 auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
 Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible); WidgetTree->RootWidget = Canvas;
 auto MakePanel = [this,Canvas](float Width,FAnchors Anchors,FVector2D Alignment,FVector2D Position,UWidget*& OutPanel)
 {
  auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(Width);
  auto* Border = WidgetTree->ConstructWidget<UBorder>(); Border->SetBrushColor(FLinearColor(0.025f,0.04f,0.055f,0.95f));Border->SetPadding(FMargin(10));
  auto* List = WidgetTree->ConstructWidget<UVerticalBox>();Border->SetContent(List);Size->SetContent(Border);
  auto* Slot = Canvas->AddChildToCanvas(Size);Slot->SetAutoSize(true);Slot->SetAnchors(Anchors);Slot->SetAlignment(Alignment);Slot->SetPosition(Position);OutPanel=Size;return List;
 };
 UWidget* MainPanel=nullptr;
 auto* Main = MakePanel(300,FAnchors(1,0),FVector2D(1,0),FVector2D(-16,16),MainPanel);Panel=MainPanel;
 auto Label=[this](FText Text){auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(Text);T->SetAutoWrapText(true);return T;};
 Main->AddChildToVerticalBox(Label(FText::FromString(TEXT("基地存档"))));
 StatusText=Label(FText::GetEmpty());Main->AddChildToVerticalBox(StatusText);
 auto Button=[this,Label](const TCHAR* Text){auto* B=WidgetTree->ConstructWidget<ULoongBuildingMenuButton>();B->SetContent(Label(FText::FromString(Text)));return B;};
 auto* FirstRow=WidgetTree->ConstructWidget<UHorizontalBox>();Main->AddChildToVerticalBox(FirstRow);
 auto* Save=Button(TEXT("保存"));FirstRow->AddChildToHorizontalBox(Save);Save->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::SaveClicked);
 auto* Load=Button(TEXT("读档"));FirstRow->AddChildToHorizontalBox(Load);Load->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::LoadClicked);
 auto* Status=Button(TEXT("刷新状态"));FirstRow->AddChildToHorizontalBox(Status);Status->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::StatusClicked);
 auto* SecondRow=WidgetTree->ConstructWidget<UHorizontalBox>();Main->AddChildToVerticalBox(SecondRow);
 auto* Clear=Button(TEXT("清空基地…"));SecondRow->AddChildToHorizontalBox(Clear);Clear->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::ClearClicked);
 auto* Delete=Button(TEXT("删除存档…"));SecondRow->AddChildToHorizontalBox(Delete);Delete->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::DeleteClicked);
 ActionButtons={Save,Load,Clear,Delete};
 UWidget* ModalPanel=nullptr;
 auto* Modal=MakePanel(380,FAnchors(0.5f,0.5f),FVector2D(0.5f,0.5f),FVector2D::ZeroVector,ModalPanel);Confirmation=ModalPanel;
 PromptText=Label(FText::GetEmpty());Modal->AddChildToVerticalBox(PromptText);
 auto* Confirm=Button(TEXT("确认"));Modal->AddChildToVerticalBox(Confirm);Confirm->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::ConfirmClicked);
 auto* Cancel=Button(TEXT("取消（Esc）"));Modal->AddChildToVerticalBox(Cancel);Cancel->OnClicked.AddDynamic(this,&ULoongBaseSaveWidget::CancelClicked);
 Confirmation->SetVisibility(ESlateVisibility::Collapsed);
}
void ULoongBaseSaveWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);
 RefreshElapsed+=DeltaTime;
 if(RefreshElapsed>=0.5f){RefreshElapsed=0.f;if(auto* S=Service())bCachedHasSave=S->HasSave();RefreshStatus();}
}
void ULoongBaseSaveWidget::RefreshStatus()
{
 auto* S=Service();if(!StatusText || !S) return;
 if(LastServiceStatus!=S->LastStatus)
 {
  LastServiceStatus=S->LastStatus;
  if(S->LastStatus.Contains(TEXT("Base saved successfully"))) Feedback=FText::FromString(TEXT("保存成功"));
  else if(S->LastStatus.Contains(TEXT("Base restored successfully"))) Feedback=FText::FromString(TEXT("读档成功"));
  else if(S->LastStatus.Contains(TEXT("No save exists"))) Feedback=FText::FromString(TEXT("尚无存档，当前基地保持不变"));
  else if(S->LastStatus.Contains(TEXT("No automatic load"))) Feedback=FText::FromString(TEXT("自动保存已启用，建造后会记录布局"));
  else Feedback=FText::FromString(TEXT("存档服务：")+S->LastStatus);
 }
 const bool Busy=BuildingSystem.IsValid() && BuildingSystem->HasPendingBuildingEdit();
 for(const auto& B:ActionButtons) if(B) B->SetIsEnabled(!Busy && !IsConfirmationOpen());
 StatusText->SetText(FText::FromString(FString::Printf(TEXT("存档槽：%s\n磁盘存档：%s\n%s%s"),*S->SaveSlotName,bCachedHasSave?TEXT("存在"):TEXT("暂无"),*Feedback.ToString(),Busy?TEXT("\n请先确认或取消当前建筑编辑"):TEXT(""))));
}
void ULoongBaseSaveWidget::RequestAction(FName Action)
{
 if(!BuildingSystem.IsValid() || BuildingSystem->HasPendingBuildingEdit() || IsConfirmationOpen()) return;
 BuildingSystem->SuppressSceneClickForUI();
 PendingAction=Action;
 const TCHAR* Message=Action==TEXT("Load")?TEXT("读档将用磁盘布局替换当前托管建筑。确定继续？"):Action==TEXT("Clear")?TEXT("清空当前基地建筑？磁盘存档会保留；后续保存会写入新布局。"):TEXT("删除磁盘存档及备份？当前场景建筑会保留，此操作无法撤销。");
 PromptText->SetText(FText::FromString(Message));Confirmation->SetVisibility(ESlateVisibility::Visible);RefreshStatus();
}
void ULoongBaseSaveWidget::CancelConfirmation()
{
 if(BuildingSystem.IsValid())BuildingSystem->SuppressSceneClickForUI();
 PendingAction=NAME_None;if(Confirmation)Confirmation->SetVisibility(ESlateVisibility::Collapsed);RefreshStatus();
}
void ULoongBaseSaveWidget::SaveClicked()
{
 auto* S=Service();if(!S || !BuildingSystem.IsValid() || BuildingSystem->HasPendingBuildingEdit()) return;
 BuildingSystem->SuppressSceneClickForUI();
 bool Ok=S->SaveBase();bCachedHasSave=S->HasSave();LastServiceStatus=S->LastStatus;
 Feedback=Ok?FText::FromString(TEXT("保存成功")):FText::FromString(TEXT("保存失败：")+S->LastStatus);RefreshStatus();
}
void ULoongBaseSaveWidget::LoadClicked(){RequestAction(TEXT("Load"));}
void ULoongBaseSaveWidget::ClearClicked(){RequestAction(TEXT("Clear"));}
void ULoongBaseSaveWidget::DeleteClicked(){RequestAction(TEXT("Delete"));}
void ULoongBaseSaveWidget::StatusClicked(){if(BuildingSystem.IsValid())BuildingSystem->SuppressSceneClickForUI();if(auto* S=Service())bCachedHasSave=S->HasSave();RefreshStatus();}
void ULoongBaseSaveWidget::ConfirmClicked()
{
 auto* S=Service();if(!S || !IsConfirmationOpen()) return;
 const FName Action=PendingAction;CancelConfirmation();
 bool Ok=false;
 if(Action==TEXT("Load"))Ok=S->LoadBase();
 else if(Action==TEXT("Clear"))Ok=S->NewBase(true);
 else if(Action==TEXT("Delete"))Ok=S->DeleteSave(true);
 LastServiceStatus=S->LastStatus;bCachedHasSave=S->HasSave();
 if(!Ok)Feedback=FText::FromString(TEXT("操作失败：")+S->LastStatus);
 else Feedback=FText::FromString(Action==TEXT("Load")?TEXT("读档成功"):Action==TEXT("Clear")?TEXT("基地已清空，磁盘存档仍保留"):TEXT("存档已删除，场景建筑保留"));
 RefreshStatus();
}
void ULoongBaseSaveWidget::CancelClicked(){CancelConfirmation();}
bool ULoongBaseSaveWidget::IsPointerOverPanel() const
{
 if(IsConfirmationOpen())return true;
 return Panel && FSlateApplication::IsInitialized() && Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}
