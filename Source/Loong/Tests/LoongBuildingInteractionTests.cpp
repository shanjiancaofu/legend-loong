#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongBuildingSystemComponent.h"
#include "../LoongBuildingInstanceComponent.h"
#include "../BuildPlacementLibrary.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBuildingInteractionTest, "Loong.Building.Interaction.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongBuildingInteractionTest::RunTest(const FString& Parameters)
{
 const FName TestWorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("LoongBuildingInteractionWorld"));
 const UWorld::InitializationValues InitValues = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TestWorldName, GetTransientPackage(), true, ERHIFeatureLevel::SM6, &InitValues);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 AActor* Owner = World->SpawnActor<AActor>();
 auto* System = NewObject<ULoongBuildingSystemComponent>(Owner); Owner->AddInstanceComponent(System); System->RegisterComponent();
 UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
 for (auto& D : System->BuildingDefinitions) D.BuildingMesh = Cube;
 auto Make = [&](int32 Index, FVector Position)
 {
  System->SelectedBuildingIndex = Index;
  AActor* A = System->SpawnBuildingActor(FTransform(FRotator::ZeroRotator, Position), false);
  return A;
 };
 AActor* A = Make(0, FVector::ZeroVector);
 auto* Identity = A->FindComponentByClass<ULoongBuildingInstanceComponent>();
 const FGuid OriginalId = Identity->InstanceId;
 auto* OriginalMesh = A->FindComponentByClass<UStaticMeshComponent>();
 UMaterialInterface* OriginalMaterial = OriginalMesh->GetMaterial(0);
 const bool OriginalDepth = OriginalMesh->bRenderCustomDepth != 0;
 // Test the actual footprints of all configured sizes at every quarter turn.
 for (int32 Index=0; Index<System->BuildingDefinitions.Num(); ++Index)
 {
  const auto& Definition = System->BuildingDefinitions[Index];
  const FVector2D Half(Definition.GridWidth * 100.f, Definition.GridHeight * 100.f);
  AActor* Existing = Make(Index, FVector(10000,10000,0));
  for (int32 Turn=0; Turn<4; ++Turn)
  {
   const FRotator Rotation(0,Turn*90.f,0);
   Existing->SetActorRotation(Rotation);
   const FVector Center = Existing->GetActorLocation();
   const FVector Touch = Center + Rotation.RotateVector(FVector(Half.X+100.f,0,0));
   TestFalse(TEXT("Different size footprints touching are allowed"), UBuildPlacementLibrary::DoBuildingFootprintsOverlap(Existing,Touch,FVector2D(100,100),Rotation));
   const FVector Intersect = Touch - Rotation.RotateVector(FVector(100,0,0));
   TestTrue(TEXT("Crossing footprints are rejected"), UBuildPlacementLibrary::DoBuildingFootprintsOverlap(Existing,Intersect,FVector2D(100,100),Rotation));
  }
  Existing->Destroy();
 }
 System->SelectedBuildingIndex = 0;

 TestTrue(TEXT("Spawn creates managed identity"), System->IsManagedBuildingActor(A) && OriginalId.IsValid());
 AActor* Decoration = World->SpawnActor<AActor>();
 System->SelectBuildingActor(Decoration);
 TestEqual(TEXT("Unmanaged actors cannot be selected"), System->GetInteractionState(), ELoongBuildingInteractionState::Default);
 System->SelectBuildingActor(A);
 TestEqual(TEXT("Selection state"), System->GetInteractionState(), ELoongBuildingInteractionState::Selected);
 System->SetBuildingMode(true);
 TestEqual(TEXT("Building mode clears selection"), System->GetInteractionState(), ELoongBuildingInteractionState::Building);
 TestNull(TEXT("Building clears selected reference"), System->GetSelectedActor());
 System->SetBuildingMode(false);
 TestTrue(TEXT("Deselect restores original material and depth"), OriginalMesh->GetMaterial(0) == OriginalMaterial && (OriginalMesh->bRenderCustomDepth != 0) == OriginalDepth);
 TestEqual(TEXT("Building exit"), System->GetInteractionState(), ELoongBuildingInteractionState::Default);
 System->SelectBuildingActor(A);
 const FTransform OriginalTransform = A->GetActorTransform();
 A->SetActorEnableCollision(false);
 System->BeginMoveSelectedBuilding();
 TestEqual(TEXT("Move state"), System->GetInteractionState(), ELoongBuildingInteractionState::Moving);
 TestFalse(TEXT("Type changes rejected in move"), System->SelectBuildingByIndex(2));
 System->SetBuildingMode(true);
 System->RequestDeleteSelectedBuilding();
 TestFalse(TEXT("Move blocks new build and deletion"), System->IsBuildingMode() || System->IsDeleteConfirmationOpen());
 A->SetActorLocation(FVector(400,200,0));
 A->SetActorRotation(FRotator(0,90,0));
 TestFalse(TEXT("No ground hit cannot confirm"), System->ConfirmMoveBuilding());
 System->CancelMoveBuilding();
 TestTrue(TEXT("Cancel restores full transform"), A->GetActorTransform().Equals(OriginalTransform));
 TestFalse(TEXT("Cancel restores originally disabled collision"), A->GetActorEnableCollision());
 TestEqual(TEXT("Cancel keeps GUID"), Identity->InstanceId, OriginalId);
 System->BeginMoveSelectedBuilding();
 const FVector Destination(600,0,0); A->SetActorLocation(Destination);
 System->FinishMoveBuilding(true);
 TestTrue(TEXT("Commit uses same Actor and new transform"), System->GetSelectedActor() == A && A->GetActorLocation().Equals(Destination));
 TestEqual(TEXT("Commit retains instance identity"), Identity->InstanceId, OriginalId);
 // A real physics query checks candidate overlap and self exclusion.
 A->SetActorEnableCollision(true);
 for (auto* Mesh : TInlineComponentArray<UStaticMeshComponent*>(A)) { Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Mesh->SetCollisionResponseToAllChannels(ECR_Block); }
 System->CurrentPlacementCenter = Destination;
 System->CurrentPlacementRotation = FRotator::ZeroRotator;
 System->MovingBuildingActor = A;
 TestTrue(TEXT("Moving Actor excluded from occupancy"), System->IsPlacementClearFiltered());
 System->MovingBuildingActor = nullptr;
 TestFalse(TEXT("Existing building occupies its footprint"), System->IsPlacementClearFiltered());
 System->PlayerController = World->SpawnActor<APlayerController>();
 System->RequestDeleteSelectedBuilding();
 TestTrue(TEXT("Delete opens confirmation"), System->IsDeleteConfirmationOpen());
 TestTrue(TEXT("Modal blocks scene input"), System->IsPointerOverBuildingUI());
 System->CancelDeletePrompt();
 TestTrue(TEXT("Cancel keeps Actor and selection"), IsValid(A) && System->GetSelectedActor() == A);
 System->RequestDeleteSelectedBuilding();
 System->ConfirmDeleteSelectedBuilding();
 System->ConfirmDeleteSelectedBuilding();
 TestNull(TEXT("Deletion clears selection"), System->GetSelectedActor());
 TestFalse(TEXT("Deletion closes modal"), System->IsDeleteConfirmationOpen());
 TestTrue(TEXT("Deletion destroys target only"), A->IsActorBeingDestroyed() && IsValid(Decoration));
 TestTrue(TEXT("Deletion releases occupied footprint"), System->IsPlacementClearFiltered());
 AActor* B = Make(3, FVector(1000,1000,0));
 System->SelectBuildingActor(B); System->RequestDeleteSelectedBuilding(); B->Destroy();
 System->TickComponent(0, LEVELTICK_All, nullptr);
 TestNull(TEXT("Externally destroyed target clears reference"), System->GetSelectedActor());
 TestFalse(TEXT("Externally destroyed target closes modal"), System->IsDeleteConfirmationOpen());
 System->bEndingPlay = true; System->ClearSelectedBuilding(); System->HideBuildingMenu(); System->HideDeletePrompt();
 GEngine->DestroyWorldContext(World);
 World->DestroyWorld(false);
 World->RemoveFromRoot();
 return true;
}
#endif
