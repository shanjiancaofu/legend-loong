#include "LoongBuildingSystemComponent.h"

#include "BuildPlacementLibrary.h"
#include "LoongBuildingInstanceComponent.h"
#include "LoongBaseSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "LoongBuildingMenuWidget.h"
#include "LoongBuildingConfirmWidget.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"

ULoongBuildingSystemComponent::ULoongBuildingSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	BuildingDefinitions.SetNum(4);
	BuildingDefinitions[0].BuildingId = TEXT("SmallDragonHouse");
	BuildingDefinitions[0].DisplayName = NSLOCTEXT("LoongBuilding", "SmallDragonHouse", "Small Dragon House");
	BuildingDefinitions[0].GridWidth = 1;
	BuildingDefinitions[0].GridHeight = 1;
	BuildingDefinitions[0].BuildingHeight = 200.0f;
	BuildingDefinitions[0].BuildingCategory = ELoongBuildingCategory::Housing;
	BuildingDefinitions[1].BuildingId = TEXT("LongBuilding");
	BuildingDefinitions[1].DisplayName = NSLOCTEXT("LoongBuilding", "LongBuilding", "Long Building");
	BuildingDefinitions[1].GridWidth = 1;
	BuildingDefinitions[1].GridHeight = 2;
	BuildingDefinitions[1].BuildingHeight = 200.0f;
	BuildingDefinitions[1].BuildingCategory = ELoongBuildingCategory::Housing;
	BuildingDefinitions[2].BuildingId = TEXT("MediumDragonHouse");
	BuildingDefinitions[2].DisplayName = NSLOCTEXT("LoongBuilding", "MediumDragonHouse", "Medium Dragon House");
	BuildingDefinitions[2].GridWidth = 2;
	BuildingDefinitions[2].GridHeight = 2;
	BuildingDefinitions[2].BuildingHeight = 200.0f;
	BuildingDefinitions[2].BuildingCategory = ELoongBuildingCategory::Housing;
	BuildingDefinitions[3].BuildingId = TEXT("Hatchery");
	BuildingDefinitions[3].DisplayName = NSLOCTEXT("LoongBuilding", "Hatchery", "Hatchery");
	BuildingDefinitions[3].GridWidth = 2;
	BuildingDefinitions[3].GridHeight = 3;
	BuildingDefinitions[3].BuildingHeight = 200.0f;
	BuildingDefinitions[3].BuildingCategory = ELoongBuildingCategory::Production;
}

void ULoongBuildingSystemComponent::BeginPlay()
{
	Super::BeginPlay();
 if(GetWorld() && GetWorld()->GetAuthGameMode() && (GetWorld()->GetAuthGameMode()->ActorHasTag(TEXT("LoongCombatMode")) || GetWorld()->GetAuthGameMode()->ActorHasTag(TEXT("LoongCombatArtTest"))))
 {bEnableBasePersistence=false;bEnableDirectKeyboardInput=false;bStartInBuildMode=false;SetComponentTickEnabled(false);return;}

	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		PlayerController = Cast<APlayerController>(PawnOwner->GetController());
	}
	if (!PlayerController && GetWorld())
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}
	ResolveBaseGroundActor();
 if (bEnableBasePersistence && GetWorld() && GetWorld()->GetGameInstance())
  GetWorld()->GetGameInstance()->GetSubsystem<ULoongBaseSaveSubsystem>()->RegisterBuildingSystem(this);

	if (bEnableBasePersistence && PlayerController && PlayerController->GetLocalPlayer())
 {
  BaseSaveWidget=CreateWidget<ULoongBaseSaveWidget>(PlayerController);
  if(BaseSaveWidget){BaseSaveWidget->InitializePanel(this);BaseSaveWidget->AddToViewport(400);}
 }
 if (bStartInBuildMode)
	{
		SetBuildingMode(true);
	}
}

void ULoongBuildingSystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
 if(BaseSaveWidget){BaseSaveWidget->RemoveFromParent();BaseSaveWidget=nullptr;}
 if (bEnableBasePersistence && GetWorld() && GetWorld()->GetGameInstance())
  GetWorld()->GetGameInstance()->GetSubsystem<ULoongBaseSaveSubsystem>()->UnregisterBuildingSystem(this);
 if (bMovingBuilding)
	{
		CancelMoveBuilding();
	}
	HideBuildingMenu();
	HideDeletePrompt();
	ClearSelectedBuilding();
	DestroyPreviewActor();
	Super::EndPlay(EndPlayReason);
}

void ULoongBuildingSystemComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
 if (bDeleteConfirmationOpen && !PendingDeleteActor.IsValid()) { HideDeletePrompt(); ClearSelectedBuilding(); }
 if (SelectedBuildingActor && !IsValid(SelectedBuildingActor)) { SelectedBuildingActor = nullptr; ClearSelectedBuilding(); }
 if (bMovingBuilding && !IsValid(MovingBuildingActor)) { FinishMoveBuilding(false); }


	if (bEnableDirectKeyboardInput)
	{
		ProcessDirectKeyboardInput();
	}
	if (bMovingBuilding)
	{
		UpdateMovingBuilding();
	}
	else if (bBuildingMode)
	{
		UpdatePreview();
	}
	else if (bEnableDirectKeyboardInput)
	{
		// Selection is dispatched by ProcessDirectKeyboardInput.
	}
}

void ULoongBuildingSystemComponent::ToggleBuildingMode()
{
	SetBuildingMode(!bBuildingMode);
}

