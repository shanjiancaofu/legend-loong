#include "BuildPlacementLibrary.h"

#include "Engine/World.h"
#include "Engine/EngineTypes.h"

FVector UBuildPlacementLibrary::SnapToGrid(const FVector WorldLocation, const float GridSize)
{
	if (GridSize <= KINDA_SMALL_NUMBER)
	{
		return WorldLocation;
	}

	return FVector(
		FMath::GridSnap(WorldLocation.X, GridSize),
		FMath::GridSnap(WorldLocation.Y, GridSize),
		WorldLocation.Z);
}

FRotator UBuildPlacementLibrary::SnapRotationTo90(const FRotator WorldRotation)
{
	FRotator Result = WorldRotation;
	Result.Yaw = FMath::GridSnap(FRotator::NormalizeAxis(Result.Yaw), 90.0f);
	Result.Normalize();
	return Result;
}

bool UBuildPlacementLibrary::IsWithinBaseBounds(const FVector WorldLocation, const FVector2D BaseCenter,
	const FVector2D BaseExtent, const FVector2D FootprintExtent, const float YawDegrees)
{
	if (BaseExtent.X < 0.0f || BaseExtent.Y < 0.0f || FootprintExtent.X < 0.0f || FootprintExtent.Y < 0.0f)
	{
		return false;
	}

	const float Radians = FMath::DegreesToRadians(YawDegrees);
	const float CosAngle = FMath::Abs(FMath::Cos(Radians));
	const float SinAngle = FMath::Abs(FMath::Sin(Radians));
	const FVector2D RotatedExtent(
		CosAngle * FootprintExtent.X + SinAngle * FootprintExtent.Y,
		SinAngle * FootprintExtent.X + CosAngle * FootprintExtent.Y);
	const FVector2D Delta(WorldLocation.X - BaseCenter.X, WorldLocation.Y - BaseCenter.Y);

	return FMath::Abs(Delta.X) + RotatedExtent.X <= BaseExtent.X + KINDA_SMALL_NUMBER
		&& FMath::Abs(Delta.Y) + RotatedExtent.Y <= BaseExtent.Y + KINDA_SMALL_NUMBER;
}

bool UBuildPlacementLibrary::CheckPlacementClear(UObject* WorldContextObject, const FVector WorldLocation,
	const FVector BoxExtent, const FRotator WorldRotation, const ECollisionChannel TraceChannel,
	AActor* ActorToIgnore)
{
	if (!WorldContextObject || BoxExtent.X < 0.0f || BoxExtent.Y < 0.0f || BoxExtent.Z < 0.0f)
	{
		return false;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
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
	return !World->OverlapBlockingTestByChannel(WorldLocation, WorldRotation.Quaternion(), TraceChannel, Shape, QueryParams);
}
