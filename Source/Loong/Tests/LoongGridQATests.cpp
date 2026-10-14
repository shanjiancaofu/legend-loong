#if WITH_DEV_AUTOMATION_TESTS
#include "../BuildPlacementLibrary.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongSignedGridSnapTest, "Loong.Building.Grid.SnapSignedCoordinates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongSignedGridSnapTest::RunTest(const FString& Parameters)
{
    const TArray<TPair<float, float>> Cases = {{0, 0},     {99, 0},      {100, 200},   {101, 200},
                                               {-99, 0},   {-100, 0},    {-101, -200}, {299, 200},
                                               {300, 400}, {-299, -200}, {-300, -200}};
    for (const auto& Case : Cases)
    {
        const auto Result = UBuildPlacementLibrary::SnapToGrid(FVector(Case.Key, Case.Key, 37), 200);
        TestTrue(TEXT("Signed coordinate snap and half-cell convention"),
                 Result.Equals(FVector(Case.Value, Case.Value, 37)));
        TestTrue(TEXT("Grid snap is idempotent"),
                 UBuildPlacementLibrary::SnapToGrid(Result, 200).Equals(Result));
    }
    const FVector Original(-123, 456, 78);
    TestTrue(TEXT("Invalid grid spacing leaves position intact"),
             UBuildPlacementLibrary::SnapToGrid(Original, 0).Equals(Original));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongGridEdgeTest, "Loong.Building.Grid.EdgeAndQuarterTurns",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongGridEdgeTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Negative quarter turn wraps"), UBuildPlacementLibrary::GetRotatedGridDimensions(2, 3, -1),
              FIntPoint(3, 2));
    TestEqual(TEXT("Large quarter turn wraps"), UBuildPlacementLibrary::GetRotatedGridDimensions(2, 3, 5),
              FIntPoint(3, 2));
    TestEqual(TEXT("Invalid footprint returns zero"),
              UBuildPlacementLibrary::GetRotatedGridDimensions(0, 3, 0), FIntPoint::ZeroValue);
    const FVector2D Center(-1000, 400), Extent(500, 500);
    TestTrue(TEXT("Footprint exactly touching base edge fits"),
             UBuildPlacementLibrary::IsWithinBaseBounds(FVector(-700, 400, 0), Center, Extent,
                                                        FVector2D(200, 300), 0));
    TestFalse(TEXT("One cm beyond base edge fails"),
              UBuildPlacementLibrary::IsWithinBaseBounds(FVector(-699, 400, 0), Center, Extent,
                                                         FVector2D(200, 300), 0));
    TestTrue(TEXT("Rotated footprint at exact edge fits"),
             UBuildPlacementLibrary::IsWithinBaseBounds(FVector(-800, 400, 0), Center, Extent,
                                                        FVector2D(200, 300), 90));
    TestFalse(TEXT("Rotated footprint beyond edge fails"),
              UBuildPlacementLibrary::IsWithinBaseBounds(FVector(-799, 400, 0), Center, Extent,
                                                         FVector2D(200, 300), 90));
    TestTrue(TEXT("Negative anchor offset follows rotated dimensions"),
             UBuildPlacementLibrary::GetBuildingCenterFromGridAnchor(FVector(-400, -600, 25), 2, 3, 200, -1)
                 .Equals(FVector(-200, -500, 25)));
    return true;
}
#endif
