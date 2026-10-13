#include "LoongCombatArtTest.h"
#include "LoongBuildingSystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

ALoongArtTestCamera::ALoongArtTestCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    GetCameraComponent()->bConstrainAspectRatio = false;
}

FVector ALoongArtTestCamera::ComputeCameraLocation(FVector Focus, float Pitch, float Yaw, float Distance)
{
    return Focus - FRotator(Pitch, Yaw, 0).Vector() * FMath::Max(100.f, Distance);
}

void ALoongArtTestCamera::ApplyCamera(FVector Focus)
{
    const FRotator Rotation(FMath::Clamp(CameraPitch, -85.f, -15.f), CameraYaw, 0);
    SetActorLocationAndRotation(ComputeCameraLocation(Focus, Rotation.Pitch, Rotation.Yaw, CameraDistance),
                                Rotation);
    auto* C = GetCameraComponent();
    C->SetProjectionMode(bOrthographic ? ECameraProjectionMode::Orthographic
                                       : ECameraProjectionMode::Perspective);
    C->SetOrthoWidth(FMath::Max(100.f, OrthographicWidth));
    C->SetFieldOfView(FMath::Clamp(PerspectiveFOV, 10.f, 90.f));
    C->PostProcessSettings.bOverride_AutoExposureMethod = true;
    C->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    C->PostProcessSettings.bOverride_AutoExposureBias = true;
    C->PostProcessSettings.AutoExposureBias = 0;
    C->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    C->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    C->PostProcessSettings.bOverride_MotionBlurAmount = true;
    C->PostProcessSettings.MotionBlurAmount = 0;
    C->PostProcessSettings.bOverride_BloomIntensity = true;
    C->PostProcessSettings.BloomIntensity = 0.15f;
    C->PostProcessBlendWeight = 1;
}

void ALoongArtTestCamera::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyCamera(IsValid(FocusActor) ? FocusActor->GetActorLocation() + FocusOffset : FocusOffset);
}

void ALoongArtTestCamera::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!LocalController.IsValid())
        LocalController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!LocalController.IsValid() || !LocalController->IsLocalController())
        return;
    if (bFollowPlayer)
        FocusActor = LocalController->GetPawn();
    ApplyCamera(IsValid(FocusActor) ? FocusActor->GetActorLocation() + FocusOffset : FocusOffset);
    LocalController->bAutoManageActiveCameraTarget = false;
    if (LocalController->GetViewTarget() != this)
        LocalController->SetViewTarget(this);
}

ALoongCombatArtTestGameMode::ALoongCombatArtTestGameMode()
{
    Tags.AddUnique(TEXT("LoongCombatArtTest"));
    static ConstructorHelpers::FClassFinder<APawn> Hero(TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter"));
    static ConstructorHelpers::FClassFinder<APlayerController> Controller(
        TEXT("/Game/TopDown/Blueprints/BP_TopDownController"));
    if (Hero.Succeeded())
        DefaultPawnClass = Hero.Class;
    if (Controller.Succeeded())
        PlayerControllerClass = Controller.Class;
}

void ALoongCombatArtTestGameMode::DisableBaseSystems(APawn* Pawn)
{
    if (!Pawn)
        return;
    TInlineComponentArray<ULoongBuildingSystemComponent*> Systems(Pawn);
    for (auto* S : Systems)
    {
        S->bEnableBasePersistence = false;
        S->bEnableDirectKeyboardInput = false;
        S->bStartInBuildMode = false;
        S->SetComponentTickEnabled(false);
    }
}

APawn* ALoongCombatArtTestGameMode::SpawnDefaultPawnAtTransform_Implementation(
    AController* NewPlayer, const FTransform& SpawnTransform)
{
    UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
    if (!PawnClass || !GetWorld())
        return nullptr;
    APawn* Pawn = GetWorld()->SpawnActorDeferred<APawn>(
        PawnClass, SpawnTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Pawn)
        return nullptr;
    DisableBaseSystems(Pawn); // before pawn BeginPlay can touch the player's base slot
    UGameplayStatics::FinishSpawningActor(Pawn, SpawnTransform);
    return Pawn;
}

ALoongCombatArtTestScene::ALoongCombatArtTestScene()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    WindComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindPreview"));
    WindComponent->SetupAttachment(GetRootComponent());
    WindComponent->SetAutoActivate(false);
}

