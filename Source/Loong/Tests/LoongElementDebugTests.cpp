#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongCombatDemo.h"
#include "../LoongElementDebug.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongElementDebugTest, "Loong.Combat.Phase6.ElementDebugChain",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongElementDebugTest::RunTest(const FString& Parameters)
{
    auto* DebugCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Loong.Elements.Debug"));
    auto* TestCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Loong.Elements.TestMode"));
    // Read-only audit of the user's saved configuration/map, when present locally.
    if (FPaths::FileExists(FPaths::ProjectContentDir() /
                           TEXT("Loong/Data/Combat/DA_Combat_Phase6_Test.uasset")))
    {
        if (auto* Actual = LoadObject<ULoongCombatDefinition>(
                nullptr, TEXT("/Game/Loong/Data/Combat/DA_Combat_Phase6_Test.DA_Combat_Phase6_Test")))
        {
            AddInfo(FString::Printf(TEXT("Saved config audit: %s EquipPresets=%d presets=%d"),
                                    *Actual->GetPathName(), Actual->bEquipElementTestPresets,
                                    Actual->ElementDragonPresets.Num()));
            for (const auto& Preset : Actual->ElementDragonPresets)
                AddInfo(FString::Printf(TEXT("Saved preset: %s modules=%d"), *Preset.DragonId.ToString(),
                                        Preset.ElementAbilities.Num()));
        }
    }
    if (auto* Actual = FindObject<ULoongCombatDefinition>(
            nullptr, TEXT("/Game/Loong/Data/Combat/DA_Combat_Phase6_Test.DA_Combat_Phase6_Test")))
    {
        for (const auto& Preset : Actual->ElementDragonPresets)
            for (const auto& Skill : Preset.ElementAbilities)
                AddInfo(FString::Printf(
                    TEXT(
                        "Saved effect audit: %s/%s chance=%.2f cooldown=%.2f duration=%.2f visual=%s valid=%d"),
                    *Preset.DragonId.ToString(), *Skill.SkillId.ToString(), Skill.TriggerChance,
                    Skill.TriggerCooldown, Skill.Effect.Duration,
                    *Skill.Effect.VisualEffect.ToSoftObjectPath().ToString(), Skill.Effect.IsValid()));
    }
    if (FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Loong/Maps/L_Combat.umap")))
    {
        if (auto* Map = LoadObject<UWorld>(nullptr, TEXT("/Game/Loong/Maps/L_Combat.L_Combat")))
        {
            if (Map->PersistentLevel)
                for (const auto& Actor : Map->PersistentLevel->Actors)
                    if (auto* Authored = Cast<ALoongCombatDirector>(Actor))
                        AddInfo(FString::Printf(TEXT("Saved map director audit: %s Definition=%s"),
                                                *Authored->GetName(),
                                                *GetPathNameSafe(Authored->Definition)));
            if (Map->GetWorldSettings() && Map->GetWorldSettings()->DefaultGameMode)
                if (auto* Mode = Cast<ALoongCombatGameMode>(
                        Map->GetWorldSettings()->DefaultGameMode->GetDefaultObject()))
                    AddInfo(FString::Printf(TEXT("Saved map GameMode override definition=%s"),
                                            *GetPathNameSafe(Mode->CombatDefinition)));
        }
    }
    const int32 OldDebug = DebugCVar->GetInt(), OldTest = TestCVar->GetInt();
    DebugCVar->Set(0, ECVF_SetByCode);
    TestCVar->Set(0, ECVF_SetByCode);
    const auto Init = UWorld::InitializationValues()
                          .AllowAudioPlayback(false)
                          .RequiresHitProxies(false)
                          .CreatePhysicsScene(true)
                          .CreateNavigation(false)
                          .CreateAISystem(false)
                          .ShouldSimulatePhysics(false);
    auto* W = UWorld::CreateWorld(
        EWorldType::Game, false,
        MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("LoongElementDebugTest")),
        GetTransientPackage(), true, ERHIFeatureLevel::SM6, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* D = W->SpawnActor<ALoongCombatDirector>();
    auto* Pawn = W->SpawnActor<ACharacter>();
    D->Definition = NewObject<ULoongCombatDefinition>();
    D->Definition->InitialEnemies = 0;
    D->Definition->Enemy.MaxHealth = 1000;
    D->Definition->Enemy.ExperienceReward = 0;
    D->Definition->bEquipElementTestPresets = true;
    D->StartCombat(Pawn);
    TestEqual(TEXT("Start registers old dragon plus all three test presets"), D->GetEquippedDragonCount(), 4);
    FString Summary = D->GetElementDebugSummary();
    TestTrue(TEXT("All three DragonIds actually registered"), Summary.Contains(TEXT("FireDragonTest")) &&
                                                                  Summary.Contains(TEXT("IceDragonTest")) &&
                                                                  Summary.Contains(TEXT("WindDragonTest")));
    auto* E = D->SpawnEnemyOfType(TEXT("MeleeTest"), FVector(300, 0, 100));
    TestNotNull(TEXT("PIE-type spawned enemy has element component"), E->Elements.Get());
    TestCVar->Set(1, ECVF_SetByCode);
    const float Before = E->Health->CurrentHealth;
    D->ApplyHit(E, Pawn, 25, true);
    TestEqual(TEXT("Observation test keeps target alive with one direct damage"), E->Health->CurrentHealth,
              Before - 1);
    TestEqual(TEXT("Configured fire ice wind apply"), E->Elements->GetEffectCount(), 3);
    TestEqual(TEXT("Diagnostic application counters"), E->Elements->SuccessfulApplications, 3);
    for (int32 i = 0; i < 2; ++i)
    {
        E->Elements->AdvanceCombatTime(0.2f);
        D->ApplyHit(E, Pawn, 25, true);
    }
    TestTrue(TEXT("Ice observation reaches frozen state"), E->Elements->IsFrozen());
    FString StateText = E->Elements->BuildDebugText();
    TestTrue(TEXT("World text includes all three element types and times"),
             StateText.Contains(TEXT("FIRE")) && StateText.Contains(TEXT("ICE")) &&
                 StateText.Contains(TEXT("WIND")) && StateText.Contains(TEXT("remaining=")));
    const int32 TickCount = E->Elements->PeriodicApplications;
    D->State = ELoongCombatState::UpgradeSelection;
    const FString Paused = E->Elements->BuildDebugText();
    E->Elements->AdvanceCombatTime(9);
    TestEqual(TEXT("Pause leaves timer counters unchanged"), E->Elements->PeriodicApplications, TickCount);
    TestEqual(TEXT("Pause leaves state duration unchanged"), E->Elements->BuildDebugText(), Paused);
    D->State = ELoongCombatState::Fighting;
    E->Elements->AdvanceCombatTime(0.6f);
    TestTrue(TEXT("Resume advances periodic damage"), E->Elements->PeriodicApplications > TickCount);
    D->RemoveDragon(TEXT("FireDragonTest"));
    D->RemoveDragon(TEXT("IceDragonTest"));
    D->RemoveDragon(TEXT("WindDragonTest"));
    D->ApplyHit(E, Pawn, 25, true);
    TestTrue(TEXT("Missing modules gives explicit diagnostic"),
             D->GetElementDebugSummary().Contains(TEXT("no equipped element modules")));
    TestEqual(TEXT("Test mode did not mutate combat asset damage"), D->Definition->HeroStats.AttackDamage,
              25.f);
    TestCVar->Set(0, ECVF_SetByCode);
    const float NormalBefore = E->Health->CurrentHealth;
    D->ApplyHit(E, Pawn, 25, true);
    TestTrue(TEXT("Normal mode restores full direct damage"), E->Health->CurrentHealth <= NormalBefore - 25);
    D->EndCombat(true);
    D->Destroy();
    GEngine->DestroyWorldContext(W);
    W->DestroyWorld(false);
    W->RemoveFromRoot();
    DebugCVar->Set(OldDebug, ECVF_SetByCode);
    TestCVar->Set(OldTest, ECVF_SetByCode);
    return true;
}
#endif
