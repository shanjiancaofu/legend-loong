#include "LoongBaseSaveSubsystem.h"
#include "LoongBuildingSystemComponent.h"
#include "LoongBuildingInstanceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/SecureHash.h"

bool ULoongBaseSaveSubsystem::SlotValid() const
{
    if (SaveSlotName.IsEmpty() || SaveSlotName.Len() > 128 || UserIndex < 0)
        return false;
    for (TCHAR C : SaveSlotName)
        if (!FChar::IsAlnum(C) && C != TEXT('_') && C != TEXT('-'))
            return false;
    return true;
}

FString ULoongBaseSaveSubsystem::CurrentMap() const
{
    return BuildingSystem.IsValid() ? BuildingSystem->GetBaseMapIdentifier() : FString();
}

bool ULoongBaseSaveSubsystem::SetError(const FString& Error, bool bProtectDisk)
{
    LastStatus = Error;
    bProtectExistingSave |= bProtectDisk;
    UE_LOG(LogTemp, Error, TEXT("Loong Save: %s"), *Error);
    return false;
}

void ULoongBaseSaveSubsystem::RegisterBuildingSystem(ULoongBuildingSystemComponent* System)
{
    if (!IsValid(System) || BuildingSystem.Get() == System)
        return;
    if (BuildingSystem.IsValid() && BuildingSystem->GetWorld() == System->GetWorld())
    {
        SetError(TEXT("A second building component tried to register in the same world."));
        return;
    }
    if (BuildingSystem.IsValid())
        UnregisterBuildingSystem(BuildingSystem.Get());
    const bool bSameSession =
        RegisteredMap == System->GetBaseMapIdentifier() && RegisteredSlot == SaveSlotName;
    BuildingSystem = System;
    // A respawn transfers only explicit managed instance identities, never guesses old BP buildings.
    for (TActorIterator<AActor> It(System->GetWorld()); It; ++It)
        if (auto* I = It->FindComponentByClass<ULoongBuildingInstanceComponent>())
            if (!I->ManagingSystem.IsValid() && I->OwningBaseIdentifier == System->GetBaseMapIdentifier() &&
                I->InstanceId.IsValid() && System->HasBuildingDefinition(I->BuildingId))
                I->ManagingSystem = System;
    System->OnBuildingPlaced.AddDynamic(this, &ULoongBaseSaveSubsystem::BuildingPlaced);
    System->OnBuildingMoved.AddDynamic(this, &ULoongBaseSaveSubsystem::BuildingMoved);
    System->OnBuildingDemolished.AddDynamic(this, &ULoongBaseSaveSubsystem::BuildingDemolished);
    RegisteredMap = CurrentMap();
    RegisteredSlot = SaveSlotName;
    TArray<FLoongSavedBuilding> Current;
    if (!System->ExportCommittedBuildings(Current))
    {
        SetError(TEXT("Invalid live instance registry."), true);
        return;
    }
    if (bSameSession && Current.Num() > 0)
    {
        CommittedBuildings = Current;
        LastStatus = TEXT("Rebound to existing managed base after respawn.");
        return;
    }
    if (bAutoLoad && HasSave())
    {
        LoadBase();
    }
    else
    {
        CommittedBuildings = Current;
        LastStatus = TEXT("No automatic load; current base retained.");
    }
}

void ULoongBaseSaveSubsystem::ClearTimer()
{
    if (TimerWorld.IsValid())
        TimerWorld->GetTimerManager().ClearTimer(SaveTimer);
    TimerWorld.Reset();
}

void ULoongBaseSaveSubsystem::FlushPending()
{
    ClearTimer();
    if (bPending && !bProtectExistingSave && !bRestoring)
        SaveBase();
}

void ULoongBaseSaveSubsystem::UnregisterBuildingSystem(ULoongBuildingSystemComponent* System)
{
    if (BuildingSystem.Get() != System)
        return;
    FlushPending();
    System->OnBuildingPlaced.RemoveDynamic(this, &ULoongBaseSaveSubsystem::BuildingPlaced);
    System->OnBuildingMoved.RemoveDynamic(this, &ULoongBaseSaveSubsystem::BuildingMoved);
    System->OnBuildingDemolished.RemoveDynamic(this, &ULoongBaseSaveSubsystem::BuildingDemolished);
    for (TActorIterator<AActor> It(System->GetWorld()); It; ++It)
        if (auto* I = It->FindComponentByClass<ULoongBuildingInstanceComponent>())
            if (I->ManagingSystem.Get() == System)
                I->ManagingSystem.Reset();
    BuildingSystem.Reset();
}

