#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongCombatArtTest.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMovementCameraTest, "Loong.Combat.Camera.ScreenMovement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMovementCameraTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues()
                          .AllowAudioPlayback(false)
                          .RequiresHitProxies(false)
                          .CreatePhysicsScene(true)
                          .CreateNavigation(false)
                          .CreateAISystem(false)
                          .ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(
        EWorldType::Game, false,
        MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("MovementCameraTest")),
        GetTransientPackage(), true, ERHIFeatureLevel::SM6, &Init);
    auto* PawnClass = LoadClass<ACharacter>(
        nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C"));
    if (!TestNotNull(TEXT("Actual movement blueprint loads"), PawnClass))
    {
        World->DestroyWorld(false);
        World->RemoveFromRoot();
        return false;
    }
    auto* Pawn = World->SpawnActor<ACharacter>(PawnClass);
    auto* Camera = Pawn->FindComponentByClass<UCameraComponent>();
    if (TestNotNull(TEXT("Actual pawn has the Camera used by its movement graph"), Camera))
    {
        for (const float Yaw : {-135.f, -90.f, -45.f, 0.f, 90.f})
        {
            const FRotator View(-55.f, Yaw, 0.f);
            ALoongArtTestCamera::SynchronizePawnMovementCamera(Pawn, View);
            Pawn->SetActorRotation(FRotator(0, Yaw + 90.f, 0));
            const FRotator InputBasis(0, Camera->GetComponentRotation().Yaw, 0);
            const FRotator ScreenBasis(0, Yaw, 0);
            // Reproduce the actual BP graph: flattened Camera rotation -> Forward/Right vectors.
            const FVector Forward = UKismetMathLibrary::GetForwardVector(InputBasis);
            const FVector Right = UKismetMathLibrary::GetRightVector(InputBasis);
            const FVector ScreenForward = ScreenBasis.Vector();
            const FVector ScreenRight = UKismetMathLibrary::GetRightVector(ScreenBasis);
            TestTrue(TEXT("W goes up; S goes down"),
                     Forward.Equals(ScreenForward) && (-Forward).Equals(-ScreenForward));
            TestTrue(TEXT("D goes right; A goes left"),
                     Right.Equals(ScreenRight) && (-Right).Equals(-ScreenRight));
            TestTrue(TEXT("Turning pawn does not rotate the input camera"),
                     Camera->IsUsingAbsoluteRotation());
        }
        ALoongArtTestCamera::SynchronizePawnMovementCamera(Pawn, FRotator(-45.f, 30.f, 0));
        TestTrue(TEXT("Runtime camera yaw changes update input basis"),
                 FMath::IsNearlyEqual(Camera->GetComponentRotation().Yaw, 30.f));
    }
    ALoongArtTestCamera::SynchronizePawnMovementCamera(nullptr, FRotator::ZeroRotator);
    auto* WithoutCamera = World->SpawnActor<ACharacter>();
    ALoongArtTestCamera::SynchronizePawnMovementCamera(WithoutCamera, FRotator::ZeroRotator);
    World->DestroyWorld(false);
    World->RemoveFromRoot();
    return true;
}
#endif
