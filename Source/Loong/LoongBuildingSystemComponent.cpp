#include "LoongBuildingSystemComponent.h"

#include "BuildPlacementLibrary.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"

ULoongBuildingSystemComponent::ULoongBuildingSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
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

	if (bStartInBuildMode)
	{
		SetBuildingMode(true);
	}
}

void ULoongBuildingSystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	if (bBuildingMode)
	{
		UpdatePreview();
	}
}

void ULoongBuildingSystemComponent::ToggleBuildingMode()
{
	SetBuildingMode(!bBuildingMode);
}

void ULoongBuildingSystemComponent::SetBuildingMode(const bool bEnabled)
{
	bBuildingMode = bEnabled;
	bCurrentPlacementValid = false;
	if (bBuildingMode)
	{
		CreatePreviewActor();
	}
	else
	{
		DestroyPreviewActor();
	}
}

void ULoongBuildingSystemComponent::RotateBuilding()
{
	CurrentPlacementRotation = UBuildPlacementLibrary::SnapRotationTo90(
		CurrentPlacementRotation + FRotator(0.0f, 90.0f, 0.0f));
	UpdatePreview();
}

bool ULoongBuildingSystemComponent::PlaceBuilding()
{
	if (!bBuildingMode || !bCurrentPlacementValid || !BuildingClass || !GetWorld())
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* PlacedActor = GetWorld()->SpawnActor<AActor>(BuildingClass, CurrentPlacementLocation,
		CurrentPlacementRotation, SpawnParams);
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
	if (!PreviewActor || !BuildingClass)
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

	CurrentPlacementLocation = UBuildPlacementLibrary::SnapToGrid(
		GroundHit.ImpactPoint + FVector(0.0f, 0.0f, BuildingPivotZOffset), GridSize);
	PreviewActor->SetActorLocationAndRotation(CurrentPlacementLocation, CurrentPlacementRotation);

	bCurrentPlacementValid = IsInsideBaseBounds() && IsPlacementClearFiltered();
	SetPreviewMaterial(bCurrentPlacementValid ? PreviewValidMaterial : PreviewInvalidMaterial);
}

void ULoongBuildingSystemComponent::CreatePreviewActor()
{
	if (PreviewActor || !BuildingClass || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = GetWorld()->SpawnActor<AActor>(BuildingClass, FVector::ZeroVector,
		FRotator::ZeroRotator, SpawnParams);
	if (PreviewActor)
	{
		PreviewActor->SetActorEnableCollision(false);
		PreviewActor->SetActorHiddenInGame(false);
		SetPreviewMaterial(PreviewInvalidMaterial);
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

	const FVector TraceEnd = RayOrigin + RayDirection * 100000.0f;
	return GetWorld()->LineTraceSingleByChannel(OutHit, RayOrigin, TraceEnd,
		ResolveGroundTraceChannel(), QueryParams);
}

bool ULoongBuildingSystemComponent::IsPlacementClearFiltered() const
{
	if (!GetWorld() || !BuildingClass)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LoongBuildingOverlap), false);
	QueryParams.AddIgnoredActor(GetOwner());
	if (PreviewActor)
	{
		QueryParams.AddIgnoredActor(PreviewActor);
	}

	const FCollisionShape Shape = FCollisionShape::MakeBox(
		FVector(BuildingFootprintHalfExtent.X, BuildingFootprintHalfExtent.Y, BuildingHalfHeight));
	TArray<FOverlapResult> Overlaps;
	if (!GetWorld()->OverlapMultiByChannel(Overlaps, CurrentPlacementLocation,
		CurrentPlacementRotation.Quaternion(), PlacementCollisionChannel, Shape, QueryParams))
	{
		return true;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* HitActor = Overlap.GetActor();
		if (HitActor && HitActor != PreviewActor && HitActor->IsA(BuildingClass))
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

	FVector BoundsOrigin;
	FVector BoundsExtent;
	BaseGroundActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	return UBuildPlacementLibrary::IsWithinBaseBounds(CurrentPlacementLocation,
		FVector2D(BoundsOrigin.X, BoundsOrigin.Y), FVector2D(BoundsExtent.X, BoundsExtent.Y),
		BuildingFootprintHalfExtent, CurrentPlacementRotation.Yaw);
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

	if (bBuildKeyDown && !bPreviousBuildKeyDown)
	{
		ToggleBuildingMode();
	}
	if (bBuildingMode && bRotateKeyDown && !bPreviousRotateKeyDown)
	{
		RotateBuilding();
	}
	if (bBuildingMode && bPlaceKeyDown && !bPreviousPlaceKeyDown)
	{
		PlaceBuilding();
	}

	bPreviousBuildKeyDown = bBuildKeyDown;
	bPreviousRotateKeyDown = bRotateKeyDown;
	bPreviousPlaceKeyDown = bPlaceKeyDown;
}
