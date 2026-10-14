#if WITH_DEV_AUTOMATION_TESTS
#include "LoongAutomationTestWorld.h"
#include "../LoongBaseSaveSubsystem.h"
#include "../LoongBuildingSystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FScopedBaseSave
{
    LoongTests::FScopedWorld Fixture;
    TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
    TStrongObjectPtr<ULoongBaseSaveSubsystem> Service{NewObject<ULoongBaseSaveSubsystem>(Instance.Get())};
    ULoongBuildingSystemComponent* System = nullptr;
    const FString Slot = TEXT("Loong_QA_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);

    FScopedBaseSave()
    {
        Service->SaveSlotName = Slot;
        Service->bAutoSave = false;
        Service->bAutoLoad = false;
        auto* Owner = Fixture.World->SpawnActor<AActor>();
        System = NewObject<ULoongBuildingSystemComponent>(Owner);
        System->bEnableBasePersistence = false;
        System->BaseIdentifierOverride = TEXT("QA_IsolatedBase");
        Owner->AddInstanceComponent(System);
        System->RegisterComponent();
        auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        for (auto& Definition : System->BuildingDefinitions)
            Definition.BuildingMesh = Cube;
    }

    ~FScopedBaseSave()
    {
        Service->UnregisterBuildingSystem(System);
        for (const FString& Suffix : {FString(), FString(TEXT("_Backup")), FString(TEXT("_Integrity"))})
            UGameplayStatics::DeleteGameInSlot(Slot + Suffix, 0);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBuildingStateSaveTest,
                                 "Loong.Building.Persistence.ExtensionStateAndCleanup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongBuildingStateSaveTest::RunTest(const FString& Parameters)
{
    FString Slot;
    {
        FScopedBaseSave Save;
        Slot = Save.Slot;
        FLoongSavedBuilding Record;
        Record.InstanceGuid = FGuid::NewGuid();
        Record.BuildingId = Save.System->BuildingDefinitions[0].BuildingId;
        Record.Transform = FTransform(FRotator(0, 90, 0), FVector(-400, 200, 0));
        Record.State.Values.Add(TEXT("QA_Progress"), TEXT("17"));
        Record.State.Values.Add(TEXT("QA_Label"), TEXT("测试扩展状态"));
        if (!TestTrue(TEXT("Isolated DTO creates managed building"), Save.System->RestoreBuildings({Record})))
            return false;
        Save.Service->RegisterBuildingSystem(Save.System);
        TestFalse(TEXT("Unique slot has no previous file"), Save.Service->HasSave());
        if (!TestTrue(TEXT("Actual disk save succeeds"), Save.Service->SaveBase()))
            return false;
        auto* Disk = Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
        if (!TestNotNull(TEXT("Saved bytes deserialize"), Disk))
            return false;
        TestEqual(TEXT("Saved base identifier"), Disk->MapIdentifier, FString(TEXT("QA_IsolatedBase")));
        TestEqual(TEXT("Extension value is present on disk"),
                  Disk->Buildings[0].State.Values.FindRef(TEXT("QA_Progress")), FString(TEXT("17")));
        Save.System->RestoreBuildings({});
        TestTrue(TEXT("Load restores a cleared runtime base"), Save.Service->LoadBase());
        TArray<FLoongSavedBuilding> Restored;
        Save.System->ExportCommittedBuildings(Restored);
        if (TestEqual(TEXT("Restore yields exactly one building"), Restored.Num(), 1))
        {
            TestEqual(TEXT("Stable GUID preserved"), Restored[0].InstanceGuid, Record.InstanceGuid);
            TestTrue(TEXT("Negative position and rotation preserved"),
                     Restored[0].Transform.Equals(Record.Transform));
            TestEqual(TEXT("Unicode extension payload preserved"),
                      Restored[0].State.Values.FindRef(TEXT("QA_Label")), FString(TEXT("测试扩展状态")));
        }
        Save.Service->LoadBase();
        Save.System->ExportCommittedBuildings(Restored);
        TestEqual(TEXT("Repeated load cannot duplicate buildings"), Restored.Num(), 1);
    }
    for (const FString& Suffix : {FString(), FString(TEXT("_Backup")), FString(TEXT("_Integrity"))})
        TestFalse(TEXT("RAII deletes only this test's allocated slots"),
                  UGameplayStatics::DoesSaveGameExist(Slot + Suffix, 0));
    return true;
}
#endif