void ULoongBaseSaveSubsystem::Deinitialize()
{
    FlushPending();
    ClearTimer();
    BuildingSystem.Reset();
    Super::Deinitialize();
}

bool ULoongBaseSaveSubsystem::HasSave() const
{
    return SlotValid() && UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex);
}

bool ULoongBaseSaveSubsystem::ValidateSave(const ULoongBaseSaveGame* Save, FString& Error) const
{
    if (!BuildingSystem.IsValid() || !Save)
    {
        Error = TEXT("Missing building system or unreadable SaveGame.");
        return false;
    }
    if (Save->SaveVersion != ULoongBaseSaveGame::CurrentSaveVersion)
    {
        Error = TEXT("Unsupported save version; original file preserved.");
        return false;
    }
    if (Save->MapIdentifier != CurrentMap() || Save->SaveSlotName != SaveSlotName)
    {
        Error = TEXT("Map/base identifier or slot mismatch.");
        return false;
    }
    TSet<FGuid> Guids;
    for (const auto& R : Save->Buildings)
    {
        if (!R.InstanceGuid.IsValid() || Guids.Contains(R.InstanceGuid))
        {
            Error = TEXT("Invalid or duplicate instance GUID.");
            return false;
        }
        Guids.Add(R.InstanceGuid);
        if (!BuildingSystem->HasBuildingDefinition(R.BuildingId))
        {
            Error = FString::Printf(TEXT("Missing definition: %s"), *R.BuildingId.ToString());
            return false;
        }
        if (R.Transform.ContainsNaN() || !R.Transform.GetRotation().IsNormalized() ||
            R.Transform.GetScale3D().GetMin() <= KINDA_SMALL_NUMBER || R.State.StateVersion != 1)
        {
            Error = TEXT("Invalid transform, scale or building state version.");
            return false;
        }
    }
    return true;
}

bool ULoongBaseSaveSubsystem::SaveBase()
{
    if (!SlotValid() || !BuildingSystem.IsValid())
        return SetError(TEXT("Cannot save: invalid slot or no active base."));
    if (bRestoring || bProtectExistingSave)
        return SetError(TEXT("Save blocked while loading or protecting a failed load."));
    // Snapshot is updated only by confirmed operation notifications, never by moving previews.
    auto* Save =
        Cast<ULoongBaseSaveGame>(UGameplayStatics::CreateSaveGameObject(ULoongBaseSaveGame::StaticClass()));
    Save->SaveSlotName = SaveSlotName;
    Save->MapIdentifier = CurrentMap();
    Save->Buildings = CommittedBuildings;
    FString Error;
    if (!ValidateSave(Save, Error))
        return SetError(Error, true);
    TArray<uint8> PreviousBytes, PreviousIntegrity;
    const bool bHadPrevious = HasSave();
    if (bHadPrevious)
    {
        UGameplayStatics::LoadDataFromSlot(PreviousIntegrity, SaveSlotName + TEXT("_Integrity"), UserIndex);
        if (!UGameplayStatics::LoadDataFromSlot(PreviousBytes, SaveSlotName, UserIndex))
            return SetError(TEXT("Existing file cannot be backed up; overwrite refused."), true);
        if (!UGameplayStatics::SaveDataToSlot(PreviousBytes, SaveSlotName + TEXT("_Backup"), UserIndex))
            return SetError(TEXT("Backup write failed; primary save untouched."));
    }
    TArray<uint8> SaveBytes;
    if (!UGameplayStatics::SaveGameToMemory(Save, SaveBytes))
        return SetError(TEXT("Save serialization failed."));
    TArray<uint8> Digest;
    Digest.SetNumUninitialized(20);
    FSHA1::HashBuffer(SaveBytes.GetData(), SaveBytes.Num(), Digest.GetData());
    if (!UGameplayStatics::SaveDataToSlot(SaveBytes, SaveSlotName, UserIndex) ||
        !UGameplayStatics::SaveDataToSlot(Digest, SaveSlotName + TEXT("_Integrity"), UserIndex))
    {
        if (bHadPrevious && PreviousIntegrity.Num() == 20)
            UGameplayStatics::SaveDataToSlot(PreviousIntegrity, SaveSlotName + TEXT("_Integrity"), UserIndex);
        if (bHadPrevious && !UGameplayStatics::SaveDataToSlot(PreviousBytes, SaveSlotName, UserIndex))
            UE_LOG(LogTemp, Error,
                   TEXT("Loong Save: restoring previous file failed; use the _Backup slot for recovery."));
        return SetError(TEXT("Disk write failed; pending save retained."));
    }
    bPending = false;
    ClearTimer();
    LastStatus = TEXT("Base saved successfully.");
    UE_LOG(LogTemp, Log, TEXT("Loong Save: %s (%s, %d buildings)"), *LastStatus, *SaveSlotName,
           Save->Buildings.Num());
    return true;
}

