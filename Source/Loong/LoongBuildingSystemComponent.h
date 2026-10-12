#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoongBuildingTypes.h"
#include "LoongBuildingMenuWidget.h"
#include "LoongBuildingConfirmWidget.h"
#include "LoongBuildingSystemComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;
class ULoongBuildingMenuWidget;
class ULoongBuildingConfirmWidget;
class UMeshComponent;

USTRUCT()
struct FLoongSelectedMaterialState
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UMeshComponent> Mesh = nullptr;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Materials;
 UPROPERTY() TObjectPtr<UMaterialInterface> Overlay = nullptr;
 bool bCustomDepth = false;
 int32 Stencil = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLoongBuildingChanged, FGuid, InstanceId, FName, BuildingId, FTransform, Transform);

UENUM(BlueprintType)
enum class ELoongBuildingInteractionState : uint8
{
	Default,
	Selected,
	Building,
	Moving,
	DeleteConfirmation
};

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

	/** Inline definitions for the initial catalog; later these can be moved to data assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Catalog")
	TArray<FLoongBuildingDefinition> BuildingDefinitions;

	/** Currently selected entry in BuildingDefinitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Catalog", meta=(ClampMin="0"))
	int32 SelectedBuildingIndex = 0;

	/** Ground actor whose bounds define the playable build area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Bounds")
	TObjectPtr<AActor> BaseGroundActor;

	/** Optional level Actor tag used to resolve BaseGroundActor once at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Bounds")
	FName BaseGroundActorTag = TEXT("LoongBaseGround");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Assets")
	TObjectPtr<UMaterialInterface> PreviewValidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Assets")
	TObjectPtr<UMaterialInterface> PreviewInvalidMaterial;

	/** Optional selected-building overlay; falls back to PreviewValidMaterial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Selection")
	TObjectPtr<UMaterialInterface> SelectedBuildingMaterial;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loong|Building|Menu")
	TSubclassOf<ULoongBuildingMenuWidget> BuildingMenuWidgetClass;

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Mode")
	void ToggleBuildingMode();

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Mode")
	void SetBuildingMode(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Loong|Building|Mode")
	bool IsBuildingMode() const { return bBuildingMode; }

	UFUNCTION(BlueprintPure, Category="Loong|Building|Mode")
	bool IsMovingBuilding() const { return bMovingBuilding; }
	UFUNCTION(BlueprintPure, Category="Loong|Building|Mode") bool IsDeleteConfirmationOpen() const { return bDeleteConfirmationOpen; }
	UFUNCTION(BlueprintCallable, Category="Loong|Building|Delete") void ConfirmDeleteSelectedBuilding();
	UFUNCTION(BlueprintCallable, Category="Loong|Building|Delete") void CancelDeletePrompt();

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

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Catalog")
	bool SelectBuildingByIndex(int32 DefinitionIndex);

	UFUNCTION(BlueprintCallable, Category="Loong|Building|Catalog")
	bool SelectBuildingById(FName BuildingId);

	UFUNCTION(BlueprintPure, Category="Loong|Building|Catalog")
	FName GetSelectedBuildingId() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
 UFUNCTION(BlueprintPure, Category="Loong|Building") ELoongBuildingInteractionState GetInteractionState() const;
 UFUNCTION(BlueprintPure, Category="Loong|Building") AActor* GetSelectedActor() const { return SelectedBuildingActor; }
 UFUNCTION(BlueprintPure, Category="Loong|Building") FLoongBuildingDefinition GetSelectedDefinition() const;
 UFUNCTION(BlueprintCallable, Category="Loong|Building") void SelectBuildingActor(AActor* Actor);
 UFUNCTION(BlueprintCallable, Category="Loong|Building") void ClearSelectedBuilding();
 UFUNCTION(BlueprintCallable, Category="Loong|Building") void BeginMoveSelectedBuilding();
 UFUNCTION(BlueprintCallable, Category="Loong|Building") bool ConfirmMoveBuilding();
 UFUNCTION(BlueprintCallable, Category="Loong|Building") void CancelMoveBuilding();
 UFUNCTION(BlueprintCallable, Category="Loong|Building") void RequestDeleteSelectedBuilding();
 UPROPERTY(BlueprintAssignable) FLoongBuildingChanged OnBuildingPlaced;
 UPROPERTY(BlueprintAssignable) FLoongBuildingChanged OnBuildingMoved;
 UPROPERTY(BlueprintAssignable) FLoongBuildingChanged OnBuildingDemolished;
private:
 friend class FLoongBuildingInteractionTest;
 void FinishMoveBuilding(bool bConfirm);
 bool IsPointerOverBuildingUI() const;
 void RefreshOperationPanel();
 void NotifyChange(AActor* Actor, FLoongBuildingChanged& Event);
 void UpdatePreview();
	void CreatePreviewActor();
	void DestroyPreviewActor();
	void SetPreviewMaterial(UMaterialInterface* Material);
	AActor* SpawnBuildingActor(const FTransform& SpawnTransform, bool bForPreview);
	void ApplyActiveDefinitionVisuals(AActor* BuildingActor, bool bForPreview);
	void ShowBuildingMenu();
	void HideBuildingMenu();
	void ProcessSelectionInput();

	void ShowDeletePrompt();
	void HideDeletePrompt();



	void UpdateMovingBuilding();
	void CaptureAndSetActorMaterial(AActor* Actor, UMaterialInterface* Material,
		TArray<FLoongSelectedMaterialState>& OutStates);
	void RestoreMaterialStates(TArray<FLoongSelectedMaterialState>& States);
	bool TraceMouseToBuilding(FHitResult& OutHit) const;
	bool IsManagedBuildingActor(const AActor* Actor) const;


	const FLoongBuildingDefinition* GetActiveDefinition() const;
	TSubclassOf<AActor> GetActiveBuildingClass() const;
	FVector2D GetActiveFootprintHalfExtent() const;
	float GetActiveBuildingHalfHeight() const;
	int32 GetActiveGridWidth() const;
	int32 GetActiveGridHeight() const;
	bool TraceMouseToGround(FHitResult& OutHit) const;
	bool IsPlacementClearFiltered() const;
	bool IsInsideBaseBounds() const;
	void ResolveBaseGroundActor();
	ECollisionChannel ResolveGroundTraceChannel() const;
	void ProcessDirectKeyboardInput();

	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<ULoongBuildingMenuWidget> BuildingMenuWidget;
	UPROPERTY(Transient) TObjectPtr<ULoongBuildingConfirmWidget> DeletePromptWidget;
 UPROPERTY(Transient) TObjectPtr<ULoongBuildingConfirmWidget> OperationPanelWidget;
 TWeakObjectPtr<AActor> PendingDeleteActor;
 bool bSuppressSceneClickUntilRelease = false;
 bool bEndingPlay = false;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SelectedBuildingActor;

	UPROPERTY(Transient) TArray<FLoongSelectedMaterialState> SelectedMaterialStates;

	UPROPERTY(Transient)
	TObjectPtr<AActor> MovingBuildingActor;

	FTransform MovingOriginalTransform;
	UPROPERTY(Transient) TArray<FLoongSelectedMaterialState> MovingMaterialStates;
 bool bMovingOriginalCollision = true;
	bool bMovingBuilding = false;
	bool bDeleteConfirmationOpen = false;
	bool bMovingPlacementValid = false;
	bool bPreviousMoveKeyDown = false;
	bool bPreviousEscapeKeyDown = false;
	bool bPreviousDeleteKeyDown = false;

	FVector CurrentPlacementLocation = FVector::ZeroVector;
	FVector CurrentPlacementCenter = FVector::ZeroVector;
	FVector CurrentGridAnchor = FVector::ZeroVector;
	FRotator CurrentPlacementRotation = FRotator::ZeroRotator;
	bool bBuildingMode = false;
	bool bCurrentPlacementValid = false;
	bool bPreviousBuildKeyDown = false;
	bool bPreviousRotateKeyDown = false;
	bool bPreviousPlaceKeyDown = false;
	bool bPreviousSelectKeyDown = false;
	bool bPreviousBuildingNumberKeyDown[4] = {false, false, false, false};
};
