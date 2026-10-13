#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LoongBaseSaveGame.generated.h"

/** Versioned extension payload: values only, never runtime object references. */
USTRUCT(BlueprintType)
struct LOONG_API FLoongSavedBuildingState
{
 GENERATED_BODY()
 UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) int32 StateVersion = 1;
 UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) TMap<FName, FString> Values;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongSavedBuilding
{
 GENERATED_BODY()
 UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FGuid InstanceGuid;
 UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FName BuildingId;
 UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FTransform Transform;
 UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FLoongSavedBuildingState State;
};
UCLASS()
class LOONG_API ULoongBaseSaveGame : public USaveGame
{
 GENERATED_BODY()
public:
 static constexpr int32 CurrentSaveVersion = 1;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) int32 SaveVersion = CurrentSaveVersion;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) FString SaveSlotName;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) FString MapIdentifier;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) TArray<FLoongSavedBuilding> Buildings;
};
