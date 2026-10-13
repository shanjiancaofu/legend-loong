#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongCombatDemo.h"
#include "../LoongElementComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "HAL/PlatformTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongElementFoundationTest, "Loong.Combat.Phase6.EnemiesAndElements",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoongElementFoundationTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues()
                          .AllowAudioPlayback(false)
                          .RequiresHitProxies(false)
                          .CreatePhysicsScene(true)
                          .CreateNavigation(false)
                          .CreateAISystem(false)
                          .ShouldSimulatePhysics(false);
    auto* W = UWorld::CreateWorld(
        EWorldType::Game, false,
        MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("LoongPhase6")),
        GetTransientPackage(), true, ERHIFeatureLevel::SM6, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    auto* D = W->SpawnActor<ALoongCombatDirector>();
    auto* Pawn = W->SpawnActor<ACharacter>();
    D->Definition = NewObject<ULoongCombatDefinition>();
    D->Definition->InitialEnemies = 0;
    D->Definition->Dragons.Reset();
    D->Definition->Enemy.MaxHealth = 1000;
    D->Definition->Enemy.ExperienceReward = 0;
    TestTrue(TEXT("Start"), D->StartCombat(Pawn));
    TestEqual(TEXT("Legacy melee preserves edited health"),
              D->Definition->FindEnemy(TEXT("MeleeTest"))->MaxHealth, 1000.f);
    TestTrue(TEXT("Fast enemy distinct speed/health"),
             D->Definition->FindEnemy(TEXT("FastTest"))->MoveSpeed > D->Definition->Enemy.MoveSpeed &&
                 D->Definition->FindEnemy(TEXT("FastTest"))->MaxHealth < D->Definition->Enemy.MaxHealth);
    FRandomStream R1(4), R2(4);
    TSet<FName> Types;
    for (int i = 0; i < 100; ++i)
    {
        auto* A = D->Definition->SelectSpawnEnemy(80, R1);
        auto* B = D->Definition->SelectSpawnEnemy(80, R2);
        TestEqual(TEXT("Seed repeatability"), A->EnemyId, B->EnemyId);
        Types.Add(A->EnemyId);
    }
    TestEqual(TEXT("Weighted phase includes three enemy types"), Types.Num(), 3);
    FLoongEnemySpawnWeight Unknown;
    Unknown.EnemyId = TEXT("NoDefinition");
    Unknown.Weight = 1000;
    D->Definition->SpawnPhases[0].Enemies.Add(Unknown);
    TestEqual(TEXT("Unknown weighted candidate ignored"), D->Definition->SelectSpawnEnemy(0, R1)->EnemyId,
              FName(TEXT("MeleeTest")));
    TestEqual(TEXT("Early phase only melee"), D->Definition->SelectSpawnEnemy(0, R1)->EnemyId,
              FName(TEXT("MeleeTest")));
    TestEqual(TEXT("Phase boundary"), D->Definition->FindSpawnPhase(60)->StartSeconds, 60.f);
    auto Phases = D->Definition->SpawnPhases;
    D->Definition->SpawnPhases.Reset();
    TestTrue(TEXT("Empty phases use legacy enemy"),
             D->Definition->SelectSpawnEnemy(90, R1) == &D->Definition->Enemy);
    D->Definition->SpawnPhases = Phases;
    TestNull(TEXT("Unknown ID rejected"), D->SpawnEnemyOfType(TEXT("Missing"), FVector::ZeroVector));
    auto* E = D->SpawnEnemyOfType(TEXT("MeleeTest"), FVector(500, 0, 100));
    auto* Elements = E->Elements.Get();
    FLoongElementEffect Fire;
    Fire.Duration = 1;
    Fire.TickInterval = 0.25f;
    Fire.DamagePerTick = 2;
    Fire.ApplicationCooldown = 0.1f;
    Fire.MaxStacks = 2;
    TestTrue(TEXT("Burn applies"),
             Elements->ApplyEffect(Fire, TEXT("FireDragon"), Pawn, Pawn->GetActorLocation()));
    FLoongElementEffect Collision = Fire;
    Collision.Element = ELoongElementKind::Wind;
    Collision.ApplicationCooldown = 0;
    TestFalse(TEXT("Conflicting element type with same source/id rejected"),
              Elements->ApplyEffect(Collision, TEXT("FireDragon"), Pawn, FVector::ZeroVector));
    TestFalse(TEXT("Rapid repeat cooldown"),
              Elements->ApplyEffect(Fire, TEXT("FireDragon"), Pawn, FVector::ZeroVector));
    Elements->AdvanceCombatTime(0.1f);
    TestTrue(TEXT("Second stack"),
             Elements->ApplyEffect(Fire, TEXT("FireDragon"), Pawn, FVector::ZeroVector));
    Elements->AdvanceCombatTime(0.1f);
    Elements->ApplyEffect(Fire, TEXT("FireDragon"), Pawn, FVector::ZeroVector);
    TestEqual(TEXT("Stack limit"), Elements->GetStacks(TEXT("Burn"), TEXT("FireDragon")), 2);
    float Health = E->Health->CurrentHealth;
    Elements->AdvanceCombatTime(0.05f);
    TestEqual(TEXT("Burn tick damage scales by stacks"), E->Health->CurrentHealth, Health - 4);
    D->State = ELoongCombatState::UpgradeSelection;
    Health = E->Health->CurrentHealth;
    Elements->AdvanceCombatTime(10);
    TestEqual(TEXT("Upgrade pause freezes statuses"), E->Health->CurrentHealth, Health);
    TestEqual(TEXT("Pause does not expire effect"), Elements->GetEffectCount(), 1);
    D->State = ELoongCombatState::Fighting;
    Elements->AdvanceCombatTime(2);
    TestEqual(TEXT("Burn expires"), Elements->GetEffectCount(), 0);
    FLoongElementEffect Ice;
    Ice.EffectId = TEXT("Chill");
    Ice.Element = ELoongElementKind::Ice;
    Ice.Duration = 2;
    Ice.ApplicationCooldown = 0;
    Ice.FreezeAtStacks = 3;
    Ice.FreezeDuration = 0.5f;
    Elements->ApplyEffect(Ice, TEXT("IceDragon"), Pawn, FVector::ZeroVector);
    TestTrue(TEXT("Ice slows"), Elements->GetMovementMultiplier() < 1);
    Elements->ApplyEffect(Ice, TEXT("IceDragon"), Pawn, FVector::ZeroVector);
    Elements->ApplyEffect(Ice, TEXT("IceDragon"), Pawn, FVector::ZeroVector);
    TestTrue(TEXT("Ice threshold freezes"), Elements->IsFrozen());
    Elements->AdvanceCombatTime(0.6f);
    TestFalse(TEXT("Freeze expires before slow"), Elements->IsFrozen());
    TestTrue(TEXT("Slow persists"), Elements->GetMovementMultiplier() < 1);
    Elements->ApplyEffect(Fire, TEXT("FireDragon"), Pawn, FVector::ZeroVector);
    TestEqual(TEXT("Fire and Ice coexist"), Elements->GetEffectCount(), 2);
    Elements->RemoveSource(TEXT("FireDragon"));
    TestEqual(TEXT("Remove source preserves other elements"), Elements->GetEffectCount(), 1);
    Elements->ClearEffects();
    TestEqual(TEXT("Remove restores movement"), Elements->GetMovementMultiplier(), 1.f);
    auto* Neighbor = D->SpawnEnemyOfType(TEXT("MeleeTest"), FVector(600, 0, 100));
    FLoongElementEffect Wind;
    Wind.Element = ELoongElementKind::Wind;
    Wind.EffectId = TEXT("Whirl");
    Wind.Duration = 1;
    Wind.TickInterval = 0.25f;
    Wind.DamagePerTick = 5;
    Wind.Radius = 200;
    Wind.DisplacementPerTick = -30;
    Health = Neighbor->Health->CurrentHealth;
    float X = Neighbor->GetActorLocation().X;
    Elements->ApplyEffect(Wind, TEXT("WindDragon"), Pawn, FVector::ZeroVector);
    Elements->AdvanceCombatTime(0.25f);
    TestEqual(TEXT("Wind area periodic damage"), Neighbor->Health->CurrentHealth, Health - 5);
    TestTrue(TEXT("Wind pulls toward source"), Neighbor->GetActorLocation().X < X);
    Elements->ClearEffects();
    FLoongElementEffect Invalid = Fire;
    Invalid.Duration = -1;
    TestFalse(TEXT("Invalid duration rejected"),
              Elements->ApplyEffect(Invalid, TEXT("Bad"), Pawn, FVector::ZeroVector));
    // Refresh variants.
    Fire.Duration = 1;
    Fire.ApplicationCooldown = 0;
    Fire.RefreshRule = ELoongElementRefresh::KeepDuration;
    Elements->ApplyEffect(Fire, TEXT("Keep"), Pawn, FVector::ZeroVector);
    Elements->AdvanceCombatTime(0.6f);
    Elements->ApplyEffect(Fire, TEXT("Keep"), Pawn, FVector::ZeroVector);
    Elements->AdvanceCombatTime(0.5f);
    TestEqual(TEXT("KeepDuration does not extend life"), Elements->GetEffectCount(), 0);
    Fire.RefreshRule = ELoongElementRefresh::ExtendDuration;
    Elements->ApplyEffect(Fire, TEXT("Extend"), Pawn, FVector::ZeroVector);
    Elements->AdvanceCombatTime(0.6f);
    Elements->ApplyEffect(Fire, TEXT("Extend"), Pawn, FVector::ZeroVector);
    Elements->AdvanceCombatTime(0.5f);
    TestTrue(TEXT("ExtendDuration extends life"), Elements->GetEffectCount() > 0);
    Elements->ClearEffects();
    FLoongDragonAbility Dragon;
    Dragon.DragonId = TEXT("ElementDragon");
    Dragon.bSpawnCompanion = false;
    Dragon.HitEffectChance = 0;
    FLoongElementAbility Skill;
    Skill.TriggerChance = 1;
    Skill.TriggerCooldown = 1;
    Skill.Effect = Fire;
    Dragon.ElementAbilities.Add(Skill);
    D->EquipDragon(Dragon);
    D->ApplyHit(E, Pawn, 1, true);
    TestEqual(TEXT("Dragon calls element module"), Elements->GetStacks(Fire.EffectId, Dragon.DragonId), 1);
    D->ApplyHit(E, Pawn, 1, true);
    TestEqual(TEXT("Skill cooldown resists high frequency hits"),
              Elements->GetStacks(Fire.EffectId, Dragon.DragonId), 1);
    D->Elapsed += 1.1f;
    D->ApplyHit(E, Pawn, 1, true);
    TestEqual(TEXT("Skill cooldown reopens"), Elements->GetStacks(Fire.EffectId, Dragon.DragonId), 2);
    D->RemoveDragon(Dragon.DragonId);
    TestEqual(TEXT("Unequip clears owned statuses"), Elements->GetEffectCount(), 0);
    auto* Ranged = D->SpawnEnemyOfType(TEXT("RangedTest"), FVector(700, 0, 100));
    int32 Shots = D->Projectiles.Num();
    Ranged->Tick(0.1f);
    TestEqual(TEXT("Ranged fires projectile"), D->Projectiles.Num(), Shots + 1);
    Ranged->Tick(0.1f);
    TestEqual(TEXT("Ranged cooldown"), D->Projectiles.Num(), Shots + 1);
    D->State = ELoongCombatState::UpgradeSelection;
    Ranged->Tick(10);
    TestEqual(TEXT("Ranged paused"), D->Projectiles.Num(), Shots + 1);
    D->State = ELoongCombatState::Fighting;
    auto* Shot = D->Projectiles.Last().Get();
    Shot->SetActorLocation(Pawn->GetActorLocation() - Shot->FlightDirection * 10);
    float HeroHealth = D->PlayerHealth->CurrentHealth;
    Shot->Tick(0.1f);
    TestTrue(TEXT("Enemy projectile damages player"), D->PlayerHealth->CurrentHealth < HeroHealth);
    Elements->ApplyEffect(Fire, TEXT("Fatal"), Pawn, FVector::ZeroVector);
    E->Health->ApplyCombatDamage(10000, Pawn);
    TestEqual(TEXT("Death removes statuses"), Elements->GetEffectCount(), 0);
    Neighbor->Elements->ApplyEffect(Ice, TEXT("Ice"), Pawn, FVector::ZeroVector);
    D->EndCombat(true);
    TestEqual(TEXT("Settlement removes status"), Neighbor->Elements->GetEffectCount(), 0);
    D->EndCombat(true);
    TestEqual(TEXT("Settlement once"), D->SettlementCount, 1);
    D->Destroy();
    GEngine->DestroyWorldContext(W);
    W->DestroyWorld(false);
    W->RemoveFromRoot();
    return true;
}
#endif
