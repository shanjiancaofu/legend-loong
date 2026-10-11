#include "LoongBuildingSystemComponent.h"

#include "BuildPlacementLibrary.h"
#include "LoongBuildingInstanceComponent.h"
#include "LoongBuildingMenuWidget.h"
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

	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		PlayerController = Cast<APlayerController>(PawnOwner->GetController());
	}
	if (!PlayerController && GetWorld())
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}
	ResolveBaseGroundActor();

	if (bStartInBuildMode)
	{
		SetBuildingMode(true);
	}
}

void ULoongBuildingSystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bMovingBuilding)
	{
		CancelMoveBuilding();
	}
	HideBuildingMenu();
	ClearSelectedBuilding();
	DestroyPreviewActor();
	Super::EndPlay(EndPlayReason);
}

void ULoongBuildingSystemComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
		ProcessSelectionInput();
	}
}

void ULoongBuildingSystemComponent::ToggleBuildingMode()
{
	SetBuildingMode(!bBuildingMode);
}

void ULoongBuildingSystemComponent::SetBuildingMode(const bool bEnabled)
{
	if (bMovingBuilding)
	{
		return;
	}

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
	if (!BuildingDefinitions.IsValidIndex(DefinitionIndex) || !BuildingDefinitions[DefinitionIndex].IsUsable())
	{
		return false;
	}

	SelectedBuildingIndex = DefinitionIndex;
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
	if (!TraceMouseToGround(GroundHit))
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
	if (!bForPreview)
	{
		ULoongBuildingInstanceComponent* InstanceComponent = NewObject<ULoongBuildingInstanceComponent>(
			BuildingActor, TEXT("LoongBuildingInstance"));
		InstanceComponent->BuildingId = GetSelectedBuildingId();
		InstanceComponent->RegisterComponent();
		BuildingActor->Tags.AddUnique(TEXT("LoongPlacedBuilding"));
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
	if (BuildingMenuWidget || !PlayerController)
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
	if (!Actor)
	{
		return false;
	}
	if (Actor->FindComponentByClass<ULoongBuildingInstanceComponent>()
		|| Actor->ActorHasTag(TEXT("LoongPlacedBuilding")))
	{
		return true;
	}
	if (BuildingClass && Actor->IsA(BuildingClass))
	{
		return true;
	}
	for (const FLoongBuildingDefinition& Definition : BuildingDefinitions)
	{
		if (Definition.BuildingClass && Actor->IsA(Definition.BuildingClass))
		{
			return true;
		}
	}
	return false;
}

void ULoongBuildingSystemComponent::SelectBuildingActor(AActor* Actor)
{
	if (SelectedBuildingActor == Actor)
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
			Primitive->SetRenderCustomDepth(true);
			Primitive->SetCustomDepthStencilValue(1);
			if (UMeshComponent* Mesh = Cast<UMeshComponent>(Primitive))
			{
				FLoongSelectedMaterialState State;
				State.Mesh = Mesh;
				for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
				{
					State.Materials.Add(Mesh->GetMaterial(MaterialIndex));
					if (HighlightMaterial)
					{
						Mesh->SetMaterial(MaterialIndex, const_cast<UMaterialInterface*>(HighlightMaterial));
					}
				}
				SelectedMaterialStates.Add(MoveTemp(State));
				Mesh->SetOverlayMaterial(SelectedBuildingMaterial);
			}
		}
	}
}

