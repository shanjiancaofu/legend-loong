#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongCombatDemo.h"
#include "../LoongElementDebug.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"

namespace
{
struct FElementTestWorld
{
    UWorld* World;
    ALoongCombatDirector* Director;
    ACharacter* Player;
    IConsoleVariable* TestCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Loong.Elements.TestMode"));
    int32 PreviousTest = TestCVar->GetInt();

    explicit FElementTestWorld(ULoongCombatDefinition* SavedDefinition = nullptr)
    {
        TestCVar->SetWithCurrentPriority(0);
        const auto Init = UWorld::InitializationValues()
                              .AllowAudioPlayback(false)
                              .RequiresHitProxies(false)
                              .CreatePhysicsScene(true)
                              .CreateNavigation(false)
                              .CreateAISystem(false)
                              .ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(
            EWorldType::Game, false,
            MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ElementStability")),
            GetTransientPackage(), true, ERHIFeatureLevel::SM6, &Init);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        Player = World->SpawnActor<ACharacter>();
        Director = World->SpawnActor<ALoongCombatDirector>();
        Director->Definition = SavedDefinition
                                   ? DuplicateObject<ULoongCombatDefinition>(SavedDefinition, Director)
                                   : NewObject<ULoongCombatDefinition>(Director);
        Director->Definition->InitialEnemies = 0;
        if (!SavedDefinition)
            Director->Definition->Dragons.Reset();
        Director->Definition->Enemy.MaxHealth = 1000;
        Director->Definition->Enemy.ExperienceReward = 0;
        Director->StartCombat(Player);
    }