void ULoongBuildingSystemComponent::SetBuildingMode(const bool bEnabled)
{
	if (bMovingBuilding || bDeleteConfirmationOpen || IsSaveConfirmationOpen())
	{
		return;
	}

 if (bEnabled) ClearSelectedBuilding();
	bBuildingMode = bEnabled;
	bCurrentPlacementValid = false;
	if (bBuildingMode)
	{
		CreatePreviewActor();
		ShowBuildingMenu();
	}
	else
	{
		HideBuildingMenu();
		DestroyPreviewActor();
	}
}

void ULoongBuildingSystemComponent::RotateBuilding()
{
 if (bDeleteConfirmationOpen || (!bMovingBuilding && !bBuildingMode)) return;
	CurrentPlacementRotation = UBuildPlacementLibrary::SnapRotationTo90(
		CurrentPlacementRotation + FRotator(0.0f, 90.0f, 0.0f));
	if (bMovingBuilding)
	{
		UpdateMovingBuilding();
	}
	else
	{
		UpdatePreview();
	}
}

bool ULoongBuildingSystemComponent::SelectBuildingByIndex(const int32 DefinitionIndex)
{
	if (bMovingBuilding || bDeleteConfirmationOpen || IsSaveConfirmationOpen() || !BuildingDefinitions.IsValidIndex(DefinitionIndex) || !BuildingDefinitions[DefinitionIndex].IsUsable() || !BuildingDefinitions[DefinitionIndex].bUnlocked)
	{
		return false;
	}

	bSuppressSceneClickUntilRelease = true;
 SelectedBuildingIndex = DefinitionIndex;
 if (BuildingMenuWidget) BuildingMenuWidget->RefreshSelection();
	if (bBuildingMode)
	{
		DestroyPreviewActor();
		CreatePreviewActor();
		UpdatePreview();
	}
	return true;
}

bool ULoongBuildingSystemComponent::SelectBuildingById(const FName BuildingId)
{
	for (int32 Index = 0; Index < BuildingDefinitions.Num(); ++Index)
	{
		if (BuildingDefinitions[Index].BuildingId == BuildingId)
		{
			return SelectBuildingByIndex(Index);
		}
	}
	return false;
}

FName ULoongBuildingSystemComponent::GetSelectedBuildingId() const
{
	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	return Definition ? Definition->BuildingId : NAME_None;
}

bool ULoongBuildingSystemComponent::PlaceBuilding()
{
 if (bBuildingMode) UpdatePreview();
	if (bMovingBuilding || !bBuildingMode || !bCurrentPlacementValid || (!GetActiveBuildingClass() && !GetActiveDefinition()) || !GetWorld())
	{
		return false;
	}

	AActor* PlacedActor = SpawnBuildingActor(FTransform(CurrentPlacementRotation, CurrentPlacementLocation), false);
	if (!PlacedActor)
	{
		return false;
	}

	// Re-evaluate after the spawn attempt so the preview and placement share the same rules.
	UpdatePreview();
	return true;
}

void ULoongBuildingSystemComponent::UpdatePreview()
{
	if (!PreviewActor || (!GetActiveBuildingClass() && !GetActiveDefinition()))
	{
		bCurrentPlacementValid = false;
		return;
	}

	FHitResult GroundHit;
	if (!TraceMouseToGround(GroundHit) || GroundHit.GetActor() != BaseGroundActor)
	{
		bCurrentPlacementValid = false;
		SetPreviewMaterial(PreviewInvalidMaterial);
		return;
	}

	CurrentGridAnchor = UBuildPlacementLibrary::SnapToGrid(
		GroundHit.ImpactPoint + FVector(0.0f, 0.0f, BuildingPivotZOffset), GridSize);
	const int32 RotationQuarterTurns = UBuildPlacementLibrary::GetRotationQuarterTurns(CurrentPlacementRotation.Yaw);
	CurrentPlacementCenter = UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(
		CurrentGridAnchor, GetActiveGridWidth(), GetActiveGridHeight(), GridSize, RotationQuarterTurns);
	CurrentPlacementLocation = UBuildPlacementLibrary::GetActorLocationForCenteredFootprint(
		PreviewActor, CurrentPlacementCenter, CurrentPlacementRotation);
	PreviewActor->SetActorLocationAndRotation(CurrentPlacementLocation, CurrentPlacementRotation);

	bCurrentPlacementValid = IsInsideBaseBounds() && IsPlacementClearFiltered();
	SetPreviewMaterial(bCurrentPlacementValid ? PreviewValidMaterial : PreviewInvalidMaterial);
}

void ULoongBuildingSystemComponent::CreatePreviewActor()
{
	if (PreviewActor || (!GetActiveBuildingClass() && !GetActiveDefinition()) || !GetWorld())
	{
		return;
	}

	PreviewActor = SpawnBuildingActor(FTransform::Identity, true);
	if (PreviewActor)
	{
		PreviewActor->SetActorEnableCollision(false);
		PreviewActor->SetActorHiddenInGame(false);
		SetPreviewMaterial(PreviewInvalidMaterial);
	}
}

AActor* ULoongBuildingSystemComponent::SpawnBuildingActor(const FTransform& SpawnTransform, const bool bForPreview)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	const TSubclassOf<AActor> SpawnClass = GetActiveBuildingClass();
	UClass* SpawnUClass = SpawnClass ? SpawnClass.Get() : AActor::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* BuildingActor = GetWorld()->SpawnActor<AActor>(SpawnUClass,
		SpawnTransform, SpawnParams);
	if (!BuildingActor)
	{
		return nullptr;
	}

	ApplyActiveDefinitionVisuals(BuildingActor, bForPreview);
 BuildingActor->SetActorTransform(SpawnTransform);
	if (!bForPreview)
	{
		ULoongBuildingInstanceComponent* InstanceComponent = NewObject<ULoongBuildingInstanceComponent>(
			BuildingActor, TEXT("LoongBuildingInstance"));
		InstanceComponent->BuildingId = GetSelectedBuildingId();
  InstanceComponent->InstanceId = FGuid::NewGuid();
  InstanceComponent->ManagingSystem = this;
 InstanceComponent->OwningBaseIdentifier = GetBaseMapIdentifier();
  BuildingActor->AddInstanceComponent(InstanceComponent);
		InstanceComponent->RegisterComponent();
		BuildingActor->Tags.AddUnique(TEXT("LoongPlacedBuilding"));
 NotifyChange(BuildingActor, OnBuildingPlaced);
	}
	return BuildingActor;
}

