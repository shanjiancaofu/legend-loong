#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongCombatDemo.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

namespace LoongTests
{
/** Each test owns its world/context and restores process-wide observation settings. */
struct FScopedWorld
{
    UWorld* World = nullptr;
    IConsoleVariable* TestMode = IConsoleManager::Get().FindConsoleVariable(TEXT("Loong.Elements.TestMode"));
    int32 PreviousTestMode = TestMode->GetInt();

    FScopedWorld()
    {
        TestMode->SetWithCurrentPriority(0);
        const auto Init = UWorld::InitializationValues()
                              .AllowAudioPlayback(false)
                              .RequiresHitProxies(false)
                              .CreatePhysicsScene(true)
                              .CreateNavigation(false)
                              .CreateAISystem(false)
                              .ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(
            EWorldType::Game, false,
            MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("LoongQA")),
            GetTransientPackage(), true, ERHIFeatureLevel::SM6, &Init);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    }

    ~FScopedWorld()
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        World->RemoveFromRoot();
        TestMode->SetWithCurrentPriority(PreviousTestMode);
    }

    FScopedWorld(const FScopedWorld&) = delete;
    FScopedWorld& operator=(const FScopedWorld&) = delete;
};

struct FScopedCombat : FScopedWorld
{
    ACharacter* Player = nullptr;
    ALoongCombatDirector* Director = nullptr;

    FScopedCombat()
    {
        Player = World->SpawnActor<ACharacter>();
        Director = World->SpawnActor<ALoongCombatDirector>();
        Director->Definition = NewObject<ULoongCombatDefinition>(Director);
        Director->Definition->Dragons.Reset();
        Director->Definition->InitialEnemies = 0;
        Director->Definition->Enemy.MaxHealth = 1000;
        Director->Definition->Enemy.ExperienceReward = 0;
        Director->StartCombat(Player);
    }

    ~FScopedCombat()
    {
        Director->EndCombat(false);
        Director->Destroy();
    }
};
}
#endif
