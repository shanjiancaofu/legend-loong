#if WITH_DEV_AUTOMATION_TESTS
#include "LoongAutomationTestWorld.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/WorldSettings.h"
#include <limits>

namespace
{
ALoongCombatDragon* EquipCompanion(LoongTests::FScopedCombat& Fixture, FName Id = TEXT("QA_Companion"))
{
    FLoongDragonAbility Ability;
    Ability.DragonId = Id;
    Ability.HitEffectChance = 0;
    Fixture.Director->EquipDragon(Ability);
    return Fixture.Director->Companions.Last().Get();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionFollowTest, "Loong.Combat.Companion.FollowConvergence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionFollowTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Dragon = EquipCompanion(Fixture);
    const FVector Offset = Dragon->Definition.FollowOffset;
    TestTrue(TEXT("Initial spawn uses existing world-space offset"),
             Dragon->GetActorLocation().Equals(Fixture.Player->GetActorLocation() + Offset));
    // Moderate moving target: require a bounded lag rather than teleporting every frame.
    for (int32 i = 0; i < 60; ++i)
    {
        Fixture.Player->AddActorWorldOffset(FVector(5, 0, 0));
        const FVector Previous = Dragon->GetActorLocation();
        Dragon->Tick(1.f / 60.f);
        TestTrue(TEXT("Normal following remains smooth"),
                 FVector::Dist(Previous, Dragon->GetActorLocation()) < 10.f);
    }
    const FVector Destination = Fixture.Player->GetActorLocation() + Offset;
    float PreviousDistance = FVector::Dist(Dragon->GetActorLocation(), Destination);
    TestTrue(TEXT("Moving player keeps a bounded follow lag"), PreviousDistance < 100.f);
    for (int32 i = 0; i < 120; ++i)
    {
        Dragon->Tick(1.f / 60.f);
        const float Distance = FVector::Dist(Dragon->GetActorLocation(), Destination);
        TestTrue(TEXT("Stopped player produces monotonic convergence"),
                 Distance <= PreviousDistance + KINDA_SMALL_NUMBER);
        PreviousDistance = Distance;
    }
    TestTrue(TEXT("Dragon settles close to the destination"), PreviousDistance < 0.1f);
    Fixture.Director->AddExperience(Fixture.Director->ExperienceThreshold());
    const FVector Paused = Dragon->GetActorLocation();
    Fixture.Player->AddActorWorldOffset(FVector(200, 0, 0));
    Dragon->Tick(5);
    TestTrue(TEXT("Upgrade pause stops following"), Dragon->GetActorLocation().Equals(Paused));
    Fixture.Director->ChooseUpgrade(Fixture.Director->UpgradeChoices[0]);
    Dragon->Tick(0.1f);
    TestFalse(TEXT("Following resumes"), Dragon->GetActorLocation().Equals(Paused));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionRecoveryTest, "Loong.Combat.Companion.FarDistanceRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionRecoveryTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Dragon = EquipCompanion(Fixture);
    Dragon->SetActorLocation(FVector(10000, 0, 100));
    Dragon->Tick(1.f / 60.f);
    TestTrue(TEXT("A lost companion recovers near the player on the next valid tick"),
             Dragon->GetActorLocation().Equals(Fixture.Player->GetActorLocation() +
                                               Dragon->Definition.FollowOffset));
    Fixture.Player->SetActorLocation(FVector(4000, -3000, 100));
    Dragon->Tick(1.f / 60.f);
    TestTrue(TEXT("Player teleport also recovers the companion"),
             Dragon->GetActorLocation().Equals(Fixture.Player->GetActorLocation() +
                                               Dragon->Definition.FollowOffset));
    const FVector Position = Dragon->GetActorLocation();
    Dragon->Tick(std::numeric_limits<float>::quiet_NaN());
    Dragon->Tick(-1);
    TestTrue(TEXT("Invalid frame times cannot move the companion"),
             Dragon->GetActorLocation().Equals(Position));
    FLoongDragonAbility Broken = Dragon->Definition;
    Broken.FollowOffset.X = std::numeric_limits<double>::quiet_NaN();
    Broken.FollowRecoveryDistance = -1;
    Fixture.Director->EquipDragon(Broken);
    auto* Replacement = Fixture.Director->Companions.Last().Get();
    TestTrue(TEXT("Invalid offset falls back safely before spawning"),
             Replacement->GetActorLocation().Equals(Fixture.Player->GetActorLocation() +
                                                    FLoongDragonAbility().FollowOffset));
    TestEqual(TEXT("Invalid recovery range uses the compatible default"),
              Replacement->Definition.FollowRecoveryDistance, 2000.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionAttackTest, "Loong.Combat.Companion.TargetAndAttackCooldown",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionAttackTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Dragon = EquipCompanion(Fixture);
    auto* DestroyedTarget = Fixture.Director->SpawnEnemyAt(Dragon->GetActorLocation() + FVector(10, 0, 0));
    auto* Near = Fixture.Director->SpawnEnemyAt(Dragon->GetActorLocation() + FVector(100, 0, 0));
    Fixture.Director->SpawnEnemyAt(Dragon->GetActorLocation() + FVector(1200, 0, 0));
    DestroyedTarget->Destroy();
    TestTrue(TEXT("Target selection skips a destroyed actor"), Dragon->Attack->FindTarget() == Near);
    auto* SavedHealth = Near->Health.Get();
    Near->Health = nullptr;
    TestNull(TEXT("Target selection skips an enemy without health"), Dragon->Attack->FindTarget());
    Near->Health = SavedHealth;
    Dragon->Attack->TickComponent(0.01f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Companion launches a projectile"), Fixture.Director->Projectiles.Num(), 1);
    Dragon->Attack->TickComponent(0.25f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Cooldown prevents an extra projectile"), Fixture.Director->Projectiles.Num(), 1);
    const float Cooldown = Dragon->Attack->Cooldown;
    Fixture.Director->AddExperience(Fixture.Director->ExperienceThreshold());
    Dragon->Attack->TickComponent(5, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Pause preserves attack cooldown"), Dragon->Attack->Cooldown, Cooldown);
    Fixture.Director->ChooseUpgrade(Fixture.Director->UpgradeChoices[0]);
    Dragon->Attack->TickComponent(Cooldown, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Cooldown reopens after resume"), Fixture.Director->Projectiles.Num(), 2);
    const float Health = Near->Health->CurrentHealth;
    Fixture.Director->Projectiles.Last()->ResolveHit();
    TestEqual(TEXT("Projectile applies actual companion damage"), Near->Health->CurrentHealth,
              Health - Dragon->Definition.CompanionDamage);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionUnequipTest,
                                 "Loong.Combat.Companion.UnequipCancelsOwnedAttacks",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionUnequipTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Dragon = EquipCompanion(Fixture);
    auto* Other = EquipCompanion(Fixture, TEXT("OtherCompanion"));
    auto* Enemy = Fixture.Director->SpawnEnemyAt(FVector(300, 0, 100));
    FLoongElementEffect Burn;
    Burn.bUseFallbackVisual = false;
    Enemy->Elements->ApplyEffect(Burn, Dragon->Definition.DragonId, Dragon, Dragon->GetActorLocation());
    Fixture.Director->FireProjectile(Dragon, Enemy, Dragon->Attack->Stats, false);
    auto* OwnedShot = Fixture.Director->Projectiles.Last().Get();
    Fixture.Director->FireProjectile(Other, Enemy, Other->Attack->Stats, false);
    Fixture.Director->FireProjectile(Fixture.Player, Enemy, Fixture.Director->PlayerAttack->Stats, true);
    const float Health = Enemy->Health->CurrentHealth;
    auto* Attack = Dragon->Attack.Get();
    Fixture.Director->RemoveDragon(Dragon->Definition.DragonId);
    TestTrue(TEXT("Unequipped actor is destroyed"), Dragon->IsActorBeingDestroyed());
    TestEqual(TEXT("Only the remaining companion stays registered"), Fixture.Director->Companions.Num(), 1);
    TestEqual(TEXT("Owned element statuses are removed"), Enemy->Elements->GetEffectCount(), 0);
    TestEqual(TEXT("Only the other companion and hero projectiles remain"),
              Fixture.Director->Projectiles.Num(), 2);
    OwnedShot->ResolveHit();
    TestEqual(TEXT("In-flight removed source cannot deal ghost damage"), Enemy->Health->CurrentHealth,
              Health);
    TestFalse(TEXT("Removed component is no longer registered for engine ticking"), Attack->IsRegistered());
    TestFalse(TEXT("Removed component has its tick explicitly disabled"), Attack->IsComponentTickEnabled());
    // Replacing an equipped ID also cancels only that old actor's in-flight attacks.
    Fixture.Director->FireProjectile(Other, Enemy, Other->Attack->Stats, false);
    auto* ReplacementShot = Fixture.Director->Projectiles.Last().Get();
    FLoongDragonAbility Ability = Other->Definition;
    Fixture.Director->EquipDragon(Ability);
    TestTrue(TEXT("Replacement destroys the previous companion"), Other->IsActorBeingDestroyed());
    TestTrue(TEXT("Replacement cancels previous actor's projectile"),
             ReplacementShot->bConsumed && ReplacementShot->IsActorBeingDestroyed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionLifecycleTest,
                                 "Loong.Combat.Companion.DestroyAndSettlementSafety",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionLifecycleTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Dragon = EquipCompanion(Fixture);
    auto* Enemy = Fixture.Director->SpawnEnemyAt(FVector(300, 0, 100));
    FLoongElementEffect Burn;
    Burn.bUseFallbackVisual = false;
    Enemy->Elements->ApplyEffect(Burn, Dragon->Definition.DragonId, Dragon, Dragon->GetActorLocation());
    Fixture.Director->FireProjectile(Dragon, Enemy, Dragon->Attack->Stats, false);
    Dragon->Destroy();
    TestEqual(TEXT("Direct destruction unregisters its loadout"), Fixture.Director->GetEquippedDragonCount(),
              0);
    TestEqual(TEXT("Direct destruction removes its statuses"), Enemy->Elements->GetEffectCount(), 0);
    TestEqual(TEXT("Direct destruction cancels its projectiles"), Fixture.Director->Projectiles.Num(), 0);
    auto* NewDragon = EquipCompanion(Fixture);
    auto* Attack = NewDragon->Attack.Get();
    Fixture.Director->PlayerHealth->ApplyCombatDamage(10000, Enemy);
    TestFalse(TEXT("Settled companion component is unregistered"), Attack->IsRegistered());
    Fixture.Director->Tick(10);
    TestEqual(TEXT("Player death enters results"), Fixture.Director->State, ELoongCombatState::Results);
    TestEqual(TEXT("After settlement no attacks remain"), Fixture.Director->Projectiles.Num(), 0);
    TestEqual(TEXT("After settlement no companions remain"), Fixture.Director->Companions.Num(), 0);
    for (const bool Pause : {false, true})
    {
        LoongTests::FScopedCombat LostPlayer;
        auto* Companion = EquipCompanion(LostPlayer);
        if (Pause)
            LostPlayer.Director->AddExperience(LostPlayer.Director->ExperienceThreshold());
        const FVector Before = Companion->GetActorLocation();
        LostPlayer.Player->Destroy();
        Companion->Tick(0.1f);
        Companion->Attack->TickComponent(0.1f, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Missing player leaves companion transform intact"),
                 Companion->GetActorLocation().Equals(Before));
        TestEqual(TEXT("Missing player prevents attacks"), LostPlayer.Director->Projectiles.Num(), 0);
        LostPlayer.Director->Tick(0.1f);
        TestEqual(TEXT("Lost player safely ends both fighting and upgrade pause"), LostPlayer.Director->State,
                  ELoongCombatState::Results);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionEndPlayTest, "Loong.Combat.Companion.DirectorEndPlayCleanup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionEndPlayTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedWorld Fixture;
    auto* Player = Fixture.World->SpawnActor<ACharacter>();
    auto* Director = Fixture.World->SpawnActor<ALoongCombatDirector>();
    Director->Definition = NewObject<ULoongCombatDefinition>(Director);
    Director->Definition->InitialEnemies = 0;
    Director->Definition->Dragons.Reset();
    Director->StartCombat(Player);
    FLoongDragonAbility Ability;
    Director->EquipDragon(Ability);
    auto* Dragon = Director->Companions.Last().Get();
    auto* Enemy = Director->SpawnEnemyAt(FVector(300, 0, 100));
    Director->FireProjectile(Dragon, Enemy, Dragon->Attack->Stats, false);
    auto* Shot = Director->Projectiles.Last().Get();
    auto* Attack = Dragon->Attack.Get();
    Fixture.World->InitializeActorsForPlay(FURL(), false);
    Fixture.World->GetWorldSettings()->NotifyBeginPlay();
    TestTrue(TEXT("Lifecycle test director actually began play"), Director->HasActorBegunPlay());
    Fixture.World->EndPlay(EEndPlayReason::LevelTransition);
    TestTrue(TEXT("Director EndPlay destroys the companion"), Dragon->IsActorBeingDestroyed());
    TestTrue(TEXT("Director EndPlay destroys its projectile"), Shot->IsActorBeingDestroyed());
    TestFalse(TEXT("Companion attack no longer holds the director"), Attack->Director.IsValid());
    TestFalse(TEXT("Companion attack is unregistered after EndPlay"), Attack->IsRegistered());
    TestFalse(TEXT("Player attack component was cleaned up"), IsValid(Director->PlayerAttack));
    return true;
}
#endif