void ULoongBuildingSystemComponent::ApplyActiveDefinitionVisuals(AActor* BuildingActor, const bool bForPreview)
{
	if (!BuildingActor)
	{
		return;
	}

	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	const UStaticMesh* ConfiguredMesh = Definition ?
		(bForPreview && Definition->PreviewMesh ? Definition->PreviewMesh : Definition->BuildingMesh) : nullptr;
	TArray<UStaticMeshComponent*> MeshComponents;
	BuildingActor->GetComponents<UStaticMeshComponent>(MeshComponents);
	if (MeshComponents.Num() == 0 && ConfiguredMesh)
	{
		USceneComponent* RootComponent = BuildingActor->GetRootComponent();
		if (!RootComponent)
		{
			RootComponent = NewObject<USceneComponent>(BuildingActor, TEXT("BuildingRoot"));
			BuildingActor->SetRootComponent(RootComponent);
			RootComponent->RegisterComponent();
		}

		UStaticMeshComponent* GeneratedMeshComponent = NewObject<UStaticMeshComponent>(BuildingActor, TEXT("BuildingMesh"));
		GeneratedMeshComponent->SetupAttachment(RootComponent);
		GeneratedMeshComponent->SetStaticMesh(const_cast<UStaticMesh*>(ConfiguredMesh));
		GeneratedMeshComponent->RegisterComponent();
		MeshComponents.Add(GeneratedMeshComponent);
	}
	if (ConfiguredMesh)
	{
		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent)
			{
				MeshComponent->SetStaticMesh(const_cast<UStaticMesh*>(ConfiguredMesh));
			}
		}
	}

	if (Definition && Definition->bAutoFitMeshToFootprint && MeshComponents.Num() == 1 && MeshComponents[0])
	{
		FVector LocalMin;
		FVector LocalMax;
		MeshComponents[0]->GetLocalBounds(LocalMin, LocalMax);
		const FVector CurrentSize = (LocalMax - LocalMin) * MeshComponents[0]->GetRelativeScale3D();
		const FVector DesiredSize(
			Definition->GridWidth * GridSize,
			Definition->GridHeight * GridSize,
			Definition->BuildingHeight);
		FVector RelativeScale = MeshComponents[0]->GetRelativeScale3D();
		if (FMath::Abs(CurrentSize.X) > KINDA_SMALL_NUMBER)
		{
			RelativeScale.X *= DesiredSize.X / CurrentSize.X;
		}
		if (FMath::Abs(CurrentSize.Y) > KINDA_SMALL_NUMBER)
		{
			RelativeScale.Y *= DesiredSize.Y / CurrentSize.Y;
		}
		if (FMath::Abs(CurrentSize.Z) > KINDA_SMALL_NUMBER)
		{
			RelativeScale.Z *= DesiredSize.Z / CurrentSize.Z;
		}
		MeshComponents[0]->SetRelativeScale3D(RelativeScale);
	}
}

void ULoongBuildingSystemComponent::DestroyPreviewActor()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

void ULoongBuildingSystemComponent::SetPreviewMaterial(UMaterialInterface* Material)
{
	if (!PreviewActor || !Material)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PreviewActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}
		for (int32 MaterialIndex = 0; MaterialIndex < Primitive->GetNumMaterials(); ++MaterialIndex)
		{
			Primitive->SetMaterial(MaterialIndex, Material);
		}
	}
}

void ULoongBuildingSystemComponent::ShowBuildingMenu()
{
	if (BuildingMenuWidget || !PlayerController || !PlayerController->GetLocalPlayer())
	{
		return;
	}

	UClass* WidgetClass = BuildingMenuWidgetClass ? BuildingMenuWidgetClass.Get() : ULoongBuildingMenuWidget::StaticClass();
	BuildingMenuWidget = CreateWidget<ULoongBuildingMenuWidget>(PlayerController, WidgetClass);
	if (BuildingMenuWidget)
	{
		BuildingMenuWidget->InitializeForBuildingSystem(this);
		BuildingMenuWidget->AddToViewport(100);
	}
}

void ULoongBuildingSystemComponent::HideBuildingMenu()
{
	if (BuildingMenuWidget)
	{
		BuildingMenuWidget->RemoveFromParent();
		BuildingMenuWidget = nullptr;
	}
}

void ULoongBuildingSystemComponent::ShowDeletePrompt()
{
	if (bDeleteConfirmationOpen || !IsManagedBuildingActor(SelectedBuildingActor)) return;
 PendingDeleteActor = SelectedBuildingActor;
 bDeleteConfirmationOpen = true;
 if (OperationPanelWidget) OperationPanelWidget->RemoveFromParent();
	if (!PlayerController || !PlayerController->GetLocalPlayer()) return;
 DeletePromptWidget = CreateWidget<ULoongBuildingConfirmWidget>(PlayerController, ULoongBuildingConfirmWidget::StaticClass());
	if (DeletePromptWidget)
	{
		FText Name = FText::FromString(SelectedBuildingActor->GetName());
		if (const ULoongBuildingInstanceComponent* Instance = SelectedBuildingActor->FindComponentByClass<ULoongBuildingInstanceComponent>())
		{
			Name = GetSelectedDefinition().DisplayName;
		}
		DeletePromptWidget->InitializeDeletePrompt(this, Name);
		DeletePromptWidget->AddToViewport(300);
		bDeleteConfirmationOpen = true;
	}
}

