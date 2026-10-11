#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoongBuildingSystemComponent.generated.h"

class UMaterialInterface;

/**
 * Self-contained building-mode controller. Add this component to the player
 * character (or another locally controlled actor) and configure the exposed
 * assets. It intentionally coexists with the legacy blueprint manager.
 */
UCLASS(ClassGroup=(Loong), meta=(BlueprintSpawnableComponent))
class LOONG_API ULoongBuildingSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULoongBuildingSystemComponent();

	/** Building actor class used for both the preview and the final placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Assets")
	TSubclassOf<AActor> BuildingClass;

	/** Ground actor whose bounds define the playable build area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Bounds")
	TObjectPtr<AActor> BaseGroundActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Assets")
	TObjectPtr<UMaterialInterface> PreviewValidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Assets")
	TObjectPtr<UMaterialInterface> PreviewInvalidMaterial;

	/**
	 * Trace channel selected in Project Settings > Collision > Trace Channels.
	 * The current project maps TraceTypeQuery1 to BuildGroundTrace.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Collision")
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel = TraceTypeQuery1;

	/** Channel used to query existing blocking geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Collision")
	TEnumAsByte<ECollisionChannel> PlacementCollisionChannel = ECC_WorldDynamic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Grid", meta=(ClampMin="1.0"))
	float GridSize = 200.0f;

	/** Half-size of the default 200x200 footprint; increase for multi-cell buildings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Placement", meta=(ClampMin="0.0"))
	FVector2D BuildingFootprintHalfExtent = FVector2D(100.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Placement", meta=(ClampMin="0.0"))
	float BuildingHalfHeight = 100.0f;

	/** Offset from the ground hit to the actor pivot. Set to 100 when the pivot is at the center of a 200cm building. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Placement")
	float BuildingPivotZOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Input")
	bool bEnableDirectKeyboardInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Input")
	bool bStartInBuildMode = false;

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Mode")
	void ToggleBuildingMode();

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Mode")
	void SetBuildingMode(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Loong|Building|Mode")
	bool IsBuildingMode() const { return bBuildingMode; }

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Placement")
	void RotateBuilding();

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Placement")
	bool PlaceBuilding();

	UFUNCTION(BlueprintPure, Category="Loong|Building|Placement")
	bool IsCurrentPlacementValid() const { return bCurrentPlacementValid; }

	UFUNCTION(BlueprintPure, Category="Loong|Building|Placement")
	FVector GetCurrentPlacementLocation() const { return CurrentPlacementLocation; }

	UFUNCTION(BlueprintPure, Category="Loong|Building|Placement")
	FRotator GetCurrentPlacementRotation() const { return CurrentPlacementRotation; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdatePreview();
	void CreatePreviewActor();
	void DestroyPreviewActor();
	void SetPreviewMaterial(UMaterialInterface* Material);
	bool TraceMouseToGround(FHitResult& OutHit) const;
	bool IsPlacementClearFiltered() const;
	bool IsInsideBaseBounds() const;
	ECollisionChannel ResolveGroundTraceChannel() const;
	void ProcessDirectKeyboardInput();

	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;

	FVector CurrentPlacementLocation = FVector::ZeroVector;
	FRotator CurrentPlacementRotation = FRotator::ZeroRotator;
	bool bBuildingMode = false;
	bool bCurrentPlacementValid = false;
	bool bPreviousBuildKeyDown = false;
	bool bPreviousRotateKeyDown = false;
	bool bPreviousPlaceKeyDown = false;
};