bool ULoongBaseSaveSubsystem::LoadBase()
{
    if (!SlotValid() || !BuildingSystem.IsValid())
        return SetError(TEXT("Cannot load: invalid slot or no active base."));
    if (BuildingSystem->HasPendingBuildingEdit())
        return SetError(TEXT("Finish or cancel current edit before loading."));
    if (!HasSave())
    {
        LastStatus = TEXT("No save exists; live base unchanged.");
        return false;
    }
    TArray<uint8> Bytes, StoredDigest, ActualDigest;
    if (!UGameplayStatics::LoadDataFromSlot(Bytes, SaveSlotName, UserIndex) || Bytes.Num() < 32 ||
        !UGameplayStatics::LoadDataFromSlot(StoredDigest, SaveSlotName + TEXT("_Integrity"), UserIndex) ||
        StoredDigest.Num() != 20)
    {
        ClearTimer();
        return SetError(TEXT("Unreadable save or missing integrity data; original preserved."), true);
    }
    ActualDigest.SetNumUninitialized(20);
    FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), ActualDigest.GetData());
    if (ActualDigest != StoredDigest)
    {
        ClearTimer();
        return SetError(TEXT("Save integrity mismatch; corrupt data not deserialized."), true);
    }
    auto* Save = Cast<ULoongBaseSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    FString Error;
    if (!ValidateSave(Save, Error))
    {
        ClearTimer();
        return SetError(Error, true);
    }
    TArray<FLoongSavedBuilding> Current;
    if (!BuildingSystem->ExportCommittedBuildings(Current))
        return SetError(TEXT("Duplicate/invalid live GUIDs; load rejected."), true);
    TGuardValue<bool> Guard(bRestoring, true);
    if (!BuildingSystem->RestoreBuildings(Save->Buildings))
        return SetError(TEXT("Staged restore failed; previous base retained."), true);
    CommittedBuildings = Save->Buildings;
    bPending = false;
    bProtectExistingSave = false;
    ClearTimer();
    LastStatus = TEXT("Base restored successfully without duplicate instances.");
    UE_LOG(LogTemp, Log, TEXT("Loong Save: %s (%d buildings)"), *LastStatus, Save->Buildings.Num());
    return true;
}

bool ULoongBaseSaveSubsystem::DeleteSave(bool bExplicitlyConfirmed)
{
    if (!bExplicitlyConfirmed)
        return SetError(TEXT("DeleteSave requires explicit confirmation."));
    if (!SlotValid() || bRestoring || (BuildingSystem.IsValid() && BuildingSystem->HasPendingBuildingEdit()))
        return SetError(TEXT("Cannot delete save in current state."));
    if (HasSave() && !UGameplayStatics::DeleteGameInSlot(SaveSlotName, UserIndex))
        return SetError(TEXT("Deleting save file failed."));
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName + TEXT("_Integrity"), UserIndex))
        UGameplayStatics::DeleteGameInSlot(SaveSlotName + TEXT("_Integrity"), UserIndex);
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName + TEXT("_Backup"), UserIndex) &&
        !UGameplayStatics::DeleteGameInSlot(SaveSlotName + TEXT("_Backup"), UserIndex))
        return SetError(TEXT("Primary deleted but backup deletion failed."));
    ClearTimer();
    bPending = false;
    bProtectExistingSave = true;
    LastStatus = TEXT(
        "Save deleted. Live buildings retained; automatic overwrite blocked until NewBase or successful LoadBase.");
    return true;
}