void ULoongBuildingSystemComponent::HideDeletePrompt()
{
	if (DeletePromptWidget) DeletePromptWidget->RemoveFromParent();
	DeletePromptWidget = nullptr;
	bDeleteConfirmationOpen = false;
 PendingDeleteActor.Reset();
 bSuppressSceneClickUntilRelease = true;
}

void ULoongBuildingSystemComponent::RequestDeleteSelectedBuilding()
{
 bSuppressSceneClickUntilRelease = true;
	if (!IsSaveConfirmationOpen() && !bBuildingMode && !bMovingBuilding && SelectedBuildingActor && IsManagedBuildingActor(SelectedBuildingActor)) ShowDeletePrompt();
}

void ULoongBuildingSystemComponent::CancelDeletePrompt() { HideDeletePrompt(); RefreshOperationPanel(); }

void ULoongBuildingSystemComponent::ConfirmDeleteSelectedBuilding()
{
	if (!bDeleteConfirmationOpen) return;
	AActor* ActorToDelete = PendingDeleteActor.Get();
	HideDeletePrompt();
	ClearSelectedBuilding();
	if (IsManagedBuildingActor(ActorToDelete)) { NotifyChange(ActorToDelete, OnBuildingDemolished); ActorToDelete->Destroy(); }
}

bool ULoongBuildingSystemComponent::TraceMouseToBuilding(FHitResult& OutHit) const
{
	if (!PlayerController || !GetWorld())
	{
		return false;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LoongBuildingSelection), true);
	QueryParams.AddIgnoredActor(GetOwner());
	const FVector TraceEnd = RayOrigin + RayDirection * 100000.0f;
	if (GetWorld()->LineTraceSingleByChannel(OutHit, RayOrigin, TraceEnd, ECC_Visibility, QueryParams))
	{
		return true;
	}
	return GetWorld()->LineTraceSingleByChannel(OutHit, RayOrigin, TraceEnd, PlacementCollisionChannel, QueryParams);
}

bool ULoongBuildingSystemComponent::IsManagedBuildingActor(const AActor* Actor) const
{
 if (!IsValid(Actor) || Actor == BaseGroundActor || Actor == GetOwner()) return false;
 const auto* Instance = Actor->FindComponentByClass<ULoongBuildingInstanceComponent>();
 if (!Instance || Instance->BuildingId.IsNone() || !Instance->InstanceId.IsValid() || Instance->ManagingSystem.Get() != this) return false;
 return BuildingDefinitions.ContainsByPredicate([Instance](const FLoongBuildingDefinition& D){ return D.IsUsable() && D.BuildingId == Instance->BuildingId; });
}

void ULoongBuildingSystemComponent::SelectBuildingActor(AActor* Actor)
{
	if (bBuildingMode || bMovingBuilding || bDeleteConfirmationOpen || SelectedBuildingActor == Actor)
	{
		return;
	}

	ClearSelectedBuilding();
	if (!IsManagedBuildingActor(Actor))
	{
		return;
	}

	SelectedBuildingActor = Actor;
	TArray<UPrimitiveComponent*> Primitives;
	Actor->GetComponents<UPrimitiveComponent>(Primitives);
	const UMaterialInterface* HighlightMaterial = SelectedBuildingMaterial ? SelectedBuildingMaterial : PreviewValidMaterial;
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive)
		{

			if (UMeshComponent* Mesh = Cast<UMeshComponent>(Primitive))
			{
				FLoongSelectedMaterialState State;
				State.Mesh = Mesh;
 State.Overlay = Mesh->GetOverlayMaterial();
 State.bCustomDepth = Mesh->bRenderCustomDepth != 0;
 State.Stencil = Mesh->CustomDepthStencilValue;
				for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
				{
					State.Materials.Add(Mesh->GetMaterial(MaterialIndex));
					if (HighlightMaterial)
					{
						Mesh->SetMaterial(MaterialIndex, const_cast<UMaterialInterface*>(HighlightMaterial));
					}
				}
				SelectedMaterialStates.Add(MoveTemp(State));
 Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(1);
				Mesh->SetOverlayMaterial(SelectedBuildingMaterial);
			}
		}
	}
 RefreshOperationPanel();
}

void ULoongBuildingSystemComponent::ClearSelectedBuilding()
{
 bSuppressSceneClickUntilRelease = true;
 if (bMovingBuilding) return;
 if (OperationPanelWidget) { OperationPanelWidget->RemoveFromParent(); OperationPanelWidget = nullptr; }
	for (const FLoongSelectedMaterialState& State : SelectedMaterialStates)
	{
		if (IsValid(State.Mesh))
		{
			for (int32 MaterialIndex = 0; MaterialIndex < State.Materials.Num(); ++MaterialIndex)
			{
				State.Mesh->SetMaterial(MaterialIndex, State.Materials[MaterialIndex]);
			}
			State.Mesh->SetOverlayMaterial(State.Overlay);
 State.Mesh->SetRenderCustomDepth(State.bCustomDepth);
 State.Mesh->SetCustomDepthStencilValue(State.Stencil);
		}
	}
	SelectedMaterialStates.Empty();

	SelectedBuildingActor = nullptr;
}

