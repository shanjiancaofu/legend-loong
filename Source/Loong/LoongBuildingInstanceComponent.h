#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoongBuildingInstanceComponent.generated.h"

/** Runtime identity attached to buildings created by the C++ building system. */
UCLASS(ClassGroup=(Loong), meta=(BlueprintSpawnableComponent))
class LOONG_API ULoongBuildingInstanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loong|Building")
	FName BuildingId = NAME_None;
};
