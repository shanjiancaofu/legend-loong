#include "BuildPlacementLibrary.h"

#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

namespace
{
bool IsPointInsideLocalGroundBounds(const FVector& WorldPoint, const UPrimitiveComponent* GroundComponent)
{
    if (!GroundComponent)
    {
        return false;
    }

    const FBox LocalBox = GroundComponent->GetLocalBounds().GetBox();
    if (LocalBox.IsValid == 0)
    {
        return false;
    }
    const FVector LocalPoint = GroundComponent->GetComponentTransform().InverseTransformPosition(WorldPoint);
    return LocalPoint.X >= LocalBox.Min.X - KINDA_SMALL_NUMBER &&
           LocalPoint.X <= LocalBox.Max.X + KINDA_SMALL_NUMBER &&
           LocalPoint.Y >= LocalBox.Min.Y - KINDA_SMALL_NUMBER &&
           LocalPoint.Y <= LocalBox.Max.Y + KINDA_SMALL_NUMBER;
}

UPrimitiveComponent* FindGroundPrimitive(AActor* GroundActor)
{
    if (!GroundActor)
    {
        return nullptr;
    }

    TArray<UStaticMeshComponent*> StaticMeshes;
    GroundActor->GetComponents<UStaticMeshComponent>(StaticMeshes);
    for (UStaticMeshComponent* StaticMesh : StaticMeshes)
    {
        if (StaticMesh && StaticMesh->IsRegistered() && StaticMesh->GetStaticMesh())
        {
            return StaticMesh;
        }
    }

    TArray<UPrimitiveComponent*> Primitives;
    GroundActor->GetComponents<UPrimitiveComponent>(Primitives);
    for (UPrimitiveComponent* Primitive : Primitives)
    {
        if (Primitive && Primitive->IsRegistered())
        {
            return Primitive;
        }
    }
    return nullptr;
}

bool GetActorPrimitiveLocalBounds(const AActor* Actor, FBox& OutBounds)
{
    if (!Actor)
    {
        return false;
    }

    TArray<UPrimitiveComponent*> Primitives;
    Actor->GetComponents<UPrimitiveComponent>(Primitives);
    for (UPrimitiveComponent* Primitive : Primitives)
    {
        if (!Primitive || !Primitive->IsRegistered())
        {
            continue;
        }

        const FTransform ComponentInActor =
            Primitive->GetComponentTransform().GetRelativeTransform(Actor->GetActorTransform());
        OutBounds += Primitive->GetLocalBounds().GetBox().TransformBy(ComponentInActor);
    }
    return OutBounds.IsValid != 0;
}
}

FVector UBuildPlacementLibrary::SnapToGrid(const FVector WorldLocation, const float GridSize)
{
    if (GridSize <= KINDA_SMALL_NUMBER)
    {
        return WorldLocation;
    }

    return FVector(FMath::GridSnap(WorldLocation.X, GridSize), FMath::GridSnap(WorldLocation.Y, GridSize),
                   WorldLocation.Z);
}

FRotator UBuildPlacementLibrary::SnapRotationTo90(const FRotator WorldRotation)
{
    FRotator Result = WorldRotation;
    Result.Yaw = FMath::GridSnap(FRotator::NormalizeAxis(Result.Yaw), 90.0f);
    Result.Normalize();
    return Result;
}

FIntPoint UBuildPlacementLibrary::GetRotatedGridDimensions(const int32 GridWidth, const int32 GridHeight,
                                                           const int32 RotationQuarterTurns)
{
    if (GridWidth <= 0 || GridHeight <= 0)
    {
        return FIntPoint::ZeroValue;
    }

    const int32 NormalizedTurns = ((RotationQuarterTurns % 4) + 4) % 4;
    return (NormalizedTurns % 2 == 0) ? FIntPoint(GridWidth, GridHeight) : FIntPoint(GridHeight, GridWidth);
}

int32 UBuildPlacementLibrary::GetRotationQuarterTurns(const float YawDegrees)
{
    const float NormalizedYaw = FRotator::NormalizeAxis(YawDegrees);
    const int32 QuarterTurns = FMath::RoundToInt(NormalizedYaw / 90.0f);
    return ((QuarterTurns % 4) + 4) % 4;
}