void ULoongBuildingSystemComponent::CaptureAndSetActorMaterial(AActor* Actor, UMaterialInterface* Material,
	TArray<FLoongSelectedMaterialState>& OutStates)
{
	if (!Actor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Primitives;
	Actor->GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(Primitive))
		{
			FLoongSelectedMaterialState State;
			State.Mesh = Mesh;
 State.Overlay = Mesh->GetOverlayMaterial();
 State.bCustomDepth = Mesh->bRenderCustomDepth != 0;
 State.Stencil = Mesh->CustomDepthStencilValue;
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
			{
				State.Materials.Add(Mesh->GetMaterial(MaterialIndex));
				if (Material)
				{
					Mesh->SetMaterial(MaterialIndex, Material);
				}
			}
			OutStates.Add(MoveTemp(State));
		}
	}
}

void ULoongBuildingSystemComponent::RestoreMaterialStates(TArray<FLoongSelectedMaterialState>& States)
{
	for (const FLoongSelectedMaterialState& State : States)
	{
		if (!IsValid(State.Mesh))
		{
			continue;
		}
		for (int32 MaterialIndex = 0; MaterialIndex < State.Materials.Num(); ++MaterialIndex)
		{
			State.Mesh->SetMaterial(MaterialIndex, State.Materials[MaterialIndex]);
		}
	}
	States.Empty();
}

void ULoongBuildingSystemComponent::BeginMoveSelectedBuilding()
{
 bSuppressSceneClickUntilRelease = true;
 if (bDeleteConfirmationOpen || IsSaveConfirmationOpen()) return;
	if (bBuildingMode || bMovingBuilding || !SelectedBuildingActor || !IsManagedBuildingActor(SelectedBuildingActor))
	{
		return;
	}

	MovingBuildingActor = SelectedBuildingActor;
	MovingOriginalTransform = MovingBuildingActor->GetActorTransform();
 bMovingOriginalCollision = MovingBuildingActor->GetActorEnableCollision();
	ClearSelectedBuilding();

	if (const ULoongBuildingInstanceComponent* Instance = MovingBuildingActor->FindComponentByClass<ULoongBuildingInstanceComponent>())
	{
		if (!Instance->BuildingId.IsNone())
		{
			SelectBuildingById(Instance->BuildingId);
		}
	}

	MovingBuildingActor->SetActorEnableCollision(false);
	CaptureAndSetActorMaterial(MovingBuildingActor,
		PreviewInvalidMaterial ? PreviewInvalidMaterial : SelectedBuildingMaterial, MovingMaterialStates);
	CurrentPlacementRotation = UBuildPlacementLibrary::SnapRotationTo90(MovingBuildingActor->GetActorRotation());
	bMovingBuilding = true;
	bMovingPlacementValid = false;
	UpdateMovingBuilding();
}

void ULoongBuildingSystemComponent::UpdateMovingBuilding()
{
	if (!bMovingBuilding || !IsValid(MovingBuildingActor))
	{
		return;
	}

	FHitResult GroundHit;
	if (!TraceMouseToGround(GroundHit) || GroundHit.GetActor() != BaseGroundActor)
	{
		bMovingPlacementValid = false;
		RestoreMaterialStates(MovingMaterialStates);
		CaptureAndSetActorMaterial(MovingBuildingActor, PreviewInvalidMaterial, MovingMaterialStates);
		return;
	}

	CurrentGridAnchor = UBuildPlacementLibrary::SnapToGrid(
		GroundHit.ImpactPoint + FVector(0.0f, 0.0f, BuildingPivotZOffset), GridSize);
	const int32 RotationQuarterTurns = UBuildPlacementLibrary::GetRotationQuarterTurns(CurrentPlacementRotation.Yaw);
	CurrentPlacementCenter = UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(
		CurrentGridAnchor, GetActiveGridWidth(), GetActiveGridHeight(), GridSize, RotationQuarterTurns);
	CurrentPlacementLocation = UBuildPlacementLibrary::GetActorLocationForCenteredFootprint(
		MovingBuildingActor, CurrentPlacementCenter, CurrentPlacementRotation);
	MovingBuildingActor->SetActorLocationAndRotation(CurrentPlacementLocation, CurrentPlacementRotation);

	bMovingPlacementValid = IsInsideBaseBounds() && IsPlacementClearFiltered();
	RestoreMaterialStates(MovingMaterialStates);
	CaptureAndSetActorMaterial(MovingBuildingActor,
		bMovingPlacementValid ? PreviewValidMaterial : PreviewInvalidMaterial, MovingMaterialStates);
}

bool ULoongBuildingSystemComponent::ConfirmMoveBuilding()
{
	if (!bMovingBuilding || !IsValid(MovingBuildingActor))
	{
		return false;
	}

	UpdateMovingBuilding();
	if (!bMovingPlacementValid)
	{
		return false;
	}

 FinishMoveBuilding(true);
 return true;
}

void ULoongBuildingSystemComponent::CancelMoveBuilding() { FinishMoveBuilding(false); }

void ULoongBuildingSystemComponent::FinishMoveBuilding(bool bConfirm)
{
 if (!bMovingBuilding) return;
 bSuppressSceneClickUntilRelease = true;
 AActor* Actor = MovingBuildingActor;
 if (IsValid(Actor))
 {
  if (!bConfirm) Actor->SetActorTransform(MovingOriginalTransform);
  Actor->SetActorEnableCollision(bMovingOriginalCollision);
  RestoreMaterialStates(MovingMaterialStates);
  if (bConfirm) NotifyChange(Actor, OnBuildingMoved);
 }
 else MovingMaterialStates.Empty();
 MovingBuildingActor = nullptr; bMovingBuilding = false; bMovingPlacementValid = false;
 if (IsValid(Actor)) SelectBuildingActor(Actor);
}