bool ULoongBaseSaveSubsystem::NewBase(bool bExplicitlyConfirmed)
{
    if (!bExplicitlyConfirmed || !BuildingSystem.IsValid() || bRestoring)
        return SetError(TEXT("NewBase requires explicit confirmation and an active base."));
    if (!BuildingSystem->ClearManagedBuildings())
        return SetError(TEXT("Cancel current edit before clearing base."));
    ClearTimer();
    CommittedBuildings.Reset();
    bPending = false;
    bProtectExistingSave = false;
    LastStatus = TEXT("Live base cleared. Disk save retained; call SaveBase explicitly to replace it.");
    return true;
}

void ULoongBaseSaveSubsystem::QueueSave()
{
    if (bRestoring || !bAutoSave || bProtectExistingSave || !BuildingSystem.IsValid())
        return;
    bPending = true;
    ClearTimer();
    TimerWorld = BuildingSystem->GetWorld();
    if (TimerWorld.IsValid())
        TimerWorld->GetTimerManager().SetTimer(SaveTimer, this, &ULoongBaseSaveSubsystem::FlushPending,
                                               FMath::Max(0.01f, AutoSaveDelay), false);
}

void ULoongBaseSaveSubsystem::BuildingPlaced(FGuid InstanceId, FName BuildingId, FTransform Transform)
{
    if (bRestoring || !BuildingSystem.IsValid())
        return;
    TArray<FLoongSavedBuilding> Current;
    if (!BuildingSystem->ExportCommittedBuildings(Current))
    {
        SetError(TEXT("Confirmed change contains invalid identities."), true);
        return;
    }
    CommittedBuildings = MoveTemp(Current);
    QueueSave();
}

void ULoongBaseSaveSubsystem::BuildingMoved(FGuid InstanceId, FName BuildingId, FTransform Transform)
{
    BuildingPlaced(InstanceId, BuildingId, Transform);
    for (auto& R : CommittedBuildings)
        if (R.InstanceGuid == InstanceId)
            R.Transform = Transform;
}

void ULoongBaseSaveSubsystem::BuildingDemolished(FGuid InstanceId, FName BuildingId, FTransform Transform)
{
    BuildingPlaced(InstanceId, BuildingId, Transform);
    // Demolition notification occurs before Actor::Destroy, so remove that record explicitly.
    CommittedBuildings.RemoveAll(
        [InstanceId](const FLoongSavedBuilding& R)
        {
            return R.InstanceGuid == InstanceId;
        });
}

namespace
{
void BaseConsole(const TArray<FString>& Args, UWorld* World)
{
    if (!World || !World->GetGameInstance())
        return;
    auto* Service = World->GetGameInstance()->GetSubsystem<ULoongBaseSaveSubsystem>();
    const FString Op = Args.Num() ? Args[0].ToLower() : TEXT("status");
    if (Op == TEXT("save"))
        Service->SaveBase();
    else if (Op == TEXT("load"))
        Service->LoadBase();
    else if (Op == TEXT("delete"))
        Service->DeleteSave(Args.Num() > 1 && Args[1] == TEXT("CONFIRM"));
    else if (Op == TEXT("new"))
        Service->NewBase(Args.Num() > 1 && Args[1] == TEXT("CONFIRM"));
    if (Op == TEXT("status"))
        for (TActorIterator<AActor> It(World); It; ++It)
            if (const auto* I = It->FindComponentByClass<ULoongBuildingInstanceComponent>())
                if (I->ManagingSystem.IsValid() && !It->IsActorBeingDestroyed())
                    UE_LOG(LogTemp, Log, TEXT("Loong.Base building: %s GUID=%s transform=%s"),
                           *I->BuildingId.ToString(), *I->InstanceId.ToString(),
                           *It->GetActorTransform().ToHumanReadableString());
    UE_LOG(LogTemp, Log, TEXT("Loong.Base: slot=%s hasSave=%d status=%s"), *Service->SaveSlotName,
           Service->HasSave(), *Service->LastStatus);
}

FAutoConsoleCommandWithWorldAndArgs BaseCommand(
    TEXT("Loong.Base"), TEXT("status | save | load | new CONFIRM | delete CONFIRM"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BaseConsole));
}
