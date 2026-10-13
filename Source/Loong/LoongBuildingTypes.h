#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LoongBuildingTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class ELoongBuildingCategory : uint8
{
    Housing,
    Production,
    Utility,
    Decoration,
    Other
};

USTRUCT(BlueprintType)
struct LOONG_API FLoongBuildingCostEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Cost")
    FName ResourceId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Cost", meta = (ClampMin = "0"))
    int32 Amount = 0;
};

/** Data-driven definition shared by preview, validation and final spawning. */
USTRUCT(BlueprintType)
struct LOONG_API FLoongBuildingDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName BuildingId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation", meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
    bool bUnlocked = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footprint", meta = (ClampMin = "1"))
    int32 GridWidth = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footprint", meta = (ClampMin = "1"))
    int32 GridHeight = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footprint", meta = (ClampMin = "1.0"))
    float BuildingHeight = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TSubclassOf<AActor> BuildingClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TObjectPtr<UStaticMesh> BuildingMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TObjectPtr<UStaticMesh> PreviewMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TObjectPtr<UMaterialInterface> PreviewValidMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TObjectPtr<UMaterialInterface> PreviewInvalidMaterial;

    /** Runtime-fit a single configured static mesh to the logical grid footprint and height. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    bool bAutoFitMeshToFootprint = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classification")
    ELoongBuildingCategory BuildingCategory = ELoongBuildingCategory::Other;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    TArray<FLoongBuildingCostEntry> BuildingCost;

    bool IsUsable() const
    {
        return !BuildingId.IsNone() && GridWidth > 0 && GridHeight > 0 && BuildingHeight > 0.0f;
    }
};

/** Optional asset container for future content-browser driven building catalogs. */
UCLASS(BlueprintType)
class LOONG_API ULoongBuildingDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
    FLoongBuildingDefinition Definition;
};