void ULoongBuildingSystemComponent::ProcessSelectionInput()
{
	if (!PlayerController || bDeleteConfirmationOpen || bMovingBuilding) return;

	const bool bSelectKeyDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (bSelectKeyDown && !bPreviousPlaceKeyDown && !IsPointerOverBuildingUI() && !bSuppressSceneClickUntilRelease)
	{
		FHitResult Hit;
		if (TraceMouseToBuilding(Hit) && IsManagedBuildingActor(Hit.GetActor()))
		{
			SelectBuildingActor(Hit.GetActor());
		}
		else
		{
			ClearSelectedBuilding();
		}
	}
	bPreviousSelectKeyDown = bSelectKeyDown;
}

bool ULoongBuildingSystemComponent::TraceMouseToGround(FHitResult& OutHit) const
{
	if (!PlayerController || !GetWorld())
	{
		return false;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LoongBuildingGround), true);
	QueryParams.AddIgnoredActor(GetOwner());
	if (PreviewActor)
	{
		QueryParams.AddIgnoredActor(PreviewActor);
	}
	if (MovingBuildingActor)
	{
		QueryParams.AddIgnoredActor(MovingBuildingActor);
	}

	const FVector TraceEnd = RayOrigin + RayDirection * 100000.0f;
	return GetWorld()->LineTraceSingleByChannel(OutHit, RayOrigin, TraceEnd,
		ResolveGroundTraceChannel(), QueryParams);
}

bool ULoongBuildingSystemComponent::IsPlacementClearFiltered() const
{
	if (!GetWorld() || (!GetActiveBuildingClass() && !GetActiveDefinition()))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LoongBuildingOverlap), false);
	QueryParams.AddIgnoredActor(GetOwner());
	if (PreviewActor)
	{
		QueryParams.AddIgnoredActor(PreviewActor);
	}

		if (MovingBuildingActor)
	{
		QueryParams.AddIgnoredActor(MovingBuildingActor);
	}

 const FVector2D FootprintHalfExtent = GetActiveFootprintHalfExtent();
	const FCollisionShape Shape = FCollisionShape::MakeBox(
		FVector(FootprintHalfExtent.X, FootprintHalfExtent.Y, GetActiveBuildingHalfHeight()));
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, CurrentPlacementCenter,
 CurrentPlacementRotation.Quaternion(), PlacementCollisionChannel, Shape, QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* HitActor = Overlap.GetActor();

		if (HitActor && HitActor != PreviewActor && HitActor != MovingBuildingActor && IsManagedBuildingActor(HitActor)
			&& UBuildPlacementLibrary::DoBuildingFootprintsOverlap(HitActor, CurrentPlacementCenter,
				FootprintHalfExtent, CurrentPlacementRotation, 1.0f))
		{
			return false;
		}
	}
	return true;
}

bool ULoongBuildingSystemComponent::IsInsideBaseBounds() const
{
	if (!BaseGroundActor)
	{
		return false;
	}

	return UBuildPlacementLibrary::IsWithinBaseActorBounds(BaseGroundActor, CurrentPlacementCenter,
		GetActiveFootprintHalfExtent(),
		CurrentPlacementRotation.Yaw);
}

const FLoongBuildingDefinition* ULoongBuildingSystemComponent::GetActiveDefinition() const
{
	return BuildingDefinitions.IsValidIndex(SelectedBuildingIndex) && BuildingDefinitions[SelectedBuildingIndex].IsUsable()
		? &BuildingDefinitions[SelectedBuildingIndex] : nullptr;
}

TSubclassOf<AActor> ULoongBuildingSystemComponent::GetActiveBuildingClass() const
{
	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	return Definition && Definition->BuildingClass ? Definition->BuildingClass : BuildingClass;
}

int32 ULoongBuildingSystemComponent::GetActiveGridWidth() const
{
	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	return Definition ? Definition->GridWidth : 1;
}

int32 ULoongBuildingSystemComponent::GetActiveGridHeight() const
{
	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	return Definition ? Definition->GridHeight : 1;
}

FVector2D ULoongBuildingSystemComponent::GetActiveFootprintHalfExtent() const
{
	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	return Definition
		? FVector2D(0.5f * Definition->GridWidth * GridSize, 0.5f * Definition->GridHeight * GridSize)
		: BuildingFootprintHalfExtent;
}

float ULoongBuildingSystemComponent::GetActiveBuildingHalfHeight() const
{
	const FLoongBuildingDefinition* Definition = GetActiveDefinition();
	return Definition ? Definition->BuildingHeight * 0.5f : BuildingHalfHeight;
}

