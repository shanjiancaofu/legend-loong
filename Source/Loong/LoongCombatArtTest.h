#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/DataAsset.h"
#include "LoongCombatArtTest.generated.h"
class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class UNiagaraSystem;
class UNiagaraComponent;
class UUserWidget;
class APlayerController;

/** Only visual test resources; never a building definition or combat state manager. */
UCLASS(BlueprintType)
class LOONG_API ULoongCombatArtPalette : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character") TSoftObjectPtr<USkeletalMesh> HeroMesh;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character") TSoftObjectPtr<USkeletalMesh> WindDragonMesh;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character") TSoftClassPtr<AActor> DragonActorClass;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies") TSoftClassPtr<AActor> SmallEnemyClass;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies") TSoftClassPtr<AActor> HeavyEnemyClass;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment") TArray<TSoftObjectPtr<UStaticMesh>> ForestRuinMeshes;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials") TSoftObjectPtr<UMaterialInterface> CharacterMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials") TSoftObjectPtr<UMaterialInterface> EnvironmentMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials") TSoftObjectPtr<UMaterialInterface> GroundMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX") TSoftObjectPtr<UNiagaraSystem> WindWhirl;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI") TSoftClassPtr<UUserWidget> UpgradeWidgetClass;
};

UCLASS()
class LOONG_API ALoongArtTestCamera : public ACameraActor
{
 GENERATED_BODY()
public:
 ALoongArtTestCamera();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera") float CameraPitch = -55.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera") float CameraYaw = -45.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera", meta=(ClampMin="100")) float OrthographicWidth = 2600.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera", meta=(ClampMin="100")) float CameraDistance = 2200.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera") float PerspectiveFOV = 35.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera") bool bOrthographic = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera") bool bFollowPlayer = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Camera") FVector FocusOffset = FVector(0,0,80);
 UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="ArtTest|Camera") TObjectPtr<AActor> FocusActor;
 static FVector ComputeCameraLocation(FVector Focus, float Pitch, float Yaw, float Distance);
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void Tick(float DeltaTime) override;
private:
 void ApplyCamera(FVector Focus);
 TWeakObjectPtr<APlayerController> LocalController;
};

/** Map-only override; reuses the existing blueprint pawn/controller, disables base systems before BeginPlay. */
UCLASS()
class LOONG_API ALoongCombatArtTestGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 ALoongCombatArtTestGameMode();
 virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
 static void DisableBaseSystems(APawn* Pawn);
};

/** Visual population test, no AI damage/attack simulation is implied by placeholders. */
UCLASS()
class LOONG_API ALoongCombatArtTestScene : public AActor
{
 GENERATED_BODY()
public:
 ALoongCombatArtTestScene();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Assets") TObjectPtr<ULoongCombatArtPalette> Palette;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Population", meta=(ClampMin="0",ClampMax="30")) int32 EnemyCount = 20;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Population") float PopulationRadius = 800.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Population") bool bSpawnVisualPopulation = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Dragon") FVector DragonFollowOffset = FVector(-150,100,170);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|VFX") bool bShowWindEffect = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|VFX") float WindVisualRadius = 300.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|VFX") float WindGameplayRadiusReference = 300.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Capture") bool bRecordFrameTimes = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArtTest|Capture", meta=(ClampMin="1",ClampMax="300")) float CaptureSeconds = 60.f;
 UFUNCTION(BlueprintCallable, Category="ArtTest") void SetWindEffectVisible(bool bVisible);
 UFUNCTION(BlueprintCallable, Category="ArtTest") void ShowUpgradePreview();
 virtual void Tick(float DeltaTime) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UNiagaraComponent> WindComponent;
 UPROPERTY() TObjectPtr<AActor> Dragon;
 UPROPERTY() TArray<TObjectPtr<AActor>> Population;
 UPROPERTY() TObjectPtr<UUserWidget> UpgradePreview;
 double CaptureStart = 0;
 double PreviousFrameTime = 0;
 TArray<double> FrameTimes;
 bool bCaptureWritten = false;
 void WriteCapture();
};
