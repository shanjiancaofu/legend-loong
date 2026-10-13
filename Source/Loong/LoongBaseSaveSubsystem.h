#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoongBaseSaveGame.h"
#include "LoongBaseSaveSubsystem.generated.h"
class ULoongBuildingSystemComponent;

/** Persistence service, never a second construction manager. */
UCLASS()
class LOONG_API ULoongBaseSaveSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Save") FString SaveSlotName = TEXT("BaseSave_01");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Save") int32 UserIndex = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Save") bool bAutoSave = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Save") bool bAutoLoad = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Save", meta=(ClampMin="0.01")) float AutoSaveDelay = 0.5f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loong|Save") FString LastStatus;
 void RegisterBuildingSystem(ULoongBuildingSystemComponent* System);
 void UnregisterBuildingSystem(ULoongBuildingSystemComponent* System);
 virtual void Deinitialize() override;
 UFUNCTION(BlueprintCallable, Category="Loong|Save") bool SaveBase();
 UFUNCTION(BlueprintCallable, Category="Loong|Save") bool LoadBase();
 UFUNCTION(BlueprintPure, Category="Loong|Save") bool HasSave() const;
 UFUNCTION(BlueprintCallable, Category="Loong|Save") bool DeleteSave(bool bExplicitlyConfirmed = false);
 /** Clear runtime managed buildings. Does not delete or overwrite a disk save. */
 UFUNCTION(BlueprintCallable, Category="Loong|Save") bool NewBase(bool bExplicitlyConfirmed = false);
 UFUNCTION(BlueprintCallable, Category="Loong|Save") bool ClearBase(bool bExplicitlyConfirmed = false) { return NewBase(bExplicitlyConfirmed); }
 bool ValidateSave(const ULoongBaseSaveGame* Save, FString& Error) const;
private:
 UFUNCTION() void BuildingPlaced(FGuid InstanceId, FName BuildingId, FTransform Transform);
 UFUNCTION() void BuildingMoved(FGuid InstanceId, FName BuildingId, FTransform Transform);
 UFUNCTION() void BuildingDemolished(FGuid InstanceId, FName BuildingId, FTransform Transform);
 void QueueSave();
 void ClearTimer();
 void FlushPending();
 bool SetError(const FString& Error, bool bProtectDisk = false);
 FString CurrentMap() const;
 bool SlotValid() const;
 TWeakObjectPtr<ULoongBuildingSystemComponent> BuildingSystem;
 TWeakObjectPtr<UWorld> TimerWorld;
 FTimerHandle SaveTimer;
 UPROPERTY(Transient) TArray<FLoongSavedBuilding> CommittedBuildings;
 FString RegisteredMap;
 FString RegisteredSlot;
 bool bPending = false;
 bool bRestoring = false;
 bool bProtectExistingSave = false;
};