void ULoongBuildingSystemComponent::ResolveBaseGroundActor()
{
	if (BaseGroundActor || BaseGroundActorTag.IsNone() || !GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
	{
		AActor* Candidate = *ActorIterator;
		if (Candidate && Candidate->ActorHasTag(BaseGroundActorTag))
		{
			BaseGroundActor = Candidate;
			UE_LOG(LogTemp, Log, TEXT("Loong building system resolved BaseGroundActor '%s' using tag '%s'."),
				*Candidate->GetName(), *BaseGroundActorTag.ToString());
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Loong building system could not resolve BaseGroundActor using tag '%s'."),
		*BaseGroundActorTag.ToString());
}

ECollisionChannel ULoongBuildingSystemComponent::ResolveGroundTraceChannel() const
{
	return UEngineTypes::ConvertToCollisionChannel(GroundTraceChannel);
}

void ULoongBuildingSystemComponent::ProcessDirectKeyboardInput()
{
	if (!PlayerController)
	{
		return;
	}

	const bool bBuildKeyDown = PlayerController->IsInputKeyDown(EKeys::B);
	const bool bRotateKeyDown = PlayerController->IsInputKeyDown(EKeys::R);
	const bool bPlaceKeyDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bMoveKeyDown = PlayerController->IsInputKeyDown(EKeys::M);
	const bool bEscapeKeyDown = PlayerController->IsInputKeyDown(EKeys::Escape);
	const bool bDeleteKeyDown = PlayerController->IsInputKeyDown(EKeys::Delete);
	const FKey BuildingNumberKeys[4] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};

	if (IsSaveConfirmationOpen())
 {
  if(bEscapeKeyDown && !bPreviousEscapeKeyDown){BaseSaveWidget->CancelConfirmation();bSuppressSceneClickUntilRelease=true;}
  bPreviousBuildKeyDown=bBuildKeyDown;bPreviousMoveKeyDown=bMoveKeyDown;bPreviousRotateKeyDown=bRotateKeyDown;
  bPreviousPlaceKeyDown=bPlaceKeyDown;bPreviousEscapeKeyDown=bEscapeKeyDown;bPreviousDeleteKeyDown=bDeleteKeyDown;
  return;
 }
 if (bEscapeKeyDown && !bPreviousEscapeKeyDown)
	{
 bSuppressSceneClickUntilRelease = true;
		if (bDeleteConfirmationOpen)
		{
			CancelDeletePrompt();
		}
		else if (bMovingBuilding)
		{
			CancelMoveBuilding();
		}
		else if (bBuildingMode)
		{
			SetBuildingMode(false);
		}
		else
		{
			ClearSelectedBuilding();
		}
	}
	if (bDeleteKeyDown && !bPreviousDeleteKeyDown)
	{
		RequestDeleteSelectedBuilding();
	}
	if (!bDeleteConfirmationOpen && bMoveKeyDown && !bPreviousMoveKeyDown && !bBuildingMode && !bMovingBuilding)
	{
		BeginMoveSelectedBuilding();
	}
	if (!bDeleteConfirmationOpen && bBuildKeyDown && !bPreviousBuildKeyDown && !bMovingBuilding)
	{
		ToggleBuildingMode();
	}
	if (!bDeleteConfirmationOpen && (bBuildingMode || bMovingBuilding) && bRotateKeyDown && !bPreviousRotateKeyDown)
	{
		RotateBuilding();
	}
	if (!bDeleteConfirmationOpen && !IsPointerOverBuildingUI() && !bSuppressSceneClickUntilRelease && bMovingBuilding && bPlaceKeyDown && !bPreviousPlaceKeyDown)
	{
		ConfirmMoveBuilding();
	}
	else if (!bDeleteConfirmationOpen && !IsPointerOverBuildingUI() && !bSuppressSceneClickUntilRelease && bBuildingMode && bPlaceKeyDown && !bPreviousPlaceKeyDown)
	{
		PlaceBuilding();
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BuildingNumberKeys); ++Index)
	{
		const bool bNumberKeyDown = PlayerController->IsInputKeyDown(BuildingNumberKeys[Index]);
		if (!bMovingBuilding && !bDeleteConfirmationOpen && bBuildingMode && bNumberKeyDown && !bPreviousBuildingNumberKeyDown[Index])
		{
			SelectBuildingByIndex(Index);
		}
		bPreviousBuildingNumberKeyDown[Index] = bNumberKeyDown;
	}

	if (!bBuildingMode && !bMovingBuilding && !bDeleteConfirmationOpen) ProcessSelectionInput();
 if (!bPlaceKeyDown) bSuppressSceneClickUntilRelease = false;
 bPreviousBuildKeyDown = bBuildKeyDown;
	bPreviousRotateKeyDown = bRotateKeyDown;
	bPreviousPlaceKeyDown = bPlaceKeyDown;
	bPreviousMoveKeyDown = bMoveKeyDown;
	bPreviousEscapeKeyDown = bEscapeKeyDown;
	bPreviousDeleteKeyDown = bDeleteKeyDown;
}

ELoongBuildingInteractionState ULoongBuildingSystemComponent::GetInteractionState() const
{
 if (bDeleteConfirmationOpen) return ELoongBuildingInteractionState::DeleteConfirmation;
 if (bMovingBuilding) return ELoongBuildingInteractionState::Moving;
 if (bBuildingMode) return ELoongBuildingInteractionState::Building;
 return IsValid(SelectedBuildingActor) ? ELoongBuildingInteractionState::Selected : ELoongBuildingInteractionState::Default;
}
FLoongBuildingDefinition ULoongBuildingSystemComponent::GetSelectedDefinition() const
{
 if (IsValid(SelectedBuildingActor))
  if (const auto* I = SelectedBuildingActor->FindComponentByClass<ULoongBuildingInstanceComponent>())
   for (const auto& D : BuildingDefinitions) if (D.BuildingId == I->BuildingId) return D;
 return FLoongBuildingDefinition();
}
void ULoongBuildingSystemComponent::NotifyChange(AActor* Actor, FLoongBuildingChanged& Event)
{
 if (bRestoringBuildings) return;
 if (IsValid(Actor)) if (const auto* I = Actor->FindComponentByClass<ULoongBuildingInstanceComponent>())
  Event.Broadcast(I->InstanceId, I->BuildingId, Actor->GetActorTransform());
}
void ULoongBuildingSystemComponent::RefreshOperationPanel()
{
 if (OperationPanelWidget) { OperationPanelWidget->RemoveFromParent(); OperationPanelWidget = nullptr; }
 if (bEndingPlay || !PlayerController || !PlayerController->GetLocalPlayer() || GetInteractionState() != ELoongBuildingInteractionState::Selected) return;
 OperationPanelWidget = CreateWidget<ULoongBuildingConfirmWidget>(PlayerController);
 if (!OperationPanelWidget) return;
 OperationPanelWidget->InitializeOperations(this);
 OperationPanelWidget->AddToViewport(200);
}
bool ULoongBuildingSystemComponent::IsPointerOverBuildingUI() const
{
 if (bDeleteConfirmationOpen || IsSaveConfirmationOpen()) return true;
 const auto Over = [](const auto* W) { return W && W->IsInViewport() && W->IsPointerOverPanel(); };
 return Over(BuildingMenuWidget.Get()) || Over(OperationPanelWidget.Get()) || Over(BaseSaveWidget.Get());
}