FVector UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(const FVector GridAnchor,
                                                                const int32 GridWidth, const int32 GridHeight,
                                                                const float GridSize,
                                                                const int32 RotationQuarterTurns)
{
    const FIntPoint RotatedDimensions = GetRotatedGridDimensions(GridWidth, GridHeight, RotationQuarterTurns);
    if (RotatedDimensions.X <= 0 || RotatedDimensions.Y <= 0 || GridSize <= KINDA_SMALL_NUMBER)
    {
        return GridAnchor;
    }

    return GridAnchor + FVector(0.5f * static_cast<float>(RotatedDimensions.X - 1) * GridSize,
                                0.5f * static_cast<float>(RotatedDimensions.Y - 1) * GridSize, 0.0f);
}

FVector2D UBuildPlacementLibrary::GetBuildingFootprintHalfExtent(const int32 GridWidth,
                                                                 const int32 GridHeight, const float GridSize,
                                                                 const int32 RotationQuarterTurns)
{
    const FIntPoint RotatedDimensions = GetRotatedGridDimensions(GridWidth, GridHeight, RotationQuarterTurns);
    if (RotatedDimensions.X <= 0 || RotatedDimensions.Y <= 0 || GridSize <= KINDA_SMALL_NUMBER)
    {
        return FVector2D::ZeroVector;
    }

    return FVector2D(0.5f * static_cast<float>(RotatedDimensions.X) * GridSize,
                     0.5f * static_cast<float>(RotatedDimensions.Y) * GridSize);
}

bool UBuildPlacementLibrary::IsWithinBaseBounds(const FVector WorldLocation, const FVector2D BaseCenter,
                                                const FVector2D BaseExtent, const FVector2D FootprintExtent,
                                                const float YawDegrees)
{
    if (BaseExtent.X < 0.0f || BaseExtent.Y < 0.0f || FootprintExtent.X < 0.0f || FootprintExtent.Y < 0.0f)
    {
        return false;
    }

    const FQuat FootprintRotation(FRotator(0.0f, YawDegrees, 0.0f));
    for (const float SignX : {-1.0f, 1.0f})
    {
        for (const float SignY : {-1.0f, 1.0f})
        {
            const FVector Corner =
                WorldLocation + FootprintRotation.RotateVector(
                                    FVector(SignX * FootprintExtent.X, SignY * FootprintExtent.Y, 0.0f));
            if (FMath::Abs(Corner.X - BaseCenter.X) > BaseExtent.X + KINDA_SMALL_NUMBER ||
                FMath::Abs(Corner.Y - BaseCenter.Y) > BaseExtent.Y + KINDA_SMALL_NUMBER)
            {
                return false;
            }
        }
    }
    return true;
}

bool UBuildPlacementLibrary::IsWithinBaseActorBounds(AActor* BaseGroundActor, const FVector WorldLocation,
                                                     const FVector2D FootprintExtent, const float YawDegrees)
{
    if (!IsValid(BaseGroundActor))
    {
        return false;
    }

    return IsWithinBaseComponentBounds(FindGroundPrimitive(BaseGroundActor), WorldLocation, FootprintExtent,
                                       YawDegrees);
}

bool UBuildPlacementLibrary::IsWithinBaseComponentBounds(UPrimitiveComponent* BaseGroundComponent,
                                                         const FVector WorldLocation,
                                                         const FVector2D FootprintExtent,
                                                         const float YawDegrees)
{
    if (!IsValid(BaseGroundComponent) || FootprintExtent.X < 0.0f || FootprintExtent.Y < 0.0f)
    {
        return false;
    }

    const FQuat FootprintRotation(FRotator(0.0f, YawDegrees, 0.0f));
    for (const float SignX : {-1.0f, 1.0f})
    {
        for (const float SignY : {-1.0f, 1.0f})
        {
            const FVector Corner =
                WorldLocation + FootprintRotation.RotateVector(
                                    FVector(SignX * FootprintExtent.X, SignY * FootprintExtent.Y, 0.0f));
            if (!IsPointInsideLocalGroundBounds(Corner, BaseGroundComponent))
            {
                return false;
            }
        }
    }
    return true;
}

