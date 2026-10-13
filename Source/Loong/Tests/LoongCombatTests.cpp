#if WITH_DEV_AUTOMATION_TESTS
#include "../LoongCombatDemo.h"
#include "../LoongBuildingSystemComponent.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoongCombatLoopTest,"Loong.Combat.LoopAndAbilities",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoongCombatLoopTest::RunTest(const FString& Parameters)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("LoongCombatTests")),GetTransientPackage(),true,ERHIFeatureLevel::SM6,&Init);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Pawn=W->SpawnActor<ACharacter>();auto* D=W->SpawnActor<ALoongCombatDirector>();D->Definition=NewObject<ULoongCombatDefinition>();D->Definition->InitialEnemies=0;D->Definition->Dragons.Reset();
 TestTrue(TEXT("Preparing -> fighting"),D->StartCombat(Pawn));TestFalse(TEXT("Start twice rejected"),D->StartCombat(Pawn));
 auto* Near=D->SpawnEnemyAt(FVector(200,0,100));auto* Far=D->SpawnEnemyAt(FVector(600,0,100));
 TestTrue(TEXT("Closest registered enemy is targeted"),D->PlayerAttack->FindTarget()==Near);
 const float HP=Near->Health->CurrentHealth;
 D->PlayerAttack->TickComponent(0.01f,LEVELTICK_All,nullptr);TestEqual(TEXT("Auto attack launches projectile"),D->Projectiles.Num(),1);
 D->PlayerAttack->TickComponent(0.01f,LEVELTICK_All,nullptr);TestEqual(TEXT("Attack interval prevents extra shot"),D->Projectiles.Num(),1);
 auto* P=D->Projectiles[0].Get();P->ResolveHit();P->ResolveHit();TestEqual(TEXT("Projectile hit applies once"),Near->Health->CurrentHealth,HP-D->PlayerAttack->Stats.AttackDamage);
 FLoongDragonAbility A;A.DragonId=TEXT("Wind");A.bSpawnCompanion=false;A.HitEffectChance=1;
 D->EquipDragon(A);TestTrue(TEXT("Dragon buffs damage 10%"),FMath::IsNearlyEqual(D->GetEffectiveStats().AttackDamage,27.5f));
 D->EquipDragon(A);TestTrue(TEXT("Same dragon replacement does not accumulate"),FMath::IsNearlyEqual(D->GetEffectiveStats().AttackDamage,27.5f));
 D->RemoveDragon(A.DragonId);TestTrue(TEXT("Unequip recomputes base damage"),FMath::IsNearlyEqual(D->GetEffectiveStats().AttackDamage,25.f));
 D->EquipDragon(A);const float Before=Far->Health->CurrentHealth;D->ApplyHit(Far,Pawn,5,true);TestTrue(TEXT("Guaranteed hit effect adds element damage"),FMath::IsNearlyEqual(Far->Health->CurrentHealth,Before-5-A.ExtraElementDamage));
 D->RemoveDragon(A.DragonId);A.bSpawnCompanion=true;D->EquipDragon(A);TestEqual(TEXT("One companion"),D->Companions.Num(),1);
 auto* Dragon=D->Companions[0].Get();Dragon->SetActorLocation(Far->GetActorLocation()+FVector(0,0,150));
 TestNotNull(TEXT("Companion independent targeting"),Dragon->Attack->FindTarget());
 int32 Shots=D->Projectiles.Num();Dragon->Attack->TickComponent(0.01f,LEVELTICK_All,nullptr);TestEqual(TEXT("Companion attacks independently"),D->Projectiles.Num(),Shots+1);
 FVector OldPos=Far->GetActorLocation();Far->Tick(0.1f);TestTrue(TEXT("Enemy chases"),FVector::DistSquared(Far->GetActorLocation(),Pawn->GetActorLocation())<FVector::DistSquared(OldPos,Pawn->GetActorLocation()));
 Far->SetActorLocation(Pawn->GetActorLocation()+FVector(50,0,0));float HeroHP=D->PlayerHealth->CurrentHealth;Far->Tick(0.1f);float AfterContact=D->PlayerHealth->CurrentHealth;Far->Tick(0.1f);TestTrue(TEXT("Contact damage occurs"),AfterContact<HeroHP);TestEqual(TEXT("Contact cooldown prevents repeated damage"),D->PlayerHealth->CurrentHealth,AfterContact);
 D->Definition->MaxEnemies=D->Enemies.Num();TestNull(TEXT("Spawn count cap"),D->SpawnEnemyAt(FVector(900,0,0)));
 FLoongUpgradeDefinition Locked;Locked.UpgradeId=TEXT("Prereq");Locked.Prerequisites.Add(TEXT("AttackDamage"),1);TestFalse(TEXT("Prerequisite excluded"),D->IsUpgradeEligible(Locked));
 D->AddExperience(D->ExperienceThreshold());TestEqual(TEXT("Upgrade pauses combat"),D->State,ELoongCombatState::UpgradeSelection);TestEqual(TEXT("Three unique cards"),D->UpgradeChoices.Num(),3);
 float Time=D->Elapsed;D->Tick(10);TestEqual(TEXT("Paused timer does not advance"),D->Elapsed,Time);Shots=D->Projectiles.Num();Dragon->Attack->TickComponent(2,LEVELTICK_All,nullptr);TestEqual(TEXT("Paused attacks stop"),D->Projectiles.Num(),Shots);
 FName Pick=D->UpgradeChoices[0];TestTrue(TEXT("Valid upgrade applied"),D->ChooseUpgrade(Pick));TestFalse(TEXT("Same click cannot apply twice"),D->ChooseUpgrade(Pick));TestEqual(TEXT("One upgrade rank"),D->UpgradeLevels.FindRef(Pick),1);
 D->Definition->DurationSeconds=1;D->Tick(2);TestEqual(TEXT("Timeout reaches results"),D->State,ELoongCombatState::Results);TestTrue(TEXT("Victory result"),D->Result.bVictory);D->EndCombat(false);D->EndCombat(true);TestEqual(TEXT("Settlement only once"),D->SettlementCount,1);TestEqual(TEXT("Run upgrades cleared"),D->UpgradeLevels.Num(),0);TestEqual(TEXT("Enemies cleaned"),D->Enemies.Num(),0);TestEqual(TEXT("Dragons cleaned"),D->Companions.Num(),0);
 auto* D2=W->SpawnActor<ALoongCombatDirector>();auto* P2=W->SpawnActor<ACharacter>(FVector(4000,4000,100),FRotator::ZeroRotator);D2->Definition=NewObject<ULoongCombatDefinition>();D2->Definition->InitialEnemies=0;D2->Definition->Dragons.Reset();TestTrue(TEXT("Second test run starts"),D2->StartCombat(P2));
 D2->PlayerHealth->ApplyCombatDamage(99999,nullptr);D2->PlayerHealth->ApplyCombatDamage(99,nullptr);TestFalse(TEXT("Health zero means dead"),D2->PlayerHealth->IsAlive());TestEqual(TEXT("Defeat settles once"),D2->SettlementCount,1);TestFalse(TEXT("Defeat flag"),D2->Result.bVictory);
 auto* D3=W->SpawnActor<ALoongCombatDirector>();auto* P3=W->SpawnActor<ACharacter>(FVector(-4000,-4000,100),FRotator::ZeroRotator);
 D3->Definition=NewObject<ULoongCombatDefinition>();D3->Definition->InitialEnemies=0;D3->Definition->Dragons.Reset();D3->Definition->HeroStats.MaxHealth=100000;D3->Definition->HeroStats.AttackDamage=100;D3->Definition->Enemy.ExperienceReward=3;D3->Definition->MaxEnemies=20;D3->Definition->Upgrades[0].MaxLevel=99;D3->Definition->Upgrades[1].MaxLevel=99;D3->Definition->Upgrades[2].MaxLevel=99;
 TestTrue(TEXT("180 second simulation starts"),D3->StartCombat(P3));
 auto* KillTarget=D3->SpawnEnemyAt(P3->GetActorLocation()+FVector(100,0,0));int32 BeforeKills=D3->Kills;KillTarget->Health->ApplyCombatDamage(9999,P3);KillTarget->Health->ApplyCombatDamage(9999,P3);TestEqual(TEXT("Death awards XP and kill exactly once"),D3->Kills,BeforeKills+1);
 D3->AddExperience(90);int32 Ranks=0;
 int32 StaleGeneration=D3->UpgradeGeneration;FName FirstChoice=D3->UpgradeChoices[0];D3->ChooseUpgrade(FirstChoice,StaleGeneration);++Ranks;
 TestFalse(TEXT("Stale card cannot choose next upgrade batch"),D3->ChooseUpgrade(FirstChoice,StaleGeneration));
 while(D3->State==ELoongCombatState::UpgradeSelection && Ranks<10){D3->ChooseUpgrade(D3->UpgradeChoices[0]);++Ranks;}
 TestTrue(TEXT("XP overflow safely drives several choices"),Ranks>=2);TestTrue(TEXT("Overflow resumes combat"),D3->IsFighting());
 FLoongUpgradeDefinition AtCap;AtCap.UpgradeId=TEXT("AtCap");AtCap.MaxLevel=1;D3->UpgradeLevels.Add(AtCap.UpgradeId,1);TestFalse(TEXT("Rank cap excludes candidate"),D3->IsUpgradeEligible(AtCap));
 for(int32 Step=0;Step<2400 && D3->State!=ELoongCombatState::Results;++Step)
 {
  if(D3->State==ELoongCombatState::UpgradeSelection){D3->ChooseUpgrade(D3->UpgradeChoices[0]);continue;}
  D3->Tick(0.1f);D3->PlayerAttack->TickComponent(0.1f,LEVELTICK_All,nullptr);
  auto Enemies=D3->Enemies;for(auto& E:Enemies)if(IsValid(E) && !E->IsActorBeingDestroyed())E->Tick(0.1f);
  auto SimProjectiles=D3->Projectiles;for(auto& Shot:SimProjectiles)if(IsValid(Shot) && !Shot->IsActorBeingDestroyed())Shot->Tick(0.1f);
  TestTrue(TEXT("Population cap maintained during run"),D3->Enemies.Num()<=20);
 }
 TestEqual(TEXT("Full 180 second run settles"),D3->State,ELoongCombatState::Results);TestTrue(TEXT("Complete simulated run victory"),D3->Result.bVictory);TestEqual(TEXT("Complete run one settlement"),D3->SettlementCount,1);
 D3->Destroy();
 auto* Building=NewObject<ULoongBuildingSystemComponent>(Pawn);Pawn->AddInstanceComponent(Building);Building->RegisterComponent();ALoongCombatArtTestGameMode::DisableBaseSystems(Pawn);TestFalse(TEXT("Combat does not persist base"),Building->bEnableBasePersistence);TestFalse(TEXT("Combat does not process build inputs"),Building->bEnableDirectKeyboardInput);
 D->Destroy();D2->Destroy();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);W->RemoveFromRoot();return true;
}
#endif