FString ULoongBuildingSystemComponent::GetBaseMapIdentifier() const
{
 if (!BaseIdentifierOverride.IsEmpty()) return BaseIdentifierOverride;
 if (!GetWorld()) return FString();
 FString Name = GetWorld()->GetOutermost()->GetName();
 if (!GetWorld()->StreamingLevelsPrefix.IsEmpty()) Name.ReplaceInline(*GetWorld()->StreamingLevelsPrefix, TEXT(""));
 return Name;
}
bool ULoongBuildingSystemComponent::HasBuildingDefinition(FName Id) const
{
 return BuildingDefinitions.ContainsByPredicate([Id](const FLoongBuildingDefinition& D){ return D.IsUsable() && D.BuildingId == Id; });
}
bool ULoongBuildingSystemComponent::ExportCommittedBuildings(TArray<FLoongSavedBuilding>& OutBuildings) const
{
 OutBuildings.Reset(); if (!GetWorld()) return false;
 TSet<FGuid> Seen;
 for (TActorIterator<AActor> It(GetWorld()); It; ++It)
 {
  auto* I = It->FindComponentByClass<ULoongBuildingInstanceComponent>();
  if (!I || I->ManagingSystem.Get() != this || It->IsActorBeingDestroyed()) continue;
  if (!I->InstanceId.IsValid() || Seen.Contains(I->InstanceId) || !HasBuildingDefinition(I->BuildingId)) return false;
  Seen.Add(I->InstanceId);
  FLoongSavedBuilding R; R.InstanceGuid = I->InstanceId; R.BuildingId = I->BuildingId;
  R.Transform = (*It == MovingBuildingActor) ? MovingOriginalTransform : It->GetActorTransform();
  R.State = I->PersistentState; OutBuildings.Add(R);
 }
 return true;
}
bool ULoongBuildingSystemComponent::ClearManagedBuildings()
{
 if (!GetWorld() || HasPendingBuildingEdit()) return false;
 SetBuildingMode(false); ClearSelectedBuilding();
 TArray<AActor*> Remove;
 for (TActorIterator<AActor> It(GetWorld()); It; ++It)
  if (auto* I = It->FindComponentByClass<ULoongBuildingInstanceComponent>())
   if (I->ManagingSystem.Get() == this && *It != BaseGroundActor && *It != GetOwner()) Remove.Add(*It);
 for (AActor* A : Remove) A->Destroy();
 return true;
}
bool ULoongBuildingSystemComponent::RestoreBuildings(const TArray<FLoongSavedBuilding>& Buildings)
{
 if (!GetWorld() || HasPendingBuildingEdit()) return false;
 TArray<AActor*> Staged;
 const int32 PreviousIndex = SelectedBuildingIndex;
 bRestoringBuildings = true;
 for (const auto& R : Buildings)
 {
  SelectedBuildingIndex = BuildingDefinitions.IndexOfByPredicate([&R](const FLoongBuildingDefinition& D){ return D.IsUsable() && D.BuildingId == R.BuildingId; });
  if (SelectedBuildingIndex == INDEX_NONE) break;
  const auto* Definition = GetActiveDefinition();
  UClass* RestoreClass = GetActiveBuildingClass().Get();
  if ((RestoreClass && RestoreClass->HasAnyClassFlags(CLASS_Abstract)) || (!RestoreClass && (!Definition || !Definition->BuildingMesh))) break;
  AActor* A = SpawnBuildingActor(R.Transform, false);
  if (!IsValid(A)) break;
  // Actor Transform includes pivot and scale already; do not grid-snap it again.
  A->SetActorTransform(R.Transform);
  auto* I = A->FindComponentByClass<ULoongBuildingInstanceComponent>();
  if (!I) { A->Destroy(); break; }
  I->InstanceId = R.InstanceGuid; I->PersistentState = R.State;
  Staged.Add(A);
 }
 SelectedBuildingIndex = PreviousIndex;
 if (Staged.Num() != Buildings.Num())
 {
  for (AActor* A : Staged) A->Destroy();
  bRestoringBuildings = false; return false;
 }
 SetBuildingMode(false); ClearSelectedBuilding();
 TArray<AActor*> Old;
 for (TActorIterator<AActor> It(GetWorld()); It; ++It)
  if (!Staged.Contains(*It)) if (auto* I = It->FindComponentByClass<ULoongBuildingInstanceComponent>())
   if (I->ManagingSystem.Get() == this && *It != BaseGroundActor && *It != GetOwner()) Old.Add(*It);
 for (AActor* A : Old) A->Destroy();
 bRestoringBuildings = false; return true;
}

bool ULoongBuildingSystemComponent::IsSaveConfirmationOpen() const { return BaseSaveWidget && BaseSaveWidget->IsConfirmationOpen(); }
