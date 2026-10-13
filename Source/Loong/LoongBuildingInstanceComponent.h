#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoongBaseSaveGame.h"
#include "LoongBuildingInstanceComponent.generated.h"

class ULoongBuildingSystemComponent;

/** Runtime identity attached to buildings created by the C++ building system. */
UCLASS(ClassGroup=(Loong), meta=(BlueprintSpawnableComponent))
class LOONG_API ULoongBuildingInstanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loong|Building") FGuid InstanceId;
 UPROPERTY(Transient) TWeakObjectPtr<ULoongBuildingSystemComponent> ManagingSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loong|Building")
	FName BuildingId = NAME_None;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FLoongSavedBuildingState PersistentState;
 UPROPERTY(Transient) FString OwningBaseIdentifier;
};
