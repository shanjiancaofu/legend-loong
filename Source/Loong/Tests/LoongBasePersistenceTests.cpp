#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongBaseSaveSubsystem.h"
#include "../LoongBuildingSystemComponent.h"
#include "../LoongBuildingInstanceComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBasePersistenceTest, "Loong.Building.Persistence.RoundTripAndFailures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongBasePersistenceTest::RunTest(const FString& Parameters)
{
 const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("LoongSaveTest")), GetTransientPackage(), true, ERHIFeatureLevel::SM6, &Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 UGameInstance* GI = NewObject<UGameInstance>();
 auto* Service = NewObject<ULoongBaseSaveSubsystem>(GI);
 Service->SaveSlotName = TEXT("Loong_Automation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
 auto WriteTestSave = [&](USaveGame* Save)
 {
  TArray<uint8> Bytes, Digest; UGameplayStatics::SaveGameToMemory(Save,Bytes);Digest.SetNumUninitialized(20);FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest.GetData());
  UGameplayStatics::SaveDataToSlot(Bytes,Service->SaveSlotName,0);UGameplayStatics::SaveDataToSlot(Digest,Service->SaveSlotName + TEXT("_Integrity"),0);
 };
 Service->bAutoSave = false; Service->bAutoLoad = false;
 AActor* Owner = World->SpawnActor<AActor>();
 auto* System = NewObject<ULoongBuildingSystemComponent>(Owner); Owner->AddInstanceComponent(System); System->RegisterComponent();
 System->bEnableBasePersistence = false; System->BaseIdentifierOverride = TEXT("AutomationBase");
 auto* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 for (auto& D : System->BuildingDefinitions) D.BuildingMesh = Cube;
 Service->RegisterBuildingSystem(System);
 TestFalse(TEXT("Fresh unique test slot has no save"), Service->HasSave());
 TestFalse(TEXT("No save does not clear live world"), Service->LoadBase());
 TestTrue(TEXT("Empty base saves"), Service->SaveBase());
 TestTrue(TEXT("Empty base restores"), Service->LoadBase());
 auto Make = [&](int32 Index, FVector Location, float Yaw)
 { System->SelectedBuildingIndex = Index; return System->SpawnBuildingActor(FTransform(FRotator(0,Yaw,0),Location),false); };
 AActor* First = Make(0,FVector(0,0,0),0);
 const FGuid FirstGuid = First->FindComponentByClass<ULoongBuildingInstanceComponent>()->InstanceId;
 TestTrue(TEXT("One building saves"), Service->SaveBase());
 TestTrue(TEXT("One building restores"), Service->LoadBase());
 TArray<FLoongSavedBuilding> Records; System->ExportCommittedBuildings(Records);
 TestEqual(TEXT("One building count"),Records.Num(),1);
 TestEqual(TEXT("Building ID retained"),Records[0].BuildingId,FName(TEXT("SmallDragonHouse")));
 TestEqual(TEXT("GUID retained"),Records[0].InstanceGuid,FirstGuid);
 Make(1,FVector(1000,1000,0),90); Make(2,FVector(2000,2000,0),180); Make(3,FVector(3000,3000,0),270);
 System->ExportCommittedBuildings(Records); const auto Original = Records;
 TestTrue(TEXT("Four sizes save"),Service->SaveBase());
 TestTrue(TEXT("Four sizes load"),Service->LoadBase());
 TestTrue(TEXT("Repeat load succeeds"),Service->LoadBase());
 System->ExportCommittedBuildings(Records); TestEqual(TEXT("Repeat load has exactly four"),Records.Num(),4);
 for (const auto& R : Original)
 {
  const auto* Found = Records.FindByPredicate([&](const FLoongSavedBuilding& Other){return R.InstanceGuid == Other.InstanceGuid;});
  TestTrue(TEXT("All transforms and IDs round trip"),Found && Found->Transform.Equals(R.Transform) && Found->BuildingId == R.BuildingId);
 }
 auto Find = [&](FGuid Guid)->AActor*
 { for(TActorIterator<AActor> It(World);It;++It) if(auto* I=It->FindComponentByClass<ULoongBuildingInstanceComponent>()) if(!It->IsActorBeingDestroyed() && I->InstanceId==Guid) return *It; return nullptr; };
 First = Find(FirstGuid); System->SelectBuildingActor(First); System->BeginMoveSelectedBuilding();
 First->SetActorLocation(FVector(800,400,0));
 TestTrue(TEXT("Save during unconfirmed move writes committed layout"),Service->SaveBase());
 auto* Saved = Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Service->SaveSlotName,0));
 const auto* OldRecord = Saved->Buildings.FindByPredicate([&](const FLoongSavedBuilding& R){return R.InstanceGuid==FirstGuid;});
 TestTrue(TEXT("Unconfirmed move not persisted"),OldRecord && OldRecord->Transform.GetLocation().Equals(FVector::ZeroVector));
 System->CancelMoveBuilding(); TestTrue(TEXT("Cancel returns original transform"),First->GetActorLocation().Equals(FVector::ZeroVector));
 System->BeginMoveSelectedBuilding(); First->SetActorLocation(FVector(800,400,0)); System->FinishMoveBuilding(true);
 Service->SaveBase(); Service->LoadBase(); First=Find(FirstGuid);
 TestTrue(TEXT("Confirmed move persists new transform"),First && First->GetActorLocation().Equals(FVector(800,400,0)));
 System->SelectBuildingActor(First); System->RequestDeleteSelectedBuilding(); System->CancelDeletePrompt(); Service->SaveBase();
 TestTrue(TEXT("Cancel demolition preserves saved record"),Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Service->SaveSlotName,0))->Buildings.Num()==4);
 System->RequestDeleteSelectedBuilding(); System->ConfirmDeleteSelectedBuilding(); Service->SaveBase(); Service->LoadBase();
 System->ExportCommittedBuildings(Records); TestEqual(TEXT("Demolition persists three buildings"),Records.Num(),3); TestNull(TEXT("Deleted GUID does not return"),Find(FirstGuid));
 auto* Good = Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Service->SaveSlotName,0));
 FString Error; auto* Bad = DuplicateObject<ULoongBaseSaveGame>(Good,GetTransientPackage());
 Bad->Buildings[0].BuildingId=TEXT("MissingType"); TestFalse(TEXT("Missing definition rejected"),Service->ValidateSave(Bad,Error));
 Bad=DuplicateObject<ULoongBaseSaveGame>(Good,GetTransientPackage()); const FLoongSavedBuilding Duplicate = Bad->Buildings[0]; Bad->Buildings.Add(Duplicate); TestFalse(TEXT("Duplicate GUID rejected"),Service->ValidateSave(Bad,Error));
 Bad=DuplicateObject<ULoongBaseSaveGame>(Good,GetTransientPackage()); Bad->SaveVersion=999; TestFalse(TEXT("Future version rejected"),Service->ValidateSave(Bad,Error));
 Bad=DuplicateObject<ULoongBaseSaveGame>(Good,GetTransientPackage()); Bad->MapIdentifier=TEXT("OtherMap"); TestFalse(TEXT("Wrong map rejected"),Service->ValidateSave(Bad,Error));
 WriteTestSave(Bad);
 AddExpectedError(TEXT("Map/base identifier or slot mismatch"),EAutomationExpectedErrorFlags::Contains,1);
 TestFalse(TEXT("Bad load fails before clearing"),Service->LoadBase());
 System->ExportCommittedBuildings(Records);TestEqual(TEXT("Bad load retains current buildings"),Records.Num(),3);
 AddExpectedError(TEXT("Save blocked"),EAutomationExpectedErrorFlags::Contains,1); TestFalse(TEXT("Failed load protects disk from writeback"),Service->SaveBase());
 TestEqual(TEXT("Protected file unchanged"),Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Service->SaveSlotName,0))->MapIdentifier,FString(TEXT("OtherMap")));
 WriteTestSave(Good);TestTrue(TEXT("Valid reload removes protection"),Service->LoadBase());
 TestFalse(TEXT("Unreadable object rejected"),Service->ValidateSave(nullptr,Error));
 UStaticMesh* PreviousMesh = System->BuildingDefinitions[2].BuildingMesh;
 System->BuildingDefinitions[2].BuildingMesh = nullptr;
 AddExpectedError(TEXT("Staged restore failed"), EAutomationExpectedErrorFlags::Contains, 1);
 TestFalse(TEXT("Partial staging failure safely rolls back"), Service->LoadBase());
 System->ExportCommittedBuildings(Records); TestEqual(TEXT("Rollback preserves current three"),Records.Num(),3);
 System->BuildingDefinitions[2].BuildingMesh = PreviousMesh; Service->LoadBase();

 const TArray<uint8> CorruptBytes = {0,1,2,3}; UGameplayStatics::SaveDataToSlot(CorruptBytes,Service->SaveSlotName,0);
 AddExpectedError(TEXT("Unreadable save"),EAutomationExpectedErrorFlags::Contains,1);
 TestFalse(TEXT("Corrupt binary load retains current base"),Service->LoadBase());
 System->ExportCommittedBuildings(Records);TestEqual(TEXT("Corrupt binary retains three buildings"),Records.Num(),3);
 WriteTestSave(Good);Service->LoadBase();
 AddExpectedError(TEXT("explicit confirmation"),EAutomationExpectedErrorFlags::Contains,2);
 TestFalse(TEXT("Delete requires confirmation"),Service->DeleteSave(false));TestFalse(TEXT("Clear requires confirmation"),Service->NewBase(false));
 TestTrue(TEXT("Confirmed clear runtime base"),Service->NewBase(true));TestTrue(TEXT("Clear keeps disk save"),Service->HasSave());
 Service->LoadBase();System->ExportCommittedBuildings(Records);TestEqual(TEXT("Clear then load recovers disk layout"),Records.Num(),3);
 Service->bAutoSave = true; Service->AutoSaveDelay = 0.05f;
 AActor* Auto = Make(0,FVector(4000,4000,0),0);
 const FGuid AutoGuid = Auto->FindComponentByClass<ULoongBuildingInstanceComponent>()->InstanceId;
 World->GetTimerManager().Tick(0.01f);
 ++GFrameCounter;
 World->GetTimerManager().Tick(0.1f);
 auto* AutoSaved = Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Service->SaveSlotName,0));
 TestTrue(TEXT("Debounced confirmed spawn automatically saves"), AutoSaved && AutoSaved->Buildings.Num()==4);
 System->SelectBuildingActor(Auto);System->BeginMoveSelectedBuilding();Auto->SetActorLocation(FVector(4400,4400,0));System->FinishMoveBuilding(true);
 Service->UnregisterBuildingSystem(System);
 auto* ExitSaved = Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Service->SaveSlotName,0));
 const auto* ExitRecord = ExitSaved->Buildings.FindByPredicate([&](const FLoongSavedBuilding& R){return R.InstanceGuid==AutoGuid;});
 TestTrue(TEXT("Unregister flushes pending confirmed move"),ExitRecord && ExitRecord->Transform.GetLocation().Equals(FVector(4400,4400,0)));
 AActor* NextOwner=World->SpawnActor<AActor>();auto* Next=NewObject<ULoongBuildingSystemComponent>(NextOwner);NextOwner->AddInstanceComponent(Next);Next->RegisterComponent();Next->BaseIdentifierOverride=TEXT("AutomationBase");Next->BuildingDefinitions=System->BuildingDefinitions;
 Service->RegisterBuildingSystem(Next);Next->ExportCommittedBuildings(Records);TestEqual(TEXT("Respawn rebind has no duplicates"),Records.Num(),4);
 const FString PrimaryTestSlot = Service->SaveSlotName;
 Service->NewBase(true);
 Service->SaveSlotName = PrimaryTestSlot + TEXT("_WriteFailure");
 const FString BlockedPath = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),Service->SaveSlotName + TEXT(".sav"));
 // This newly allocated test-only directory forces the platform save backend to fail opening a file.
 IFileManager::Get().MakeDirectory(*BlockedPath,true);
 AddExpectedError(TEXT("Disk write failed"),EAutomationExpectedErrorFlags::Contains,1);
 TestFalse(TEXT("Actual disk write failure returns false"),Service->SaveBase());
 TestTrue(TEXT("Failure status explicitly reported"),Service->LastStatus.Contains(TEXT("Disk write failed")));
 IFileManager::Get().DeleteDirectory(*BlockedPath,false,false);
 UGameplayStatics::DeleteGameInSlot(Service->SaveSlotName + TEXT("_Integrity"),0);
 Service->SaveSlotName = PrimaryTestSlot;
 TestTrue(TEXT("Explicit delete file"),Service->DeleteSave(true));TestFalse(TEXT("File removed"),Service->HasSave());
 Service->UnregisterBuildingSystem(Next); Next->bEndingPlay=true;Next->ClearSelectedBuilding();System->bEndingPlay=true;System->ClearSelectedBuilding();
 GEngine->DestroyWorldContext(World);World->DestroyWorld(false);World->RemoveFromRoot();
 UGameplayStatics::DeleteGameInSlot(Service->SaveSlotName,0);
 UGameplayStatics::DeleteGameInSlot(Service->SaveSlotName + TEXT("_Backup"),0);
 UGameplayStatics::DeleteGameInSlot(Service->SaveSlotName + TEXT("_Integrity"),0);
 return true;
}
#endif
