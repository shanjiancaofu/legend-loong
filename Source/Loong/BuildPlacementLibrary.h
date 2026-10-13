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
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Grid")
    static FVector SnapToGrid(FVector WorldLocation, float GridSize = 200.0f);

    /** Normalize yaw to the nearest 90 degree increment. Pitch and roll are preserved. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Grid")
    static FRotator SnapRotationTo90(FRotator WorldRotation);

    /** Return the width/height in cells after applying quarter-turn rotation. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Grid")
    static FIntPoint GetRotatedGridDimensions(int32 GridWidth, int32 GridHeight, int32 RotationQuarterTurns);

    /** Return the quarter-turn index in the range [0,3] for a snapped yaw. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Grid")
    static int32 GetRotationQuarterTurns(float YawDegrees);

    /** Convert a grid anchor (the first cell center) into the full building center. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Grid")
    static FVector GetBuildingCenterFromGridAnchor(FVector GridAnchor, int32 GridWidth, int32 GridHeight,
                                                   float GridSize, int32 RotationQuarterTurns);

    /** Return half extents in centimeters after applying quarter-turn rotation. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Grid")
    static FVector2D GetBuildingFootprintHalfExtent(int32 GridWidth, int32 GridHeight, float GridSize,
                                                    int32 RotationQuarterTurns);

    /** Test whether a rotated rectangular footprint is fully inside a rectangular base. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Placement")
    static bool IsWithinBaseBounds(FVector WorldLocation, FVector2D BaseCenter, FVector2D BaseExtent,
                                   FVector2D FootprintExtent, float YawDegrees = 0.0f);

    /**
     * Convenience wrapper for Blueprint managers: reads the supplied base Actor's
     * world bounds once per call and reuses IsWithinBaseBounds for the actual test.
     */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Placement")
    static bool IsWithinBaseActorBounds(AActor* BaseGroundActor, FVector WorldLocation,
                                        FVector2D FootprintExtent, float YawDegrees = 0.0f);

    /** Test all four footprint corners against the supplied ground component's local bounds. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Placement")
    static bool IsWithinBaseComponentBounds(UPrimitiveComponent* BaseGroundComponent, FVector WorldLocation,
                                            FVector2D FootprintExtent, float YawDegrees = 0.0f);

    /** Return the Actor location that places its primitive-bounds XY center at DesiredCenter. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Placement")
    static FVector GetActorLocationForCenteredFootprint(AActor* BuildingActor, FVector DesiredCenter,
                                                        FRotator DesiredRotation);

    /** Return true only when two horizontal building footprints penetrate beyond the tolerance. */
    UFUNCTION(BlueprintPure, Category = "Loong|Building|Placement")
    static bool DoBuildingFootprintsOverlap(const AActor* ExistingBuilding, FVector CandidateCenter,
                                            FVector2D CandidateHalfExtent, FRotator CandidateRotation,
                                            float TouchToleranceCm = 1.0f);

    /** Test whether a box footprint overlaps any blocking geometry at the proposed location. */
    UFUNCTION(BlueprintCallable, Category = "Loong|Building|Placement",
              meta = (WorldContext = "WorldContextObject"))
    static bool CheckPlacementClear(UObject* WorldContextObject, FVector WorldLocation, FVector BoxExtent,
                                    FRotator WorldRotation, ECollisionChannel TraceChannel = ECC_WorldDynamic,
                                    AActor* ActorToIgnore = nullptr);
};
