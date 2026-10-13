#include "LoongCombatDemo.h"
#include "LoongCombatWidget.h"
#include "LoongBuildingSystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"

ULoongCombatDefinition::ULoongCombatDefinition()
{
 Dragons.Add(FLoongDragonAbility());
 FLoongUpgradeDefinition Speed;Speed.UpgradeId=TEXT("AttackSpeed");Speed.DisplayName=FText::FromString(TEXT("迅捷攻击"));Speed.Description=FText::FromString(TEXT("攻速提高15%"));Speed.Effect=ELoongUpgradeEffect::AttackSpeed;Speed.Magnitude=0.15f;
 FLoongUpgradeDefinition Damage;Damage.UpgradeId=TEXT("AttackDamage");Damage.DisplayName=FText::FromString(TEXT("强化攻击"));Damage.Description=FText::FromString(TEXT("伤害提高20%"));Damage.Effect=ELoongUpgradeEffect::AttackDamage;Damage.Magnitude=0.2f;
 FLoongUpgradeDefinition Element;Element.UpgradeId=TEXT("ElementPower");Element.DisplayName=FText::FromString(TEXT("龙族元素"));Element.Description=FText::FromString(TEXT("龙族额外伤害提高25%"));Element.Effect=ELoongUpgradeEffect::ElementDamage;Element.Magnitude=0.25f;
 Upgrades={Speed,Damage,Element};
}
void ULoongCombatHealthComponent::ResetHealth(float InMaxHealth){MaxHealth=FMath::Max(1.f,InMaxHealth);CurrentHealth=MaxHealth;}
float ULoongCombatHealthComponent::ApplyCombatDamage(float Damage,AActor* Source)
{
 if(!IsAlive() || !FMath::IsFinite(Damage) || Damage<=0)return 0;
 float Applied=FMath::Min(Damage,CurrentHealth);CurrentHealth-=Applied;OnDamaged.Broadcast(Applied,Source);
 if(!IsAlive()){NativeDeath.Broadcast();OnDeath.Broadcast();}return Applied;
}
ULoongAutoAttackComponent::ULoongAutoAttackComponent(){PrimaryComponentTick.bCanEverTick=true;}
ALoongCombatEnemy* ULoongAutoAttackComponent::FindTarget() const
{
 if(!Director.IsValid() || !GetOwner())return nullptr;
 ALoongCombatEnemy* Best=nullptr;float BestSquared=FMath::Square(Stats.AttackRange);
 for(auto& Enemy:Director->Enemies)if(IsValid(Enemy) && !Enemy->IsActorBeingDestroyed() && Enemy->Health->IsAlive())
 {float Distance=FVector::DistSquared(GetOwner()->GetActorLocation(),Enemy->GetActorLocation());if(Distance<BestSquared){BestSquared=Distance;Best=Enemy;}}
 return Best;
}
void ULoongAutoAttackComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Fn)
{
 Super::TickComponent(DeltaTime,TickType,Fn);
 if(!Director.IsValid() || !Director->IsFighting() || !Director->PlayerHealth || !Director->PlayerHealth->IsAlive())return;
 Cooldown-=DeltaTime;if(Cooldown>0)return;
 auto* Enemy=FindTarget();if(!Enemy){Cooldown=0.1f;return;}
 Cooldown=FMath::Max(0.05f,Stats.AttackInterval);
 Director->FireProjectile(GetOwner(),Enemy,Stats,bHeroAttack);OnShot.Broadcast(GetOwner()->GetActorLocation(),Enemy->GetActorLocation());
}
ALoongCombatEnemy::ALoongCombatEnemy()
{
 PrimaryActorTick.bCanEverTick=true;
 Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));SetRootComponent(Collision);Collision->InitSphereRadius(35);Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));Mesh->SetupAttachment(Collision);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 AnimatedMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedVisual"));AnimatedMesh->SetupAttachment(Collision);AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Health=CreateDefaultSubobject<ULoongCombatHealthComponent>(TEXT("Health"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));if(Cube.Succeeded())Mesh->SetStaticMesh(Cube.Object);
 Mesh->SetRelativeScale3D(FVector(0.65,0.65,1.1));Health->OnDeath.AddDynamic(this,&ALoongCombatEnemy::Died);
}
void ALoongCombatEnemy::Configure(const FLoongEnemyDefinition& InDefinition)
{
 Definition=InDefinition;Health->ResetHealth(Definition.MaxHealth);Health->NativeDeath.RemoveAll(this);Health->NativeDeath.AddUObject(this,&ALoongCombatEnemy::Died);
 if(Definition.Mesh)Mesh->SetStaticMesh(Definition.Mesh);
 if(Definition.SkeletalMesh){AnimatedMesh->SetSkeletalMesh(Definition.SkeletalMesh);Mesh->SetVisibility(false);}
 if(Definition.Material){Mesh->SetMaterial(0,Definition.Material);AnimatedMesh->SetMaterial(0,Definition.Material);}
 else {auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));if(Base){auto* Tint=UMaterialInstanceDynamic::Create(Base,this);Tint->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.7f,0.12f,0.08f));Mesh->SetMaterial(0,Tint);}}
}
void ALoongCombatEnemy::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);if(!Director.IsValid() || !Director->IsFighting() || !Health->IsAlive() || !Director->Player)return;
 FVector Delta=Director->Player->GetActorLocation()-GetActorLocation();Delta.Z=0;float Distance=Delta.Size();
 if(Distance>Definition.ContactRange*0.8f)SetActorLocation(GetActorLocation()+Delta.GetSafeNormal()*FMath::Min(Definition.MoveSpeed*DeltaTime,Distance));
 if(!Delta.IsNearlyZero())SetActorRotation(Delta.Rotation());
 ContactCooldown-=DeltaTime;
 if(Distance<=Definition.ContactRange && ContactCooldown<=0){ContactCooldown=FMath::Max(0.1f,Definition.ContactInterval);Director->PlayerHealth->ApplyCombatDamage(Definition.ContactDamage,this);}
}
void ALoongCombatEnemy::Died()
{
 if(bDeathReported)return;bDeathReported=true;if(Director.IsValid())Director->EnemyKilled(this);Destroy();
}
ALoongCombatProjectile::ALoongCombatProjectile()
{
 PrimaryActorTick.bCanEverTick=true;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual"));SetRootComponent(Mesh);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));if(Sphere.Succeeded())Mesh->SetStaticMesh(Sphere.Object);Mesh->SetRelativeScale3D(FVector(0.15));
}
void ALoongCombatProjectile::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);
 if(!Director.IsValid() || !Director->PlayerHealth || !Director->PlayerHealth->IsAlive() || !Target.IsValid() || !Target->Health->IsAlive()){Destroy();return;}
 if(!Director->IsFighting())return;
 LifeSeconds+=DeltaTime;if(LifeSeconds>4){Destroy();return;}
 FVector Delta=Target->GetActorLocation()-GetActorLocation();float Step=Speed*DeltaTime;
 if(Delta.Size()<=Step+35){ResolveHit();return;}
 SetActorLocation(GetActorLocation()+Delta.GetSafeNormal()*Step);SetActorRotation(Delta.Rotation());
}
void ALoongCombatProjectile::ResolveHit()
{
 if(bConsumed || !Director.IsValid() || !Director->IsFighting())return;bConsumed=true;
 if(Director.IsValid() && Director->IsFighting() && Target.IsValid())Director->ApplyHit(Target.Get(),Attacker.Get(),Damage,bHeroAttack);
 Destroy();
}
ALoongCombatDragon::ALoongCombatDragon()
{
 PrimaryActorTick.bCanEverTick=true;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DragonVisual"));SetRootComponent(Mesh);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 AnimatedMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedDragon"));AnimatedMesh->SetupAttachment(Mesh);AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Attack=CreateDefaultSubobject<ULoongAutoAttackComponent>(TEXT("DragonAttack"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));if(Sphere.Succeeded())Mesh->SetStaticMesh(Sphere.Object);Mesh->SetRelativeScale3D(FVector(1.3,0.7,0.7));
}
void ALoongCombatDragon::Configure(const FLoongDragonAbility& Ability)
{
 Definition=Ability;Attack->Director=Director;Attack->Stats.AttackDamage=Ability.CompanionDamage;Attack->Stats.AttackInterval=Ability.CompanionInterval;Attack->Stats.AttackRange=Ability.CompanionRange;
 if(Ability.Mesh)Mesh->SetStaticMesh(Ability.Mesh);
 if(Ability.SkeletalMesh){AnimatedMesh->SetSkeletalMesh(Ability.SkeletalMesh);Mesh->SetVisibility(false,false);}
 if(Ability.Material){Mesh->SetMaterial(0,Ability.Material);AnimatedMesh->SetMaterial(0,Ability.Material);}
 else {auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));if(Base){auto* Tint=UMaterialInstanceDynamic::Create(Base,this);Tint->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.1f,0.75f,0.4f));Mesh->SetMaterial(0,Tint);}}
}
void ALoongCombatDragon::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);if(!Director.IsValid() || !Director->IsFighting() || !Director->Player)return;
 SetActorLocation(FMath::VInterpTo(GetActorLocation(),Director->Player->GetActorLocation()+Definition.FollowOffset,DeltaTime,5));
}
ALoongCombatDirector::ALoongCombatDirector(){PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));}
bool ALoongCombatDirector::StartCombat(APawn* InPlayer)
{
 if(State!=ELoongCombatState::Preparing || !IsValid(InPlayer))return false;
 if(InPlayer->FindComponentByClass<ULoongAutoAttackComponent>())return false;
 if(!Definition)Definition=NewObject<ULoongCombatDefinition>(this);
 Player=InPlayer;Random.Initialize(Definition->RandomSeed);
 PlayerHealth=NewObject<ULoongCombatHealthComponent>(Player);Player->AddInstanceComponent(PlayerHealth);PlayerHealth->RegisterComponent();PlayerHealth->ResetHealth(Definition->HeroStats.MaxHealth);PlayerHealth->OnDeath.AddDynamic(this,&ALoongCombatDirector::PlayerDied);PlayerHealth->NativeDeath.AddUObject(this,&ALoongCombatDirector::PlayerDied);
 PlayerAttack=NewObject<ULoongAutoAttackComponent>(Player);Player->AddInstanceComponent(PlayerAttack);PlayerAttack->Director=this;PlayerAttack->bHeroAttack=true;PlayerAttack->RegisterComponent();
 if(auto* Character=Cast<ACharacter>(Player)){OriginalMoveSpeed=Character->GetCharacterMovement()->MaxWalkSpeed;bHasOriginalMoveSpeed=true;Character->GetCharacterMovement()->MaxWalkSpeed=Definition->HeroStats.MoveSpeed;if(Definition->CharacterMesh)Character->GetMesh()->SetSkeletalMesh(Definition->CharacterMesh);}
 State=ELoongCombatState::Fighting;
 for(const auto& Ability:Definition->Dragons)EquipDragon(Ability);
 RecalculateStats();
 for(int32 i=0;i<FMath::Clamp(Definition->InitialEnemies,0,Definition->MaxEnemies);++i)
 {float A=Random.FRandRange(0,2*PI);SpawnEnemyAt(GetActorLocation()+FVector(FMath::Cos(A)*900,FMath::Sin(A)*900,100));}
 auto* PC=Cast<APlayerController>(Player->GetController());
 if(PC && PC->GetLocalPlayer()){PC->bShowMouseCursor=true;Widget=CreateWidget<ULoongCombatWidget>(PC);if(Widget){Widget->Setup(this);Widget->AddToViewport(100);}}
 return true;
}
FLoongCombatStats ALoongCombatDirector::GetEffectiveStats() const
{
 FLoongCombatStats Stats=Definition?Definition->HeroStats:FLoongCombatStats();float Bonus=0;float DamageBonus=0;float SpeedBonus=0;
 for(const auto& Pair:EquippedDragons)Bonus+=Pair.Value.AttackBonusPercent;
 if(Definition)for(const auto& U:Definition->Upgrades){int32 Rank=UpgradeLevels.FindRef(U.UpgradeId);if(U.Effect==ELoongUpgradeEffect::AttackDamage)DamageBonus+=Rank*U.Magnitude;else if(U.Effect==ELoongUpgradeEffect::AttackSpeed)SpeedBonus+=Rank*U.Magnitude;}
 Stats.AttackDamage*=FMath::Max(0.f,1+Bonus+DamageBonus);Stats.AttackInterval=FMath::Max(0.05f,Stats.AttackInterval/FMath::Max(0.1f,1+SpeedBonus));return Stats;
}
void ALoongCombatDirector::RecalculateStats(){if(PlayerAttack)PlayerAttack->Stats=GetEffectiveStats();}
void ALoongCombatDirector::EquipDragon(FLoongDragonAbility Ability)
{
 if(Ability.DragonId.IsNone() || !IsFighting())return;
 RemoveDragon(Ability.DragonId);EquippedDragons.Add(Ability.DragonId,Ability);RecalculateStats();
 if(Ability.bSpawnCompanion){auto* Dragon=GetWorld()->SpawnActor<ALoongCombatDragon>(Player->GetActorLocation()+Ability.FollowOffset,FRotator::ZeroRotator);if(Dragon){Dragon->Director=this;Dragon->Configure(Ability);Companions.Add(Dragon);}}
}
void ALoongCombatDirector::RemoveDragon(FName Id)
{
 EquippedDragons.Remove(Id);for(int32 i=Companions.Num()-1;i>=0;--i)if(!IsValid(Companions[i]) || Companions[i]->Definition.DragonId==Id){if(IsValid(Companions[i]))Companions[i]->Destroy();Companions.RemoveAt(i);}RecalculateStats();
}
ALoongCombatEnemy* ALoongCombatDirector::SpawnEnemyAt(FVector Position)
{
 Enemies.RemoveAll([](const auto& E){return !IsValid(E) || E->IsActorBeingDestroyed();});
 if(!IsFighting() || !Definition || Enemies.Num()>=FMath::Clamp(Definition->MaxEnemies,0,200))return nullptr;
 auto* Enemy=GetWorld()->SpawnActor<ALoongCombatEnemy>(Position,FRotator::ZeroRotator);if(Enemy){Enemy->Director=this;Enemy->Configure(Definition->Enemy);Enemies.Add(Enemy);}return Enemy;
}
void ALoongCombatDirector::FireProjectile(AActor* Source,ALoongCombatEnemy* Enemy,FLoongCombatStats Stats,bool bHero)
{
 if(!IsFighting() || !IsValid(Source) || !IsValid(Enemy) || !Enemy->Health->IsAlive())return;
 auto* Shot=GetWorld()->SpawnActor<ALoongCombatProjectile>(Source->GetActorLocation()+FVector(0,0,30),FRotator::ZeroRotator);
 if(!Shot)return;Shot->Director=this;Shot->Target=Enemy;Shot->Attacker=Source;Shot->Damage=Stats.AttackDamage;Shot->Speed=FMath::Max(1.f,Stats.ProjectileSpeed);Shot->bHeroAttack=bHero;
 if(Definition->ProjectileMesh)Shot->Mesh->SetStaticMesh(Definition->ProjectileMesh);if(Definition->ProjectileMaterial)Shot->Mesh->SetMaterial(0,Definition->ProjectileMaterial);
 if(Definition->ProjectileEffect)UNiagaraFunctionLibrary::SpawnSystemAttached(Definition->ProjectileEffect,Shot->Mesh,NAME_None,FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true);
 Projectiles.Add(Shot);
}
float ALoongCombatDirector::ApplyHit(ALoongCombatEnemy* Enemy,AActor* Source,float Damage,bool bHero)
{
 if(!IsFighting() || !IsValid(Enemy) || !Enemy->Health->IsAlive())return 0;
 float Applied=Enemy->Health->ApplyCombatDamage(Damage,Source);
 if(Definition->HitEffect)UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Definition->HitEffect,Enemy->GetActorLocation());
 if(bHero && Enemy->Health->IsAlive())
 {
  float Multiplier=1;for(const auto& U:Definition->Upgrades)if(U.Effect==ELoongUpgradeEffect::ElementDamage)Multiplier+=UpgradeLevels.FindRef(U.UpgradeId)*U.Magnitude;
  for(const auto& Pair:EquippedDragons)if(Random.FRand()<FMath::Clamp(Pair.Value.HitEffectChance,0.f,1.f))
  {Applied+=Enemy->Health->ApplyCombatDamage(Pair.Value.ExtraElementDamage*Multiplier,Source);if(Pair.Value.SkillEffect)UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Pair.Value.SkillEffect,Enemy->GetActorLocation());if(!Enemy->Health->IsAlive())break;}
 }
 return Applied;
}
void ALoongCombatDirector::EnemyKilled(ALoongCombatEnemy* Enemy)
{
 if(!IsFighting() || !Enemy || !Enemies.Contains(Enemy))return;
 ++Kills;Enemies.Remove(Enemy);AddExperience(FMath::Max(0,Enemy->Definition.ExperienceReward));
}
void ALoongCombatDirector::AddExperience(int32 Amount)
{
 if((State!=ELoongCombatState::Fighting && State!=ELoongCombatState::UpgradeSelection) || Amount<=0)return;
 Experience=FMath::Min<int64>(static_cast<int64>(Experience)+Amount,1000000);
 if(State==ELoongCombatState::Fighting && Experience>=ExperienceThreshold()){Experience-=ExperienceThreshold();++Level;OfferUpgrades();}
}
bool ALoongCombatDirector::IsUpgradeEligible(const FLoongUpgradeDefinition& U) const
{
 if(U.UpgradeId.IsNone() || U.Weight<=0 || U.MaxLevel<=UpgradeLevels.FindRef(U.UpgradeId))return false;
 for(const auto& P:U.Prerequisites)if(UpgradeLevels.FindRef(P.Key)<P.Value)return false;return true;
}
void ALoongCombatDirector::OfferUpgrades()
{
 ++UpgradeGeneration;UpgradeChoices.Reset();TArray<const FLoongUpgradeDefinition*> Pool;
 for(const auto& U:Definition->Upgrades)if(IsUpgradeEligible(U))Pool.Add(&U);
 for(int32 n=0;n<3 && Pool.Num()>0;++n)
 {float Total=0;for(const auto* U:Pool)Total+=U->Weight;float Roll=Random.FRandRange(0,Total);int32 Pick=Pool.Num()-1;for(int32 i=0;i<Pool.Num();++i){Roll-=Pool[i]->Weight;if(Roll<=0){Pick=i;break;}}UpgradeChoices.Add(Pool[Pick]->UpgradeId);FName SelectedId=Pool[Pick]->UpgradeId;Pool.RemoveAll([SelectedId](const FLoongUpgradeDefinition* U){return U->UpgradeId==SelectedId;});}
 State=UpgradeChoices.Num()>0?ELoongCombatState::UpgradeSelection:ELoongCombatState::Fighting;
 if(auto* C=Cast<ACharacter>(Player)){C->GetCharacterMovement()->StopMovementImmediately();if(State==ELoongCombatState::UpgradeSelection)C->GetCharacterMovement()->DisableMovement();}
}
bool ALoongCombatDirector::ChooseUpgrade(FName Id,int32 ExpectedGeneration)
{
 if((ExpectedGeneration>=0 && ExpectedGeneration!=UpgradeGeneration) || State!=ELoongCombatState::UpgradeSelection || !UpgradeChoices.Contains(Id))return false;
 const auto* U=Definition->Upgrades.FindByPredicate([Id](const FLoongUpgradeDefinition& Other){return Other.UpgradeId==Id;});if(!U || !IsUpgradeEligible(*U))return false;
 UpgradeLevels.FindOrAdd(Id)++;UpgradeChoices.Reset();RecalculateStats();State=ELoongCombatState::Fighting;
 if(auto* C=Cast<ACharacter>(Player))C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 if(Experience>=ExperienceThreshold()){Experience-=ExperienceThreshold();++Level;OfferUpgrades();}return true;
}
void ALoongCombatDirector::PlayerDied(){EndCombat(false);}
void ALoongCombatDirector::CleanupActors()
{
 for(auto& P:Projectiles)if(IsValid(P))P->Destroy();Projectiles.Reset();
 for(auto& E:Enemies)if(IsValid(E))E->Destroy();Enemies.Reset();
 for(auto& D:Companions)if(IsValid(D))D->Destroy();Companions.Reset();
}
void ALoongCombatDirector::EndCombat(bool bVictory)
{
 if(State!=ELoongCombatState::Fighting && State!=ELoongCombatState::UpgradeSelection)return;
 State=bVictory?ELoongCombatState::Victory:ELoongCombatState::Defeat;
 Result.bVictory=bVictory;Result.Kills=Kills;Result.Level=Level;Result.FightingSeconds=Elapsed;Result.TestReward=FMath::Max(0,Kills*2+(bVictory?100:0));
 ++SettlementCount;CleanupActors();UpgradeLevels.Reset();EquippedDragons.Reset();RecalculateStats();State=ELoongCombatState::Results;OnSettled.Broadcast(Result);
 if(PlayerAttack)PlayerAttack->SetComponentTickEnabled(false);
 if(auto* C=Cast<ACharacter>(Player)){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();if(bHasOriginalMoveSpeed)C->GetCharacterMovement()->MaxWalkSpeed=OriginalMoveSpeed;}
}
void ALoongCombatDirector::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);if(!IsValid(Player) || !PlayerHealth){if(IsFighting())EndCombat(false);return;}
 if((State==ELoongCombatState::Fighting || State==ELoongCombatState::UpgradeSelection) && !PlayerHealth->IsAlive()){EndCombat(false);return;}
 if(!IsFighting()){if(auto* C=Cast<ACharacter>(Player)){C->GetCharacterMovement()->StopMovementImmediately();C->ConsumeMovementInputVector();}return;}
 Elapsed+=DeltaTime;
 if(Elapsed>=FMath::Max(1.f,Definition->DurationSeconds)){EndCombat(true);return;}
 Enemies.RemoveAll([](const auto& E){return !IsValid(E) || E->IsActorBeingDestroyed();});Projectiles.RemoveAll([](const auto& P){return !IsValid(P) || P->IsActorBeingDestroyed();});
 SpawnCountdown-=DeltaTime;
 if(SpawnCountdown<=0)
 {SpawnCountdown=FMath::Max(0.35f,Definition->SpawnInterval/(1+Elapsed/60));for(int32 n=0;n<1+static_cast<int32>(Elapsed/60);++n){float A=Random.FRandRange(0,2*PI);FVector P=Player->GetActorLocation()+FVector(FMath::Cos(A)*1300,FMath::Sin(A)*1300,0);P.X=FMath::Clamp(P.X,GetActorLocation().X-Definition->ArenaHalfWidth,GetActorLocation().X+Definition->ArenaHalfWidth);P.Y=FMath::Clamp(P.Y,GetActorLocation().Y-Definition->ArenaHalfWidth,GetActorLocation().Y+Definition->ArenaHalfWidth);P.Z=100;SpawnEnemyAt(P);}}
}
void ALoongCombatDirector::ReturnToBase(){if(State==ELoongCombatState::Results && !BaseMap.IsEmpty())UGameplayStatics::OpenLevel(this,FName(*BaseMap));}
void ALoongCombatDirector::EndPlay(const EEndPlayReason::Type Reason)
{
 CleanupActors();if(Widget)Widget->RemoveFromParent();if(PlayerHealth)PlayerHealth->NativeDeath.RemoveAll(this);if(PlayerHealth)PlayerHealth->OnDeath.RemoveDynamic(this,&ALoongCombatDirector::PlayerDied);if(PlayerAttack)PlayerAttack->DestroyComponent();if(PlayerHealth)PlayerHealth->DestroyComponent();Super::EndPlay(Reason);
}
ALoongCombatGameMode::ALoongCombatGameMode(){PrimaryActorTick.bCanEverTick=true;Tags.AddUnique(TEXT("LoongCombatMode"));}
void ALoongCombatGameMode::Tick(float DeltaTime)
{
 Super::Tick(DeltaTime);if(Combat)return;
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC || !PC->GetPawn())return;
 for(TActorIterator<ALoongCombatDirector> It(GetWorld());It;++It){Combat=*It;break;}
 if(!Combat)Combat=GetWorld()->SpawnActor<ALoongCombatDirector>();
 if(!Combat)return;
 if(CombatDefinition)Combat->Definition=CombatDefinition;
 Combat->StartCombat(PC->GetPawn());
 bool bCamera=false;for(TActorIterator<ALoongArtTestCamera> It(GetWorld());It;++It){bCamera=true;break;}if(!bCamera)GetWorld()->SpawnActor<ALoongArtTestCamera>();
}