void ULoongBuildingSystemComponent::ClearSelectedBuilding()
{
	for (const FLoongSelectedMaterialState& State : SelectedMaterialStates)
	{
		if (State.Mesh)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < State.Materials.Num(); ++MaterialIndex)
			{
				State.Mesh->SetMaterial(MaterialIndex, State.Materials[MaterialIndex]);
			}
			State.Mesh->SetOverlayMaterial(nullptr);
			State.Mesh->SetRenderCustomDepth(false);
		}
	}
	SelectedMaterialStates.Empty();

	if (SelectedBuildingActor)
	{
		TArray<UPrimitiveComponent*> Primitives;
		SelectedBuildingActor->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive)
			{
				Primitive->SetRenderCustomDepth(false);
			}
		}
	}
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
		if (!State.Mesh)
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
	if (bBuildingMode || bMovingBuilding || !SelectedBuildingActor || !IsManagedBuildingActor(SelectedBuildingActor))
	{
		return;
	}

	MovingBuildingActor = SelectedBuildingActor;
	MovingOriginalTransform = MovingBuildingActor->GetActorTransform();
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
	if (!bMovingBuilding || !MovingBuildingActor)
	{
		return;
	}

	FHitResult GroundHit;
	if (!TraceMouseToGround(GroundHit))
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
	if (!bMovingBuilding || !MovingBuildingActor)
	{
		return false;
	}

	UpdateMovingBuilding();
	if (!bMovingPlacementValid)
	{
		return false;
	}

	MovingBuildingActor->SetActorEnableCollision(true);
	RestoreMaterialStates(MovingMaterialStates);
	AActor* MovedActor = MovingBuildingActor;
	MovingBuildingActor = nullptr;
	bMovingBuilding = false;
	SelectBuildingActor(MovedActor);
	return true;
}

void ULoongBuildingSystemComponent::CancelMoveBuilding()
{
	if (!bMovingBuilding || !MovingBuildingActor)
	{
		return;
	}

	MovingBuildingActor->SetActorTransform(MovingOriginalTransform);
	MovingBuildingActor->SetActorEnableCollision(true);
	RestoreMaterialStates(MovingMaterialStates);
	AActor* RestoredActor = MovingBuildingActor;
	MovingBuildingActor = nullptr;
	bMovingBuilding = false;
	bMovingPlacementValid = false;
	SelectBuildingActor(RestoredActor);
}

void ULoongBuildingSystemComponent::ProcessSelectionInput()
{
	if (!PlayerController)
	{
		return;
	}

	const bool bSelectKeyDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (bSelectKeyDown && !bPreviousSelectKeyDown)
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

	const FVector2D FootprintHalfExtent = GetActiveFootprintHalfExtent();
	const FCollisionShape Shape = FCollisionShape::MakeBox(
		FVector(FootprintHalfExtent.X, FootprintHalfExtent.Y, GetActiveBuildingHalfHeight()));
	TArray<FOverlapResult> Overlaps;
	if (!GetWorld()->OverlapMultiByChannel(Overlaps, CurrentPlacementCenter,
		CurrentPlacementRotation.Quaternion(), PlacementCollisionChannel, Shape, QueryParams))
	{
		return true;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* HitActor = Overlap.GetActor();
		const TSubclassOf<AActor> ActiveClass = GetActiveBuildingClass();
		if (HitActor && HitActor != PreviewActor && ActiveClass && HitActor->IsA(ActiveClass)
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
	const FKey BuildingNumberKeys[4] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};

	if (bEscapeKeyDown && !bPreviousEscapeKeyDown)
	{
		if (bMovingBuilding)
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
	if (bMoveKeyDown && !bPreviousMoveKeyDown && !bBuildingMode && !bMovingBuilding)
	{
		BeginMoveSelectedBuilding();
	}
	if (bBuildKeyDown && !bPreviousBuildKeyDown && !bMovingBuilding)
	{
		ToggleBuildingMode();
	}
	if ((bBuildingMode || bMovingBuilding) && bRotateKeyDown && !bPreviousRotateKeyDown)
	{
		RotateBuilding();
	}
	if (bMovingBuilding && bPlaceKeyDown && !bPreviousPlaceKeyDown)
	{
		ConfirmMoveBuilding();
	}
	else if (bBuildingMode && bPlaceKeyDown && !bPreviousPlaceKeyDown)
	{
		PlaceBuilding();
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BuildingNumberKeys); ++Index)
	{
		const bool bNumberKeyDown = PlayerController->IsInputKeyDown(BuildingNumberKeys[Index]);
		if (bBuildingMode && bNumberKeyDown && !bPreviousBuildingNumberKeyDown[Index])
		{
			SelectBuildingByIndex(Index);
		}
		bPreviousBuildingNumberKeyDown[Index] = bNumberKeyDown;
	}

	bPreviousBuildKeyDown = bBuildKeyDown;
	bPreviousRotateKeyDown = bRotateKeyDown;
	bPreviousPlaceKeyDown = bPlaceKeyDown;
	bPreviousMoveKeyDown = bMoveKeyDown;
	bPreviousEscapeKeyDown = bEscapeKeyDown;
}