bool UBuildPlacementLibrary::DoBuildingFootprintsOverlap(const AActor* ExistingBuilding,
                                                         const FVector CandidateCenter,
                                                         const FVector2D CandidateHalfExtent,
                                                         const FRotator CandidateRotation,
                                                         const float TouchToleranceCm)
{
    if (!IsValid(ExistingBuilding) || CandidateHalfExtent.X < 0.0f || CandidateHalfExtent.Y < 0.0f)
    {
        return false;
    }

    FBox ExistingLocalBounds(EForceInit::ForceInit);
    if (!GetActorPrimitiveLocalBounds(ExistingBuilding, ExistingLocalBounds))
    {
        return false;
    }

    const FVector ExistingScale = ExistingBuilding->GetActorScale3D().GetAbs();
    const FVector ExistingLocalCenter = ExistingLocalBounds.GetCenter();
    const FVector ExistingCenter =
        ExistingBuilding->GetActorLocation() +
        ExistingBuilding->GetActorRotation().RotateVector(ExistingLocalCenter * ExistingScale);
    const FVector ExistingExtent = ExistingLocalBounds.GetExtent() * ExistingScale;

    const FVector2D ExistingHalfExtent(ExistingExtent.X, ExistingExtent.Y);
    const float ExistingYaw = ExistingBuilding->GetActorRotation().Yaw;
    const float CandidateYaw = CandidateRotation.Yaw;
    const FVector2D ExistingAxisX(FMath::Cos(FMath::DegreesToRadians(ExistingYaw)),
                                  FMath::Sin(FMath::DegreesToRadians(ExistingYaw)));
    const FVector2D ExistingAxisY(-ExistingAxisX.Y, ExistingAxisX.X);
    const FVector2D CandidateAxisX(FMath::Cos(FMath::DegreesToRadians(CandidateYaw)),
                                   FMath::Sin(FMath::DegreesToRadians(CandidateYaw)));
    const FVector2D CandidateAxisY(-CandidateAxisX.Y, CandidateAxisX.X);
    const FVector2D CenterDelta(CandidateCenter.X - ExistingCenter.X, CandidateCenter.Y - ExistingCenter.Y);

    const FVector2D Axes[] = {ExistingAxisX, ExistingAxisY, CandidateAxisX, CandidateAxisY};
    for (const FVector2D& Axis : Axes)
    {
        const float ExistingRadius =
            ExistingHalfExtent.X * FMath::Abs(FVector2D::DotProduct(ExistingAxisX, Axis)) +
            ExistingHalfExtent.Y * FMath::Abs(FVector2D::DotProduct(ExistingAxisY, Axis));
        const float CandidateRadius =
            CandidateHalfExtent.X * FMath::Abs(FVector2D::DotProduct(CandidateAxisX, Axis)) +
            CandidateHalfExtent.Y * FMath::Abs(FVector2D::DotProduct(CandidateAxisY, Axis));
        const float Penetration =
            ExistingRadius + CandidateRadius - FMath::Abs(FVector2D::DotProduct(CenterDelta, Axis));
        if (Penetration <= FMath::Max(0.0f, TouchToleranceCm))
        {
            return false;
        }
    }
    return true;
}

FVector UBuildPlacementLibrary::GetActorLocationForCenteredFootprint(AActor* BuildingActor,
                                                                     const FVector DesiredCenter,
                                                                     const FRotator DesiredRotation)
{
    if (!IsValid(BuildingActor))
    {
        return DesiredCenter;
    }

    FBox LocalBounds(EForceInit::ForceInit);
    if (!GetActorPrimitiveLocalBounds(BuildingActor, LocalBounds))
    {
        return DesiredCenter;
    }

    const FVector LocalFootprintCenter(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, 0.0f);
    const FTransform RotationAndScale(DesiredRotation, FVector::ZeroVector, BuildingActor->GetActorScale3D());
    return DesiredCenter - RotationAndScale.TransformVector(LocalFootprintCenter);
}

bool UBuildPlacementLibrary::CheckPlacementClear(UObject* WorldContextObject, const FVector WorldLocation,
                                                 const FVector BoxExtent, const FRotator WorldRotation,
                                                 const ECollisionChannel TraceChannel, AActor* ActorToIgnore)
{
    if (!WorldContextObject || BoxExtent.X < 0.0f || BoxExtent.Y < 0.0f || BoxExtent.Z < 0.0f)
    {
        return false;
    }

    const UWorld* World =
        GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
                : nullptr;
    if (!World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BuildPlacement), false);
    if (ActorToIgnore)
    {
        QueryParams.AddIgnoredActor(ActorToIgnore);
    }

    const FCollisionShape Shape = FCollisionShape::MakeBox(BoxExtent);
    return !World->OverlapBlockingTestByChannel(WorldLocation, WorldRotation.Quaternion(), TraceChannel,
                                                Shape, QueryParams);
}