void ALoongCombatArtTestScene::BeginPlay()
{
    Super::BeginPlay();
    // No class search per frame and no asset loading inside Tick.
    if (bSpawnVisualPopulation)
    {
        auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        for (int32 i = 0; i < FMath::Clamp(EnemyCount, 0, 30); ++i)
        {
            const float Angle = 2 * PI * i / FMath::Max(1, EnemyCount);
            const FVector Position = GetActorLocation() + FVector(FMath::Cos(Angle) * PopulationRadius,
                                                                  FMath::Sin(Angle) * PopulationRadius, 100);
            UClass* Class = Palette ? (i % 5 == 0 ? Palette->HeavyEnemyClass.LoadSynchronous()
                                                  : Palette->SmallEnemyClass.LoadSynchronous())
                                    : nullptr;
            AActor* Enemy = nullptr;
            if (Class)
                Enemy = GetWorld()->SpawnActor<AActor>(Class, Position, FRotator::ZeroRotator);
            else
            {
                auto* Placeholder = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
                if (Placeholder)
                {
                    Placeholder->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                    Placeholder->GetStaticMeshComponent()->SetStaticMesh(Cube);
                    Placeholder->SetActorScale3D(i % 5 == 0 ? FVector(1.4f, 1.4f, 2.f)
                                                            : FVector(0.7f, 0.7f, 1.3f));
                    Placeholder->SetActorEnableCollision(false);
                    Enemy = Placeholder;
                }
            }
            if (Enemy)
                Population.Add(Enemy);
        }
    }
    if (Palette && !Palette->DragonActorClass.IsNull())
        Dragon =
            GetWorld()->SpawnActor<AActor>(Palette->DragonActorClass.LoadSynchronous(),
                                           GetActorLocation() + DragonFollowOffset, FRotator::ZeroRotator);
    else
    {
        auto* Placeholder = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation() + DragonFollowOffset,
                                                                     FRotator::ZeroRotator);
        if (Placeholder)
        {
            Placeholder->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Placeholder->GetStaticMeshComponent()->SetStaticMesh(
                LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
            Placeholder->SetActorScale3D(FVector(1.4, 0.7, 0.7));
            Placeholder->SetActorEnableCollision(false);
            Dragon = Placeholder;
        }
    }
    if (Palette)
        WindComponent->SetAsset(Palette->WindWhirl.LoadSynchronous());
    SetWindEffectVisible(bShowWindEffect);
    CaptureStart = PreviousFrameTime = FPlatformTime::Seconds();
    UE_LOG(
        LogTemp, Log,
        TEXT(
            "Loong ArtTest: visual population=%d. Placeholder population does NOT imply automatic combat is integrated."),
        Population.Num());
}

void ALoongCombatArtTestScene::SetWindEffectVisible(bool bVisible)
{
    bShowWindEffect = bVisible;
    WindComponent->SetVariableFloat(TEXT("User.VisualRadius"), WindVisualRadius);
    WindComponent->SetVariableFloat(TEXT("User.DamageRadiusReference"), WindGameplayRadiusReference);
    if (bVisible && WindComponent->GetAsset())
        WindComponent->Activate(true);
    else
        WindComponent->Deactivate();
}

void ALoongCombatArtTestScene::ShowUpgradePreview()
{
    if (UpgradePreview || !Palette || Palette->UpgradeWidgetClass.IsNull())
        return;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetLocalPlayer())
        return;
    UpgradePreview = CreateWidget<UUserWidget>(PC, Palette->UpgradeWidgetClass.LoadSynchronous());
    if (UpgradePreview)
        UpgradePreview->AddToViewport();
}

void ALoongCombatArtTestScene::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (IsValid(Dragon) && PC && PC->GetPawn())
        Dragon->SetActorLocation(PC->GetPawn()->GetActorLocation() + DragonFollowOffset);
    const double Now = FPlatformTime::Seconds();
    if (bRecordFrameTimes && !bCaptureWritten)
    {
        if (Now - CaptureStart > 5 && PreviousFrameTime > 0)
            FrameTimes.Add((Now - PreviousFrameTime) * 1000);
        if (Now - CaptureStart >= FMath::Clamp(CaptureSeconds, 6.f, 300.f))
            WriteCapture();
    }
    PreviousFrameTime = Now;
}

void ALoongCombatArtTestScene::WriteCapture()
{
    bCaptureWritten = true;
    FString CSV = TEXT("frame,wall_frame_ms\n");
    for (int32 i = 0; i < FrameTimes.Num(); ++i)
        CSV += FString::Printf(TEXT("%d,%.4f\n"), i, FrameTimes[i]);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("ArtTest");
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString File = Directory / FString::Printf(TEXT("FrameTimes_%s.csv"),
                                                     *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    if (!FFileHelper::SaveStringToFile(CSV, *File))
    {
        UE_LOG(LogTemp, Error, TEXT("Loong ArtTest: failed capture write %s"), *File);
    }
    else
    {
        UE_LOG(
            LogTemp, Log,
            TEXT(
                "Loong ArtTest: %d wall-frame samples -> %s (not GPU timings; not target-device certification)"),
            FrameTimes.Num(), *File);
    }
}

void ALoongCombatArtTestScene::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bRecordFrameTimes && !bCaptureWritten)
        WriteCapture();
    if (UpgradePreview)
        UpgradePreview->RemoveFromParent();
    for (auto& A : Population)
        if (IsValid(A))
            A->Destroy();
    if (IsValid(Dragon))
        Dragon->Destroy();
    Super::EndPlay(Reason);
}
