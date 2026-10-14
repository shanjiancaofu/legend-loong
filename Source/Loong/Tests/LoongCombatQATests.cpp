#if WITH_DEV_AUTOMATION_TESTS
#include "LoongAutomationTestWorld.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongHealthArithmeticTest, "Loong.Combat.Calculation.HealthDamageGuards",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongHealthArithmeticTest::RunTest(const FString& Parameters)
{
    auto* Health = NewObject<ULoongCombatHealthComponent>();
    Health->ResetHealth(100);
    int32 Deaths = 0;
    Health->NativeDeath.AddLambda(
        [&Deaths]()
        {
            ++Deaths;
        });
    TestEqual(TEXT("Normal damage returns actual loss"), Health->ApplyCombatDamage(25, nullptr), 25.f);
    TestEqual(TEXT("Health decreased"), Health->CurrentHealth, 75.f);
    for (const float Invalid :
         {0.f, -5.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        TestEqual(TEXT("Invalid damage cannot change health"), Health->ApplyCombatDamage(Invalid, nullptr),
                  0.f);
    TestEqual(TEXT("Invalid inputs left health intact"), Health->CurrentHealth, 75.f);
    TestEqual(TEXT("Overkill capped to remaining health"), Health->ApplyCombatDamage(999, nullptr), 75.f);
    TestEqual(TEXT("Health never becomes negative"), Health->CurrentHealth, 0.f);
    Health->ApplyCombatDamage(1, nullptr);
    TestEqual(TEXT("Death callback fires once"), Deaths, 1);
    Health->NativeDeath.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongSpawnPhaseBoundaryTest,
                                 "Loong.Combat.Calculation.SpawnPhaseBoundaries",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongSpawnPhaseBoundaryTest::RunTest(const FString& Parameters)
{
    auto* Definition = NewObject<ULoongCombatDefinition>();
    TestNull(TEXT("Before first phase there is no phase"), Definition->FindSpawnPhase(-1));
    for (const auto& Case :
         TArray<TPair<float, float>>{{0, 0}, {59.99f, 0}, {60, 60}, {119.99f, 60}, {120, 120}})
    {
        const auto* Phase = Definition->FindSpawnPhase(Case.Key);
        if (TestNotNull(TEXT("Boundary resolves a phase"), Phase))
            TestEqual(TEXT("Exact stage boundary"), Phase->StartSeconds, Case.Value);
    }
    Swap(Definition->SpawnPhases[0], Definition->SpawnPhases[2]);
    TestEqual(TEXT("Selection does not depend on authored array order"),
              Definition->FindSpawnPhase(80)->StartSeconds, 60.f);
    FRandomStream First(42), Second(42);
    for (int32 i = 0; i < 32; ++i)
        TestEqual(TEXT("Same seed gives the same weighted sequence"),
                  Definition->SelectSpawnEnemy(80, First)->EnemyId,
                  Definition->SelectSpawnEnemy(80, Second)->EnemyId);
    Definition->SpawnPhases.Reset();
    TestTrue(TEXT("No phases falls back to legacy enemy"),
             Definition->SelectSpawnEnemy(80, First) == &Definition->Enemy);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongBurnClockTest, "Loong.Combat.Status.FireTickPartitionAndExpiry",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongBurnClockTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* SmallFrames = Fixture.Director->SpawnEnemyAt(FVector(500, 0, 100));
    auto* LargeFrame = Fixture.Director->SpawnEnemyAt(FVector(700, 0, 100));
    FLoongElementEffect Fire;
    Fire.bUseFallbackVisual = false;
    Fire.Duration = 1.25f;
    Fire.TickInterval = 0.25f;
    Fire.DamagePerTick = 4;
    SmallFrames->Elements->ApplyEffect(Fire, TEXT("Burn"), Fixture.Player, FVector::ZeroVector);
    LargeFrame->Elements->ApplyEffect(Fire, TEXT("Burn"), Fixture.Player, FVector::ZeroVector);
    SmallFrames->Elements->AdvanceCombatTime(0.125f);
    TestEqual(TEXT("No damage before first tick"), SmallFrames->Health->CurrentHealth, 1000.f);
    for (int32 i = 0; i < 9; ++i)
        SmallFrames->Elements->AdvanceCombatTime(0.125f);
    LargeFrame->Elements->AdvanceCombatTime(2);
    TestEqual(TEXT("Five ticks including the expiration boundary"), SmallFrames->Health->CurrentHealth,
              980.f);
    TestEqual(TEXT("Frame partitions give the same damage"), LargeFrame->Health->CurrentHealth, 980.f);
    TestEqual(TEXT("Burn expires on both paths"),
              SmallFrames->Elements->GetEffectCount() + LargeFrame->Elements->GetEffectCount(), 0);
    LargeFrame->Elements->AdvanceCombatTime(10);
    TestEqual(TEXT("No damage after expiration"), LargeFrame->Health->CurrentHealth, 980.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongIceClockTest, "Loong.Combat.Status.IceFreezeRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongIceClockTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Enemy = Fixture.Director->SpawnEnemyAt(FVector(600, 0, 100));
    FLoongElementEffect Ice;
    Ice.bUseFallbackVisual = false;
    Ice.Element = ELoongElementKind::Ice;
    Ice.EffectId = TEXT("Chill");
    Ice.Duration = 2;
    Ice.ApplicationCooldown = 0;
    Ice.SlowPerStack = 0.2f;
    Ice.FreezeDuration = 0.5f;
    Enemy->Elements->ApplyEffect(Ice, TEXT("Ice"), Fixture.Player, FVector::ZeroVector);
    Enemy->Tick(0.125f);
    TestTrue(TEXT("Actual enemy movement slows from 130 to 104 cm/s"),
             FMath::IsNearlyEqual(Enemy->LastMeasuredMoveSpeed, 104.f));
    Enemy->Elements->ApplyEffect(Ice, TEXT("Ice"), Fixture.Player, FVector::ZeroVector);
    Enemy->Elements->ApplyEffect(Ice, TEXT("Ice"), Fixture.Player, FVector::ZeroVector);
    const FVector FrozenPosition = Enemy->GetActorLocation();
    Enemy->Tick(0.125f);
    TestTrue(TEXT("Frozen enemy stops moving"),
             Enemy->Elements->IsFrozen() && Enemy->GetActorLocation().Equals(FrozenPosition));
    Fixture.Director->AddExperience(Fixture.Director->ExperienceThreshold());
    Enemy->Tick(5);
    TestTrue(TEXT("Upgrade pause does not consume freeze time"), Enemy->Elements->IsFrozen());
    Fixture.Director->ChooseUpgrade(Fixture.Director->UpgradeChoices[0]);
    Enemy->Tick(0.375f);
    TestFalse(TEXT("Freeze expires after combat time resumes"), Enemy->Elements->IsFrozen());
    TestEqual(TEXT("Slow remains after freeze"), Enemy->Elements->GetMovementMultiplier(), 0.4f,
              KINDA_SMALL_NUMBER);
    Enemy->Tick(1.5f);
    TestEqual(TEXT("Expiry restores original movement multiplier"), Enemy->Elements->GetMovementMultiplier(),
              1.f);
    TestTrue(TEXT("Actual enemy speed restored"), FMath::IsNearlyEqual(Enemy->LastMeasuredMoveSpeed, 130.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongWindRadiusTest, "Loong.Combat.Status.WindRadiusBoundary",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongWindRadiusTest::RunTest(const FString& Parameters)
{
    LoongTests::FScopedCombat Fixture;
    auto* Mark = Fixture.Director->SpawnEnemyAt(FVector(500, 0, 100));
    auto* Boundary = Fixture.Director->SpawnEnemyAt(FVector(700, 0, 150));
    auto* Outside = Fixture.Director->SpawnEnemyAt(FVector(701, 0, 100));
    FLoongElementEffect Wind;
    Wind.bUseFallbackVisual = false;
    Wind.Element = ELoongElementKind::Wind;
    Wind.EffectId = TEXT("Whirl");
    Wind.Radius = 200;
    Wind.DisplacementPerTick = -30;
    Wind.DamagePerTick = 5;
    Mark->Elements->ApplyEffect(Wind, TEXT("Wind"), Fixture.Player, FVector::ZeroVector);
    Mark->Elements->AdvanceCombatTime(0.5f);
    TestEqual(TEXT("Exact 2D boundary receives damage"), Boundary->Health->CurrentHealth, 995.f);
    TestTrue(TEXT("Boundary target pulled 30 cm towards source"),
             Boundary->GetActorLocation().Equals(FVector(670, 0, 150)));
    TestEqual(TEXT("Outside radius receives no damage"), Outside->Health->CurrentHealth, 1000.f);
    TestTrue(TEXT("Outside radius does not move"), Outside->GetActorLocation().Equals(FVector(701, 0, 100)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongSettlementReentryTest, "Loong.Combat.Lifecycle.SettlementReentry",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongSettlementReentryTest::RunTest(const FString& Parameters)
{
    for (const bool Victory : {false, true})
    {
        LoongTests::FScopedCombat Fixture;
        auto* Director = Fixture.Director;
        Director->Kills = 7;
        Director->Elapsed = 18;
        Director->Level = 3;
        Director->EndCombat(Victory);
        const FLoongCombatResult Result = Director->Result;
        TestEqual(TEXT("Reward corresponds to actual result"), Result.TestReward, Victory ? 114 : 14);
        Director->Kills = 100;
        Director->EndCombat(!Victory);
        Director->PlayerDied();
        Director->AddExperience(1000);
        Director->Tick(1000);
        TestEqual(TEXT("All duplicate entry points settle once"), Director->SettlementCount, 1);
        TestEqual(TEXT("Repeated calls do not change reward"), Director->Result.TestReward,
                  Result.TestReward);
        TestEqual(TEXT("First outcome wins"), Director->Result.bVictory, Victory);
        TestEqual(TEXT("Result snapshot retains original kills"), Director->Result.Kills, 7);
        TestEqual(TEXT("Ended combat remains in Results"), Director->State, ELoongCombatState::Results);
    }
    return true;
}
#endif