    ~FElementTestWorld()
    {
        Director->EndCombat(true);
        Director->Destroy();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        World->RemoveFromRoot();
        TestCVar->SetWithCurrentPriority(PreviousTest);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCompanionElementsTest, "Loong.Combat.P001A.CompanionElements",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCompanionElementsTest::RunTest(const FString& Parameters)
{
    FElementTestWorld Fixture;
    auto* D = Fixture.Director;
    auto* Enemy = D->SpawnEnemyAt(FVector(300, 0, 100));
    FLoongDragonAbility Fire;
    Fire.DragonId = TEXT("CompanionFire");
    Fire.HitEffectChance = 0;
    FLoongElementAbility Burn;
    Burn.TriggerChance = 1;
    Burn.TriggerCooldown = 1;
    Fire.ElementAbilities.Add(Burn);
    D->EquipDragon(Fire);
    FLoongDragonAbility Ice = Fire;
    Ice.DragonId = TEXT("PassiveIce");
    Ice.bSpawnCompanion = false;
    Ice.ElementAbilities[0].Effect.Element = ELoongElementKind::Ice;
    Ice.ElementAbilities[0].Effect.EffectId = TEXT("Chill");
    D->EquipDragon(Ice);
    auto* Companion = D->Companions[0].Get();
    D->FireProjectile(Companion, Enemy, Companion->Attack->Stats, false);
    auto* Shot = D->Projectiles.Last().Get();
    Shot->ResolveHit();
    Shot->ResolveHit();
    TestEqual(TEXT("Companion projectile applies its own burn exactly once"),
              Enemy->Elements->GetStacks(TEXT("Burn"), Fire.DragonId), 1);
    TestEqual(TEXT("Companion does not borrow another equipped dragon's ice"),
              Enemy->Elements->GetStacks(TEXT("Chill"), Ice.DragonId), 0);
    D->ApplyHit(Enemy, Companion, 1, false);
    TestEqual(TEXT("Companion respects element trigger cooldown"),
              Enemy->Elements->GetStacks(TEXT("Burn"), Fire.DragonId), 1);
    D->ApplyHit(Enemy, Fixture.Player, 1, true);
    TestEqual(TEXT("Hero continues to trigger equipped passive ice"),
              Enemy->Elements->GetStacks(TEXT("Chill"), Ice.DragonId), 1);
    Enemy->Elements->AdvanceCombatTime(0.2f);
    D->Elapsed += 1.1f;
    D->ApplyHit(Enemy, Companion, 1, false);
    TestEqual(TEXT("Companion reopens cooldown"), Enemy->Elements->GetStacks(TEXT("Burn"), Fire.DragonId), 2);
    D->RemoveDragon(Fire.DragonId);
    TestEqual(TEXT("Unequip clears companion-owned burn"),
              Enemy->Elements->GetStacks(TEXT("Burn"), Fire.DragonId), 0);
    TestEqual(TEXT("Unequip preserves other source ice"), Enemy->Elements->GetEffectCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongWindStabilityTest, "Loong.Combat.P001A.WindDisplacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongWindStabilityTest::RunTest(const FString& Parameters)
{
    FElementTestWorld Fixture;
    auto* D = Fixture.Director;
    auto* Mark = D->SpawnEnemyAt(FVector(10, 0, 100));
    auto* Neighbor = D->SpawnEnemyAt(FVector(20, 0, 100));
    auto* Outside = D->SpawnEnemyAt(FVector(500, 0, 100));
    FLoongElementEffect Wind;
    Wind.EffectId = TEXT("Wind");
    Wind.Element = ELoongElementKind::Wind;
    Wind.TickInterval = 0.25f;
    Wind.DisplacementPerTick = -30;
    Wind.Radius = 200;
    Mark->Elements->ApplyEffect(Wind, TEXT("WindSource"), Fixture.Player, FVector(0, 0, 100));
    Mark->Elements->AdvanceCombatTime(0.25f);
    TestTrue(TEXT("Pull stops at source instead of crossing it"),
             Mark->GetActorLocation().Equals(FVector(0, 0, 100)) &&
                 Neighbor->GetActorLocation().Equals(FVector(0, 0, 100)));
    TestEqual(TEXT("Wind damages neighbor in radius"), Neighbor->Health->CurrentHealth, 997.f);
    TestEqual(TEXT("Wind excludes target outside radius"), Outside->Health->CurrentHealth, 1000.f);
    Mark->Elements->AdvanceCombatTime(0.25f);
    TestTrue(TEXT("Pull at source remains stable"), Neighbor->GetActorLocation().Equals(FVector(0, 0, 100)));
    Mark->Elements->ClearEffects();
    Wind.DisplacementPerTick = 30;
    Mark->SetActorLocation(FVector(10, 0, 100));
    Mark->Elements->ApplyEffect(Wind, TEXT("PushSource"), Fixture.Player, FVector(0, 0, 100));
    Mark->Elements->AdvanceCombatTime(0.25f);
    TestTrue(TEXT("Positive wind still pushes away"), FMath::IsNearlyEqual(Mark->GetActorLocation().X, 40));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongElementVisibilityTest, "Loong.Combat.P001A.ElementVisibility",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongElementVisibilityTest::RunTest(const FString& Parameters)
{
    FElementTestWorld Fixture;
    auto* D = Fixture.Director;
    auto* Enemy = D->SpawnEnemyAt(FVector(300, 0, 100));
    for (const auto Kind : {ELoongElementKind::Fire, ELoongElementKind::Ice, ELoongElementKind::Wind})
    {
        FLoongElementEffect Effect;
        Effect.Element = Kind;
        Effect.EffectId = FName(*FString::Printf(TEXT("Visual%d"), static_cast<int32>(Kind)));
        Effect.Duration = 1;
        TestTrue(
            TEXT("Effect without Niagara still applies"),
            Enemy->Elements->ApplyEffect(Effect, TEXT("VisualSource"), Fixture.Player, FVector::ZeroVector));
    }
    TInlineComponentArray<UInstancedStaticMeshComponent*> Visuals(Enemy);
    TestEqual(TEXT("Each missing Niagara has a visible prototype component"), Visuals.Num(), 3);
    for (auto* Visual : Visuals)
    {
        TestTrue(TEXT("Fallback registered, visible, with instances and no collision"),
                 Visual->IsRegistered() && Visual->IsVisible() && Visual->GetInstanceCount() > 0 &&
                     Visual->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    }
    TArray<FTransform> BeforePause;
    for (auto* Visual : Visuals)
    {
        FTransform Transform;
        Visual->GetInstanceTransform(0, Transform);
        BeforePause.Add(Transform);
    }
    D->AddExperience(D->ExperienceThreshold());
    Enemy->Elements->AdvanceCombatTime(5);
    TestEqual(TEXT("Upgrade pause preserves effects"), Enemy->Elements->GetEffectCount(), 3);
    for (int32 i = 0; i < Visuals.Num(); ++i)
    {
        FTransform Transform;
        Visuals[i]->GetInstanceTransform(0, Transform);
        TestTrue(TEXT("Upgrade pause preserves fallback animation"), Transform.Equals(BeforePause[i]));
    }
    D->ChooseUpgrade(D->UpgradeChoices[0]);
    Enemy->Elements->AdvanceCombatTime(1.1f);
    TestEqual(TEXT("Effects expire after resume"), Enemy->Elements->GetEffectCount(), 0);
    for (auto* Visual : Visuals)
        TestFalse(TEXT("Expired fallback destroyed"), Visual->IsRegistered());
    FLoongElementEffect Invalid;
    Invalid.Element = static_cast<ELoongElementKind>(255);
    TestFalse(TEXT("Invalid element kind rejected"), Invalid.IsValid());
    Invalid.Element = ELoongElementKind::Fire;
    Invalid.VisualScale.X = 0;
    TestFalse(TEXT("Zero visual scale rejected"), Invalid.IsValid());
    FLoongElementEffect Invisible;
    Invisible.bUseFallbackVisual = false;
    TestTrue(TEXT("Fallback can be disabled without blocking gameplay"),
             Enemy->Elements->ApplyEffect(Invisible, TEXT("NoVisual"), Fixture.Player, FVector::ZeroVector));
    TInlineComponentArray<UInstancedStaticMeshComponent*> DisabledVisuals(Enemy);
    TestEqual(TEXT("Opt-out creates no visual"), DisabledVisuals.Num(), 0);
    Enemy->Elements->ClearEffects();
    FLoongElementEffect Fatal;
    Fatal.DamagePerTick = 10000;
    Enemy->Elements->ApplyEffect(Fatal, TEXT("Fatal"), Fixture.Player, FVector::ZeroVector);
    TInlineComponentArray<UInstancedStaticMeshComponent*> FatalVisuals(Enemy);
    Enemy->Elements->AdvanceCombatTime(0.5f);
    TestEqual(TEXT("Death during periodic tick clears effects safely"), Enemy->Elements->GetEffectCount(), 0);
    for (auto* Visual : FatalVisuals)
        TestFalse(TEXT("Death clears fallback component"), Visual->IsRegistered());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongSavedElementConfigurationTest,
                                 "Loong.Combat.P001A.SavedCombatConfiguration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongSavedElementConfigurationTest::RunTest(const FString& Parameters)
{
    auto* Map = LoadObject<UWorld>(nullptr, TEXT("/Game/Loong/Maps/L_Combat.L_Combat"));
    if (!TestNotNull(TEXT("Actual L_Combat loads"), Map) || !Map->PersistentLevel)
        return false;
    ULoongCombatDefinition* Saved = nullptr;
    for (const auto& Actor : Map->PersistentLevel->Actors)
        if (auto* Director = Cast<ALoongCombatDirector>(Actor))
        {
            Saved = Director->Definition;
            break;
        }
    auto* Settings = Map->GetWorldSettings();
    auto* Mode = Settings && Settings->DefaultGameMode
                     ? Cast<ALoongCombatGameMode>(Settings->DefaultGameMode->GetDefaultObject())
                     : nullptr;
    if (!TestNotNull(TEXT("Actual map uses Loong combat game mode"), Mode))
        return false;
    if (Mode->CombatDefinition)
        Saved = Mode->CombatDefinition;
    if (!TestNotNull(TEXT("Actual effective Director/Definition resolves"), Saved))
        return false;
    AddInfo(FString::Printf(TEXT("Actual effective definition: %s hero damage=%.1f melee HP=%.1f"),
                            *Saved->GetPathName(), Saved->HeroStats.AttackDamage, Saved->Enemy.MaxHealth));
    TestTrue(TEXT("Actual saved config enables element test presets"), Saved->bEquipElementTestPresets);
    const float OriginalHP = Saved->Enemy.MaxHealth;
    FElementTestWorld Fixture(Saved);
    Fixture.TestCVar->SetWithCurrentPriority(1);
    auto* Enemy = Fixture.Director->SpawnEnemyAt(FVector(300, 0, 100));
    if (!TestNotNull(TEXT("Enemy from actual configuration spawns"), Enemy))
        return false;
    for (int32 Hit = 0; Hit < 3; ++Hit)
    {
        if (Hit > 0)
            Enemy->Elements->AdvanceCombatTime(0.2f);
        Fixture.Director->ApplyHit(Enemy, Fixture.Player, 25, true);
    }
    TestEqual(TEXT("Saved preset loadout applies three element statuses"), Enemy->Elements->GetEffectCount(),
              3);
    TestTrue(TEXT("Saved ice preset freezes"), Enemy->Elements->IsFrozen());
    const float BeforeTick = Enemy->Health->CurrentHealth;
    Enemy->Elements->AdvanceCombatTime(0.2f);
    TestTrue(TEXT("Saved fire/wind presets deal periodic damage"), Enemy->Health->CurrentHealth < BeforeTick);
    TestTrue(TEXT("Saved wind preset affects registered target"), Enemy->Elements->LastWindHits > 0);
    AddInfo(Enemy->Elements->BuildDebugText());
    TestEqual(TEXT("Audit/test does not modify saved asset health"), Saved->Enemy.MaxHealth, OriginalHP);
    return true;
}
#endif
