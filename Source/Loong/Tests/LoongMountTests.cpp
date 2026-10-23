#if WITH_DEV_AUTOMATION_TESTS
#include "LoongAutomationTestWorld.h"
#include "../LoongMountPawn.h"
#include "../LoongCombatWidget.h"
#include "../LoongBuildingMenuButton.h"
#include "../LoongBuildingSystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Misc/AutomationTest.h"

namespace
{
struct FMountedFixture : LoongTests::FScopedCombat
{
    APlayerController* Controller = nullptr;
    FMountedFixture()
    {
        Controller = World->SpawnActor<APlayerController>();
        Controller->Possess(Player);
        Player->SetActorLocation(FVector(0, 0, 100));
        Director->Definition->Dragons.Add(FLoongDragonAbility());
        Director->SelectCompanion(TEXT("WindDragonTest"));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionSelectionTest, "Loong.Combat.Mount.SelectionAndRecall",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionSelectionTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    FLoongDragonAbility Passive = F.Director->Definition->ElementDragonPresets[0];
    Passive.bSpawnCompanion = false;
    F.Director->EquipDragon(Passive);
    auto* Old = F.Director->GetSelectedCompanion();
    auto* Enemy = F.Director->SpawnEnemyAt(FVector(300, 0, 100));
    F.Director->FireProjectile(Old, Enemy, Old->Attack->Stats, false);
    auto* OldShot = F.Director->Projectiles.Last().Get();
    TestTrue(TEXT("Select existing fire preset"), F.Director->SelectCompanion(TEXT("FireDragonTest")));
    TestEqual(TEXT("Selection leaves exactly one entity"), F.Director->Companions.Num(), 1);
    TestTrue(TEXT("Switch cancels old source"),
             Old->IsActorBeingDestroyed() && OldShot->IsActorBeingDestroyed());
    auto* Fire = F.Director->GetSelectedCompanion();
    TestEqual(TEXT("Transient source does not overwrite passive preset"), Fire->Definition.DragonId,
              FName(TEXT("ActiveCompanion_FireDragonTest")));
    TestFalse(TEXT("Unknown selection rejected"), F.Director->SelectCompanion(TEXT("Missing")));
    TestTrue(TEXT("Unknown selection preserves current entity"), F.Director->GetSelectedCompanion() == Fire);
    Fire->SetActorLocation(FVector(4000, 0, 100));
    TestTrue(TEXT("Recall succeeds"), F.Director->RecallCompanion());
    TestTrue(TEXT("Recall actually relocates"),
             Fire->GetActorLocation().Equals(F.Player->GetActorLocation() + Fire->Definition.FollowOffset));
    F.Director->AddExperience(F.Director->ExperienceThreshold());
    TestFalse(TEXT("Selection disabled during upgrade"), F.Director->SelectCompanion(TEXT("IceDragonTest")));
    F.Director->ChooseUpgrade(F.Director->UpgradeChoices[0]);
    TestTrue(TEXT("Unequip through same UI service"), F.Director->UnequipCompanions());
    TestNull(TEXT("No selected entity after unequip"), F.Director->GetSelectedCompanion());
    TestTrue(TEXT("Independent passive still equipped after trial switch/unequip"),
             F.Director->GetElementDebugSummary().Contains(TEXT("FireDragonTest : 1 modules")));
    TestTrue(TEXT("Can re-equip"), F.Director->SelectCompanion(TEXT("WindDragonTest")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountPossessionTest, "Loong.Combat.Mount.PossessionRoundTrip",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountPossessionTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    auto* Dragon = F.Director->GetSelectedCompanion();
    for (int32 i = 0; i < 3; ++i)
    {
        TestTrue(TEXT("Mount succeeds"), F.Director->ToggleMount());
        auto* Mount = F.Director->MountPawn.Get();
        if (!TestNotNull(TEXT("Control host exists"), Mount))
            return false;
        TestTrue(TEXT("Controller actually possesses mount"), F.Controller->GetPawn() == Mount);
        TestTrue(TEXT("Original character remains visible as rider"),
                 F.Player->GetAttachParentActor() == Mount && !F.Player->IsHidden());
        TestTrue(TEXT("Original dragon is attached to mount"), Dragon->GetAttachParentActor() == Mount);
        TestFalse(TEXT("Rider cannot independently move"), F.Player->IsActorTickEnabled());
        TestFalse(TEXT("Rider collision cannot duplicate hit"), F.Player->GetActorEnableCollision());
        TestFalse(TEXT("Hero automatic attack stops"), F.Director->PlayerAttack->IsComponentTickEnabled());
        TestFalse(TEXT("Dragon automatic attacks become manual"), Dragon->Attack->bAutomatic);
        Mount->SetActorLocation(FVector(300 + i * 200, 100, 100));
        const FVector DragonPosition = Dragon->GetActorLocation();
        Dragon->Tick(0.1f);
        TestTrue(TEXT("Mounted dragon does not follow-interpolate"),
                 Dragon->GetActorLocation().Equals(DragonPosition));
        TestTrue(TEXT("Combat target position uses mount not raised rider"),
                 F.Director->GetCombatAvatar() == Mount);
        TestTrue(TEXT("Dismount succeeds"), F.Director->ToggleMount());
        TestTrue(TEXT("Possession returns to original blueprint-compatible pawn"),
                 F.Controller->GetPawn() == F.Player);
        TestTrue(TEXT("Rider collision restored"), F.Player->GetActorEnableCollision());
        TestTrue(TEXT("Rider tick restored"), F.Player->IsActorTickEnabled());
        TestNull(TEXT("Rider detached"), F.Player->GetAttachParentActor());
        TestTrue(TEXT("Automatic attacks restored"),
                 Dragon->Attack->bAutomatic && F.Director->PlayerAttack->IsComponentTickEnabled());
        TestTrue(TEXT("Temporary host destroyed"), Mount->IsActorBeingDestroyed());
        F.Director->RecallCompanion();
    }
    Dragon->SetActorLocation(FVector(6000, 0, 100));
    TestFalse(TEXT("Cannot mount from arbitrary distance"), F.Director->ToggleMount());
    TestTrue(TEXT("Failed mount leaves possession intact"), F.Controller->GetPawn() == F.Player);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountedCombatTest, "Loong.Combat.Mount.ManualCombatAndSharedHealth",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountedCombatTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    F.Director->SelectCompanion(TEXT("FireDragonTest"));
    F.Director->ToggleMount();
    auto* Dragon = F.Director->GetSelectedCompanion();
    auto* Mount = F.Director->MountPawn.Get();
    auto* Enemy = F.Director->SpawnEnemyAt(Mount->GetActorLocation() + FVector(100, 0, 0));
    Dragon->Attack->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Mounting alone does not auto-fire"), F.Director->Projectiles.Num(), 0);
    F.Director->PlayerAttack->TickComponent(1.f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Hero cannot auto-fire even if manually ticked while mounted"),
              F.Director->Projectiles.Num(), 0);
    F.Director->MountedBasicAttack();
    Dragon->Attack->TickComponent(0.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Player command launches existing projectile"), F.Director->Projectiles.Num(), 1);
    auto* Shot = F.Director->Projectiles.Last().Get();
    TestTrue(TEXT("Projectile source remains the real dragon"), Shot->Attacker.Get() == Dragon);
    const float Before = Enemy->Health->CurrentHealth;
    Shot->ResolveHit();
    TestEqual(TEXT("Actual direct damage"), Enemy->Health->CurrentHealth,
              Before - Dragon->Definition.CompanionDamage);
    Dragon->Attack->TickComponent(2, LEVELTICK_All, nullptr);
    const int32 Count = F.Director->Projectiles.Num();
    Dragon->Attack->TickComponent(2, LEVELTICK_All, nullptr);
    TestEqual(TEXT("No input means no further attacks"), F.Director->Projectiles.Num(), Count);
    F.Director->Elapsed += 3;
    Enemy->Elements->ClearEffects();
    TestTrue(TEXT("Active elemental slot applies"), F.Director->CommandCompanionSkill(0));
    TestEqual(TEXT("Active skill belongs only to selected dragon"),
              Enemy->Elements->GetStacks(TEXT("Burn"), Dragon->Definition.DragonId), 1);
    TestFalse(TEXT("Active skill respects cooldown"), F.Director->CommandCompanionSkill(0));
    TestFalse(TEXT("Missing skill rejected"), F.Director->CommandCompanionSkill(2));
    const float Health = F.Director->PlayerHealth->CurrentHealth;
    Enemy->SetActorLocation(Mount->GetActorLocation() + FVector(30, 0, 0));
    Enemy->Tick(0.01f);
    TestTrue(TEXT("Contact maps to shared team health"), F.Director->PlayerHealth->CurrentHealth < Health);
    TestNull(TEXT("Mount does not add a second health component"),
             Mount->FindComponentByClass<ULoongCombatHealthComponent>());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountPauseDashTest, "Loong.Combat.Mount.MovementDashAndPause",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountPauseDashTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    F.Director->ToggleMount();
    auto* Mount = F.Director->MountPawn.Get();
    auto* Input = NewObject<UInputComponent>(Mount);
    Mount->SetupPlayerInputComponent(Input);
    TestTrue(TEXT("Mount input blocks underlying character/controller bindings"), Input->bBlockInput);
    Mount->MoveForward(1);
    TestTrue(TEXT("W generates movement for possessed host"),
             Mount->GetPendingMovementInputVector().Size() > 0);
    Mount->ConsumeMovementInputVector();
    Mount->StartDash();
    Mount->Tick(0.05f);
    TestEqual(TEXT("Dash changes actual movement speed"), Mount->GetCharacterMovement()->MaxWalkSpeed,
              1400.f);
    const float Cooldown = Mount->DashCooldown;
    Mount->StartDash();
    TestEqual(TEXT("Dash cannot retrigger during cooldown"), Mount->DashCooldown, Cooldown);
    F.Director->AddExperience(F.Director->ExperienceThreshold());
    Mount->Tick(5);
    Mount->MoveForward(1);
    TestTrue(TEXT("Paused mount has no movement input"),
             Mount->GetPendingMovementInputVector().IsNearlyZero());
    TestEqual(TEXT("Pause preserves dash clock"), Mount->DashCooldown, Cooldown);
    TestEqual(TEXT("Paused movement disabled"), Mount->GetCharacterMovement()->MovementMode,
              static_cast<uint8>(MOVE_None));
    TestFalse(TEXT("No dismount mutation during upgrade"), F.Director->ToggleMount());
    F.Director->ChooseUpgrade(F.Director->UpgradeChoices[0]);
    Mount->Tick(0.3f);
    TestEqual(TEXT("Resume restores normal speed after dash"), Mount->GetCharacterMovement()->MaxWalkSpeed,
              650.f);
    TestTrue(TEXT("Original rider remains movement-disabled until dismount"),
             F.Player->GetCharacterMovement()->MovementMode == MOVE_None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountCleanupTest, "Loong.Combat.Mount.InterruptionCleanup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountCleanupTest::RunTest(const FString& Parameters)
{
    for (int32 Mode = 0; Mode < 7; ++Mode)
    {
        FMountedFixture F;
        F.Director->ToggleMount();
        auto* Host = F.Director->MountPawn.Get();
        auto* Dragon = F.Director->GetSelectedCompanion();
        if (Mode == 0)
            F.Director->UnequipCompanions();
        if (Mode == 1)
            Dragon->Destroy();
        if (Mode == 2)
            Host->Destroy();
        if (Mode == 3)
            F.Director->PlayerHealth->ApplyCombatDamage(99999, nullptr);
        if (Mode == 4)
            F.Director->EndCombat(true);
        if (Mode == 5)
            F.Director->SelectCompanion(TEXT("IceDragonTest"));
        if (Mode == 6)
        {
            F.Controller->Destroy();
            F.Director->Tick(0.01f);
        }
        TestFalse(TEXT("Interruptions release mount state"), F.Director->IsMounted());
        if (Mode != 6)
            TestTrue(TEXT("Possession restored on interruption"), F.Controller->GetPawn() == F.Player);
        TestTrue(TEXT("Rider collision restored on interruption"), F.Player->GetActorEnableCollision());
        TestNull(TEXT("Rider detached on interruption"), F.Player->GetAttachParentActor());
        TestTrue(TEXT("Host removed on interruption"), Host->IsActorBeingDestroyed());
        if (Mode == 3 || Mode == 4)
            TestEqual(TEXT("Settlement remains once"), F.Director->SettlementCount, 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountBlockedExitTest, "Loong.Combat.Mount.BlockedDismountAndCollision",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountBlockedExitTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    F.Director->ToggleMount();
    auto* Host = F.Director->MountPawn.Get();
    auto* Wall = F.World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(120, 400, 300));
    Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent();
    Wall->SetActorLocation(Host->GetActorLocation() + FVector(350, 0, 0));
    const FVector Start = Host->GetActorLocation();
    FHitResult Hit;
    Host->SetActorLocation(Start + FVector(700, 0, 0), true, &Hit);
    TestTrue(TEXT("Mounted capsule sweep is blocked by wall"),
             Hit.bBlockingHit && Host->GetActorLocation().X < Start.X + 700);
    // Surround all exit candidates; normal exit must retain possession until a safe exit exists.
    Box->SetBoxExtent(FVector(500, 500, 500));
    Wall->SetActorLocation(Host->GetActorLocation());
    TestFalse(TEXT("Blocked dismount rejected"), F.Director->Dismount());
    TestTrue(TEXT("Blocked dismount keeps controller on mount"), F.Controller->GetPawn() == Host);
    Wall->Destroy();
    TestTrue(TEXT("Dismount succeeds once blocker removed"), F.Director->Dismount());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountPanelTest, "Loong.Combat.Mount.PanelButtonWorkflow",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountPanelTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    // No viewport/local player in this transient world; initialize the real native widget tree.
    auto* Panel = NewObject<ULoongCombatWidget>(F.World);
    Panel->Initialize();
    if (!TestNotNull(TEXT("Actual combat widget created"), Panel))
        return false;
    Panel->Setup(F.Director);
    auto Click = [Panel](const FString& Label)
    {
        TArray<UWidget*> Widgets;
        Panel->WidgetTree->GetAllWidgets(Widgets);
        for (auto* Widget : Widgets)
            if (auto* Button = Cast<ULoongBuildingMenuButton>(Widget))
                if (auto* Text = Cast<UTextBlock>(Button->GetContent()))
                    if (Text->GetText().ToString() == Label)
                    {
                        Button->OnClicked.Broadcast();
                        return true;
                    }
        return false;
    };
    TestTrue(TEXT("Real equip button is bound"), Click(TEXT("试装 / 切换：FireDragonTest")));
    TestEqual(TEXT("Button selects fire actor"), F.Director->GetSelectedCompanion()->Definition.DragonId,
              FName(TEXT("ActiveCompanion_FireDragonTest")));
    Click(TEXT("召回伴战"));
    Click(TEXT("F · 上龙 / 下龙"));
    TestTrue(TEXT("Actual ride button mounts"), F.Director->IsMounted());
    Click(TEXT("F · 上龙 / 下龙"));
    TestFalse(TEXT("Actual ride button dismounts"), F.Director->IsMounted());
    F.Director->AddExperience(F.Director->ExperienceThreshold());
    Click(TEXT("试装 / 切换：IceDragonTest"));
    TestEqual(TEXT("Button callback also refuses mutations while paused"),
              F.Director->GetSelectedCompanion()->Definition.DragonId,
              FName(TEXT("ActiveCompanion_FireDragonTest")));
    F.Director->ChooseUpgrade(F.Director->UpgradeChoices[0]);
    Click(TEXT("卸下实体伴战"));
    TestNull(TEXT("Actual unequip button removes entity"), F.Director->GetSelectedCompanion());
    F.Director->EndCombat(true);
    Click(TEXT("试装 / 切换：WindDragonTest"));
    TestNull(TEXT("Ended combat cannot be re-equipped from UI"), F.Director->GetSelectedCompanion());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountedBlueprintTeardownTest,
                                 "Loong.Combat.Mount.BlueprintRiderAndLevelEndPlay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountedBlueprintTeardownTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedWorld F;
    auto* Class = LoadClass<ACharacter>(
        nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C"));
    if (!TestNotNull(TEXT("Actual rider blueprint loads"), Class))
        return false;
    auto* Player = F.World->SpawnActor<ACharacter>(Class, FVector(0, 0, 100), FRotator::ZeroRotator);
    ALoongCombatArtTestGameMode::DisableBaseSystems(Player);
    if (auto* Building = Player->FindComponentByClass<ULoongBuildingSystemComponent>())
        Building->BaseGroundActor = F.World->SpawnActor<AActor>();
    auto* PC = F.World->SpawnActor<APlayerController>();
    PC->Possess(Player);
    auto* D = F.World->SpawnActor<ALoongCombatDirector>();
    D->Definition = NewObject<ULoongCombatDefinition>(D);
    D->Definition->InitialEnemies = 0;
    D->Definition->Dragons.Reset();
    D->StartCombat(Player);
    D->SelectCompanion(TEXT("WindDragonTest"));
    if (!TestTrue(TEXT("Actual blueprint rider mounts"), D->ToggleMount()))
        return false;
    TestTrue(TEXT("Actual blueprint rider can dismount and restore possession"),
             D->Dismount() && PC->GetPawn() == Player);
    D->RecallCompanion();
    if (!TestTrue(TEXT("Actual blueprint rider mounts again"), D->ToggleMount()))
        return false;
    auto* Host = D->MountPawn.Get();
    auto* Dragon = D->GetSelectedCompanion();
    F.World->InitializeActorsForPlay(FURL(), false);
    F.World->GetWorldSettings()->NotifyBeginPlay();
    F.World->EndPlay(EEndPlayReason::LevelTransition);
    TestTrue(TEXT("Level EndPlay removes mount host"), Host->IsActorBeingDestroyed());
    TestTrue(TEXT("Level EndPlay removes visible dragon"), Dragon->IsActorBeingDestroyed());
    TestNull(TEXT("Level EndPlay detaches original rider"), Player->GetAttachParentActor());
    TestNull(TEXT("Level EndPlay clears director mount reference"), D->MountPawn.Get());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongMountKeyboardTest, "Loong.Combat.Mount.KeyboardInputRouting",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongMountKeyboardTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    F.Director->ToggleMount();
    auto* Host = F.Director->MountPawn.Get();
    F.Controller->PlayerInput = NewObject<UPlayerInput>(F.Controller);
    auto* Input = NewObject<UInputComponent>(Host);
    Host->SetupPlayerInputComponent(Input);
    auto Send = [&](FKey Key, EInputEvent Event)
    {
        FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event,
                                FPlatformTime::Cycles64());
        F.Controller->PlayerInput->InputKey(Args);
        F.Controller->PlayerInput->ProcessInputStack({Input}, 0.016f, false);
    };
    Send(EKeys::W, IE_Pressed);
    Host->Tick(0.016f);
    TestTrue(TEXT("Actual W key binding produces host movement input"),
             Host->GetPendingMovementInputVector().Size() > 0.5f);
    Host->ConsumeMovementInputVector();
    Send(EKeys::W, IE_Released);
    Host->Tick(0.016f);
    TestTrue(TEXT("Release stops pending movement"), Host->GetPendingMovementInputVector().IsNearlyZero());
    Send(EKeys::LeftShift, IE_Pressed);
    TestTrue(TEXT("Actual Shift key starts dash velocity"), Host->GetVelocity().Size2D() > 1300.f);
    TestTrue(TEXT("Actual Shift key starts cooldown"), Host->DashCooldown > 0);
    Send(EKeys::F, IE_Pressed);
    TestFalse(TEXT("Actual F binding dismounts once"), F.Director->IsMounted());
    TestTrue(TEXT("F binding restores character possession"), F.Controller->GetPawn() == F.Player);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongGroundedDismountTest, "Loong.Combat.Mount.GroundedTallRiderDismount",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongGroundedDismountTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    auto* Ground = F.World->SpawnActor<AActor>();
    auto* Floor = NewObject<UBoxComponent>(Ground);
    Ground->SetRootComponent(Floor);
    Floor->SetBoxExtent(FVector(2000, 2000, 20));
    Floor->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->RegisterComponent();
    Ground->SetActorLocation(FVector(0, 0, -20));
    // The real blueprint rider is taller than the mount's 88cm half-height.
    F.Player->GetCapsuleComponent()->SetCapsuleSize(42.f, 96.f);
    F.Player->SetActorLocation(FVector(0, 0, 100));
    if (!TestTrue(TEXT("Mount on open floor succeeds"), F.Director->ToggleMount()))
        return false;
    auto* Host = F.Director->MountPawn.Get();
    Host->SetActorLocation(FVector(0, 0, 90));
    TestTrue(TEXT("Grounded shorter mount permits taller rider to dismount on open floor"),
             F.Director->Dismount());
    TestTrue(TEXT("Rider receives possession again"), F.Controller->GetPawn() == F.Player);
    TestTrue(TEXT("Rider capsule ends clear of floor"), F.Player->GetActorLocation().Z >= 98.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongRecallAbsentTest, "Loong.Combat.Mount.RecallRestoresAbsentCompanion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongRecallAbsentTest::RunTest(const FString& Parameters)
{
    FMountedFixture F;
    F.Director->SelectCompanion(TEXT("IceDragonTest"));
    F.Director->UnequipCompanions();
    TestTrue(TEXT("Recall restores last selected entity after unequip"), F.Director->RecallCompanion());
    auto* Restored = F.Director->GetSelectedCompanion();
    if (!TestNotNull(TEXT("Recall produces an actual companion"), Restored))
        return false;
    TestEqual(TEXT("Recall restores the same selected preset"), Restored->Definition.DragonId,
              FName(TEXT("ActiveCompanion_IceDragonTest")));
    Restored->Destroy();
    TestTrue(TEXT("Recall also restores accidentally destroyed entity"), F.Director->RecallCompanion());
    TestEqual(TEXT("Recall never duplicates entities"), F.Director->Companions.Num(), 1);
    F.Director->AddExperience(F.Director->ExperienceThreshold());
    TestFalse(TEXT("Recall remains blocked during upgrade selection"), F.Director->RecallCompanion());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongActualControllerDismountTest,
                                 "Loong.Combat.Mount.ActualBlueprintControllerNoRespawn",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongActualControllerDismountTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedWorld F;
    auto* RiderClass = LoadClass<ACharacter>(
        nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C"));
    auto* ControllerClass = LoadClass<APlayerController>(
        nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownController.BP_TopDownController_C"));
    if (!TestNotNull(TEXT("Actual rider blueprint loads"), RiderClass) ||
        !TestNotNull(TEXT("Actual controller blueprint loads"), ControllerClass))
        return false;
    F.World->SpawnActor<APlayerStart>(FVector(1000, 1000, 100), FRotator::ZeroRotator);
    auto* Player = F.World->SpawnActor<ACharacter>(RiderClass, FVector(0, 0, 100), FRotator::ZeroRotator);
    ALoongCombatArtTestGameMode::DisableBaseSystems(Player);
    auto* PC = F.World->SpawnActor<APlayerController>(ControllerClass);
    if (auto* Building = Player->FindComponentByClass<ULoongBuildingSystemComponent>())
        Building->BaseGroundActor = F.World->SpawnActor<AActor>();
    F.World->InitializeActorsForPlay(FURL(), false);
    F.World->GetWorldSettings()->NotifyBeginPlay();
    PC->Possess(Player);
    auto* D = F.World->SpawnActor<ALoongCombatDirector>();
    D->Definition = NewObject<ULoongCombatDefinition>(D);
    D->Definition->InitialEnemies = 0;
    D->Definition->Dragons.Reset();
    D->StartCombat(Player);
    D->SelectCompanion(TEXT("WindDragonTest"));
    for (int32 i = 0; i < 3; ++i)
    {
        D->RecallCompanion();
        if (!TestTrue(TEXT("Actual controller can mount"), D->ToggleMount()))
            break;
        if (i == 2)
            D->MountPawn->Destroy();
        else
            TestTrue(TEXT("Actual controller can dismount"), D->Dismount());
        TestTrue(TEXT("Control remains with original character after host destruction callbacks"),
                 PC->GetPawn() == Player);
        int32 Characters = 0;
        for (TActorIterator<ACharacter> It(F.World); It; ++It)
            if (!It->IsActorBeingDestroyed())
                ++Characters;
        TestEqual(TEXT("No second humanoid character was spawned by temporary Pawn destruction"), Characters,
                  1);
        TestTrue(TEXT("Director and controller still identify the same original player"),
                 D->Player == PC->GetPawn());
    }
    TestTrue(TEXT("Original rider death callback remains bound"),
             Player->OnDestroyed.Contains(PC, FName(TEXT("OnDestroyed_Event"))));
    // Test teardown must not invoke the real controller's intentionally retained player-respawn flow.
    Player->OnDestroyed.RemoveAll(PC);
    PC->Destroy();
    D->EndCombat(true);
    D->Destroy();
    F.World->EndPlay(EEndPlayReason::Quit);
    return true;
}
#endif
