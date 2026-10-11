#if WITH_DEV_AUTOMATION_TESTS

#include "../BuildPlacementLibrary.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBuildingGridDimensionsTest,
	"Loong.Building.Grid.DimensionsAndRotation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLoongBuildingGridDimensionsTest::RunTest(const FString& Parameters)
{
	const TArray<TPair<FIntPoint, FIntPoint>> Cases = {
		{FIntPoint(1, 1), FIntPoint(1, 1)},
		{FIntPoint(1, 2), FIntPoint(2, 1)},
		{FIntPoint(2, 2), FIntPoint(2, 2)},
		{FIntPoint(2, 3), FIntPoint(3, 2)}
	};

	for (const TPair<FIntPoint, FIntPoint>& TestCase : Cases)
	{
		for (int32 QuarterTurns = 0; QuarterTurns < 4; ++QuarterTurns)
		{
			const FIntPoint Expected = (QuarterTurns % 2 == 0) ? TestCase.Key : TestCase.Value;
			const FIntPoint Actual = UBuildPlacementLibrary::GetRotatedGridDimensions(
				TestCase.Key.X, TestCase.Key.Y, QuarterTurns);
			TestEqual(TEXT("Rotated grid dimensions"), Actual, Expected);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBuildingGridAnchorTest,
	"Loong.Building.Grid.AnchorAndCenter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLoongBuildingGridAnchorTest::RunTest(const FString& Parameters)
{
	const FVector Anchor = FVector::ZeroVector;
	const FVector Center1x1 = UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(Anchor, 1, 1, 200.0f, 0);
	const FVector Center1x2 = UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(Anchor, 1, 2, 200.0f, 0);
	const FVector Center1x2Rotated = UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(Anchor, 1, 2, 200.0f, 1);

	TestTrue(TEXT("1x1 keeps the existing grid origin convention"), Center1x1.Equals(FVector::ZeroVector));
	TestTrue(TEXT("1x2 receives a 100cm Y center offset"), Center1x2.Equals(FVector(0.0f, 100.0f, 0.0f)));
	TestTrue(TEXT("1x2 rotated receives a 100cm X center offset"), Center1x2Rotated.Equals(FVector(100.0f, 0.0f, 0.0f)));

	const FVector2D Extent1x2 = UBuildPlacementLibrary::GetBuildingFootprintHalfExtent(1, 2, 200.0f, 0);
	const FVector2D Extent1x2Rotated = UBuildPlacementLibrary::GetBuildingFootprintHalfExtent(1, 2, 200.0f, 1);
	TestTrue(TEXT("1x2 footprint is 200x400cm"), Extent1x2.Equals(FVector2D(100.0f, 200.0f)));
	TestTrue(TEXT("1x2 rotated footprint is 400x200cm"), Extent1x2Rotated.Equals(FVector2D(200.0f, 100.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBuildingBoundsCornerTest,
	"Loong.Building.Grid.RotatedCorners",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLoongBuildingBoundsCornerTest::RunTest(const FString& Parameters)
{
	const FVector2D BaseCenter = FVector2D::ZeroVector;
	const FVector2D BaseExtent(500.0f, 500.0f);
	TestTrue(TEXT("2x3 fits inside a 1000x1000 base"),
		UBuildPlacementLibrary::IsWithinBaseBounds(FVector::ZeroVector, BaseCenter, BaseExtent,
			FVector2D(200.0f, 300.0f), 0.0f));
	TestFalse(TEXT("2x3 rotated at the edge is rejected"),
		UBuildPlacementLibrary::IsWithinBaseBounds(FVector(300.0f, 0.0f, 0.0f), BaseCenter, BaseExtent,
			FVector2D(200.0f, 300.0f), 90.0f));
	return true;
}

#endif
