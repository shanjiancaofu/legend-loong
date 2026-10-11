#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BuildPlacementLibrary.generated.h"

/** Reusable, asset-independent helpers for the building preview and placement flow. */
UCLASS()
class LOONG_API UBuildPlacementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Snap a world position to the configured square grid. Z is preserved. */
	UFUNCTION(BlueprintPure, Category="Loong|Building|Grid")
	static FVector SnapToGrid(FVector WorldLocation, float GridSize = 200.0f);

	/** Normalize yaw to the nearest 90 degree increment. Pitch and roll are preserved. */
	UFUNCTION(BlueprintPure, Category="Loong|Building|Grid")
	static FRotator SnapRotationTo90(FRotator WorldRotation);

	/** Test whether a rotated rectangular footprint is fully inside a rectangular base. */
	UFUNCTION(BlueprintPure, Category="Loong|Building|Placement")
	static bool IsWithinBaseBounds(FVector WorldLocation, FVector2D BaseCenter, FVector2D BaseExtent,
		FVector2D FootprintExtent, float YawDegrees = 0.0f);

	/** Test whether a box footprint overlaps any blocking geometry at the proposed location. */
	UFUNCTION(BlueprintCallable, Category="Loong|Building|Placement", meta=(WorldContext="WorldContextObject"))
	static bool CheckPlacementClear(UObject* WorldContextObject, FVector WorldLocation, FVector BoxExtent,
		FRotator WorldRotation, ECollisionChannel TraceChannel = ECC_WorldDynamic,
		AActor* ActorToIgnore = nullptr);
};
