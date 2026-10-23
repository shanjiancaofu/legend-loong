#include "LoongCombatDemo.h"
#include "LoongCombatWidget.h"
#include "LoongMountPawn.h"
#include "LoongElementDebug.h"
#include "Engine/Engine.h"
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
    FLoongEnemyDefinition Fast;
    Fast.EnemyId = TEXT("FastTest");
    Fast.MaxHealth = 25;
    Fast.MoveSpeed = 240;
    Fast.ContactDamage = 4;
    Fast.ExperienceReward = 7;
    Fast.PlaceholderTint = FLinearColor(0.85f, 0.45f, 0.05f);
    Fast.VisualScale = FVector(0.8f);
    FLoongEnemyDefinition Ranged;
    Ranged.EnemyId = TEXT("RangedTest");
    Ranged.MaxHealth = 35;
    Ranged.MoveSpeed = 110;
    Ranged.AttackPattern = ELoongEnemyAttackPattern::Ranged;
    Ranged.ContactDamage = 7;
    Ranged.ExperienceReward = 10;
    Ranged.PlaceholderTint = FLinearColor(0.6f, 0.1f, 0.75f);
    EnemyDefinitions = {Fast, Ranged};
    FLoongEnemySpawnPhase Early;
    Early.StartSeconds = 0;
    Early.Enemies.Add(FLoongEnemySpawnWeight());
    FLoongEnemySpawnPhase Mixed;
    Mixed.StartSeconds = 60;
    Mixed.IntervalMultiplier = 0.65f;
    Mixed.BatchSize = 2;
    Mixed.Enemies = Early.Enemies;
    FLoongEnemySpawnWeight FastWeight;
    FastWeight.EnemyId = Fast.EnemyId;
    FastWeight.Weight = 0.7f;
    Mixed.Enemies.Add(FastWeight);
    FLoongEnemySpawnWeight RangedWeight;
    RangedWeight.EnemyId = Ranged.EnemyId;
    RangedWeight.Weight = 0.45f;
    Mixed.Enemies.Add(RangedWeight);
    FLoongEnemySpawnPhase Pressure = Mixed;
    Pressure.StartSeconds = 120;
    Pressure.IntervalMultiplier = 0.45f;
    Pressure.BatchSize = 3;
    SpawnPhases = {Early, Mixed, Pressure};
    FLoongDragonAbility FirePreset;
    FirePreset.DragonId = TEXT("FireDragonTest");
    FirePreset.bSpawnCompanion = false;
    FirePreset.HitEffectChance = 0;
    FLoongElementAbility Burn;
    Burn.SkillId = TEXT("BurnOnHit");
    FirePreset.ElementAbilities.Add(Burn);
    FLoongDragonAbility IcePreset = FirePreset;
    IcePreset.DragonId = TEXT("IceDragonTest");
    IcePreset.AttackBonusPercent = 0;
    IcePreset.ElementAbilities.Reset();
    FLoongElementAbility Chill;
    Chill.SkillId = TEXT("ChillOnHit");
    Chill.TriggerChance = 0.45f;
    Chill.Effect.EffectId = TEXT("Chill");
    Chill.Effect.Element = ELoongElementKind::Ice;
    IcePreset.ElementAbilities.Add(Chill);
    FLoongDragonAbility WindPreset = IcePreset;
    WindPreset.DragonId = TEXT("WindDragonTest");
    WindPreset.bSpawnCompanion = true;
    WindPreset.ElementAbilities.Reset();
    FLoongElementAbility Wind;
    Wind.SkillId = TEXT("WhirlOnHit");
    Wind.TriggerCooldown = 2;
    Wind.Effect.EffectId = TEXT("Whirl");
    Wind.Effect.Element = ELoongElementKind::Wind;
    Wind.Effect.DisplacementPerTick = -25;
    WindPreset.ElementAbilities.Add(Wind);
    ElementDragonPresets = {FirePreset, IcePreset, WindPreset};
    FLoongUpgradeDefinition Speed;
    Speed.UpgradeId = TEXT("AttackSpeed");
    Speed.DisplayName = FText::FromString(TEXT("迅捷攻击"));
    Speed.Description = FText::FromString(TEXT("攻速提高15%"));
    Speed.Effect = ELoongUpgradeEffect::AttackSpeed;
    Speed.Magnitude = 0.15f;
    FLoongUpgradeDefinition Damage;
    Damage.UpgradeId = TEXT("AttackDamage");
    Damage.DisplayName = FText::FromString(TEXT("强化攻击"));
    Damage.Description = FText::FromString(TEXT("伤害提高20%"));
    Damage.Effect = ELoongUpgradeEffect::AttackDamage;
    Damage.Magnitude = 0.2f;
    FLoongUpgradeDefinition Element;
    Element.UpgradeId = TEXT("ElementPower");
    Element.DisplayName = FText::FromString(TEXT("龙族元素"));
    Element.Description = FText::FromString(TEXT("龙族额外伤害提高25%"));
    Element.Effect = ELoongUpgradeEffect::ElementDamage;
    Element.Magnitude = 0.25f;
    Upgrades = {Speed, Damage, Element};
}

void ULoongCombatHealthComponent::ResetHealth(float InMaxHealth)
{
    MaxHealth = FMath::Max(1.f, InMaxHealth);
    CurrentHealth = MaxHealth;
}

float ULoongCombatHealthComponent::ApplyCombatDamage(float Damage, AActor* Source)
{
    if (!IsAlive() || !FMath::IsFinite(Damage) || Damage <= 0)
        return 0;
    float Applied = FMath::Min(Damage, CurrentHealth);
    CurrentHealth -= Applied;
    OnDamaged.Broadcast(Applied, Source);
    if (!IsAlive())
    {
        NativeDeath.Broadcast();
        OnDeath.Broadcast();
    }
    return Applied;
}

ULoongAutoAttackComponent::ULoongAutoAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

ALoongCombatEnemy* ULoongAutoAttackComponent::FindTarget() const
{
    if (!Director.IsValid() || Director->IsActorBeingDestroyed() || !IsValid(GetOwner()) ||
        GetOwner()->IsActorBeingDestroyed() || !FMath::IsFinite(Stats.AttackRange) || Stats.AttackRange <= 0)
        return nullptr;
    if (const auto* Dragon = Cast<ALoongCombatDragon>(GetOwner()))
        if (!Director->IsActiveCompanion(Dragon))
            return nullptr;
    ALoongCombatEnemy* Best = nullptr;
    float BestSquared = bUseAimPoint ? TNumericLimits<float>::Max() : FMath::Square(Stats.AttackRange);
    for (auto& Enemy : Director->Enemies)
        if (IsValid(Enemy) && !Enemy->IsActorBeingDestroyed() && IsValid(Enemy->Health) &&
            Enemy->Health->IsAlive())
        {
            if (FVector::DistSquared(GetOwner()->GetActorLocation(), Enemy->GetActorLocation()) >
                FMath::Square(Stats.AttackRange))
                continue;
            float Distance = FVector::DistSquared(bUseAimPoint ? AimPoint : GetOwner()->GetActorLocation(),
                                                  Enemy->GetActorLocation());
            if (Distance < BestSquared)
            {
                BestSquared = Distance;
                Best = Enemy;
            }
        }
    return Best;
}

void ULoongAutoAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* Fn)
{
    Super::TickComponent(DeltaTime, TickType, Fn);
    if (bHeroAttack && Director.IsValid() && Director->IsMounted())
        return;
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0 || !IsValid(GetOwner()) ||
        GetOwner()->IsActorBeingDestroyed() || !Director.IsValid() || Director->IsActorBeingDestroyed() ||
        !Director->IsFighting() || !IsValid(Director->Player) || Director->Player->IsActorBeingDestroyed() ||
        !IsValid(Director->PlayerHealth) || !Director->PlayerHealth->IsAlive() ||
        !FMath::IsFinite(Stats.AttackDamage) || Stats.AttackDamage <= 0 ||
        !FMath::IsFinite(Stats.AttackInterval) || Stats.AttackInterval <= 0)
        return;
    Cooldown -= DeltaTime;
    if (Cooldown > 0)
    {
        bAttackRequested = false;
        return;
    }
    if (!bAutomatic && !bAttackRequested)
        return;
    bAttackRequested = false;
    auto* Enemy = FindTarget();
    if (!Enemy)
    {
        Cooldown = 0.1f;
        return;
    }
    Cooldown = FMath::Max(0.05f, Stats.AttackInterval);
    Director->FireProjectile(GetOwner(), Enemy, Stats, bHeroAttack);
    OnShot.Broadcast(GetOwner()->GetActorLocation(), Enemy->GetActorLocation());
}

ALoongCombatEnemy::ALoongCombatEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(35);
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AnimatedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedVisual"));
    AnimatedMesh->SetupAttachment(Collision);
    AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Health = CreateDefaultSubobject<ULoongCombatHealthComponent>(TEXT("Health"));
    Elements = CreateDefaultSubobject<ULoongElementComponent>(TEXT("Elements"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
        Mesh->SetStaticMesh(Cube.Object);
    Mesh->SetRelativeScale3D(FVector(0.65, 0.65, 1.1));
    Health->OnDeath.AddDynamic(this, &ALoongCombatEnemy::Died);
}

void ALoongCombatEnemy::Configure(const FLoongEnemyDefinition& InDefinition)
{
    Definition = InDefinition;
    Elements->Director = Director;
    Elements->ClearEffects();
    Health->ResetHealth(Definition.MaxHealth);
    Health->NativeDeath.RemoveAll(this);
    Health->NativeDeath.AddUObject(this, &ALoongCombatEnemy::Died);
    Mesh->SetRelativeScale3D(FVector(0.65, 0.65, 1.1) * Definition.VisualScale);
    AnimatedMesh->SetRelativeScale3D(Definition.VisualScale);
    if (Definition.Mesh)
        Mesh->SetStaticMesh(Definition.Mesh);
    if (Definition.SkeletalMesh)
    {
        AnimatedMesh->SetSkeletalMesh(Definition.SkeletalMesh);
        Mesh->SetVisibility(false);
    }
    if (Definition.Material)
    {
        Mesh->SetMaterial(0, Definition.Material);
        AnimatedMesh->SetMaterial(0, Definition.Material);
    }
    else
    {
        auto* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (Base)
        {
            auto* Tint = UMaterialInstanceDynamic::Create(Base, this);
            Tint->SetVectorParameterValue(TEXT("Color"), Definition.PlaceholderTint);
            Mesh->SetMaterial(0, Tint);
        }
    }
}

void ALoongCombatEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!Director.IsValid() || !Director->IsFighting() || !Health->IsAlive() || !Director->Player)
        return;
    LastMeasuredMoveSpeed = 0.f;
    const FVector PreviousMovementPosition = GetActorLocation();
    Elements->AdvanceCombatTime(DeltaTime);
    if (!Health->IsAlive() || !Director->IsFighting())
        return;
    if (Elements->IsFrozen())
        return;
    FVector Delta = Director->GetCombatAvatar()->GetActorLocation() - GetActorLocation();
    Delta.Z = 0;
    float Distance = Delta.Size();
    float Speed = FMath::Max(0.f, Definition.MoveSpeed) * Elements->GetMovementMultiplier();
    if (Definition.AttackPattern == ELoongEnemyAttackPattern::Ranged)
    {
        float MoveSign = Distance > Definition.PreferredDistance * 1.1f
                             ? 1.f
                             : (Distance < Definition.PreferredDistance * 0.8f ? -1.f : 0.f);
        SetActorLocation(GetActorLocation() + Delta.GetSafeNormal() * MoveSign * Speed * DeltaTime);
        RangedCooldown -= DeltaTime;
        if (Distance <= Definition.RangedAttackRange && RangedCooldown <= 0)
        {
            RangedCooldown = FMath::Max(0.1f, Definition.AttackInterval);
            Director->FireEnemyProjectile(this);
        }
    }
    else
    {
        if (Distance > Definition.ContactRange * 0.8f)
            SetActorLocation(GetActorLocation() +
                             Delta.GetSafeNormal() * FMath::Min(Speed * DeltaTime, Distance));
        ContactCooldown -= DeltaTime;
        if (Distance <= Definition.ContactRange && ContactCooldown <= 0)
        {
            ContactCooldown = FMath::Max(0.1f, Definition.ContactInterval);
            Director->PlayerHealth->ApplyCombatDamage(Definition.ContactDamage, this);
        }
    }
    if (!Delta.IsNearlyZero())
        SetActorRotation(Delta.Rotation());
    LastMeasuredMoveSpeed = DeltaTime > SMALL_NUMBER
                                ? FVector::Dist2D(PreviousMovementPosition, GetActorLocation()) / DeltaTime
                                : 0.f;
}

void ALoongCombatEnemy::Died()
{
    if (bDeathReported)
        return;
    bDeathReported = true;
    Elements->ClearEffects();
    if (Director.IsValid())
        Director->EnemyKilled(this);
    if (!Definition.DeathEffect.IsNull())
        if (auto* FX = Definition.DeathEffect.LoadSynchronous())
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), FX, GetActorLocation());
    Destroy();
}

ALoongCombatProjectile::ALoongCombatProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded())
        Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(0.15));
}

void ALoongCombatProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!Director.IsValid() || Director->IsActorBeingDestroyed() || !IsValid(Director->PlayerHealth) ||
        !Director->PlayerHealth->IsAlive())
    {
        Destroy();
        return;
    }
    if (!Director->IsFighting())
        return;
    if (bCompanionProjectile && !Director->IsActiveCompanion(Cast<ALoongCombatDragon>(Attacker.Get())))
    {
        bConsumed = true;
        Destroy();
        return;
    }
    LifeSeconds += DeltaTime;
    if (LifeSeconds > 4)
    {
        Destroy();
        return;
    }
    if (bEnemyProjectile)
    {
        if (!IsValid(Director->Player))
        {
            Destroy();
            return;
        }
        const FVector Start = GetActorLocation(), End = Start + FlightDirection * Speed * DeltaTime;
        const FVector TargetPoint = Director->GetCombatAvatar()->GetActorLocation();
        if (FMath::PointDistToSegment(TargetPoint, Start, End) <= 55)
        {
            ResolveHit();
            return;
        }
        SetActorLocation(End);
        return;
    }
    if (!Target.IsValid() || Target->IsActorBeingDestroyed() || !IsValid(Target->Health) ||
        !Target->Health->IsAlive())
    {
        Destroy();
        return;
    }
    FVector Delta = Target->GetActorLocation() - GetActorLocation();
    float Step = Speed * DeltaTime;
    if (Delta.Size() <= Step + 35)
    {
        ResolveHit();
        return;
    }
    SetActorLocation(GetActorLocation() + Delta.GetSafeNormal() * Step);
    SetActorRotation(Delta.Rotation());
}

void ALoongCombatProjectile::ResolveHit()
{
    if (bConsumed || !Director.IsValid() || !Director->IsFighting())
        return;
    bConsumed = true;
    if (bCompanionProjectile && !Director->IsActiveCompanion(Cast<ALoongCombatDragon>(Attacker.Get())))
    {
        Destroy();
        return;
    }
    if (bEnemyProjectile)
    {
        Director->PlayerHealth->ApplyCombatDamage(Damage, Attacker.Get());
        Destroy();
        return;
    }
    if (Director.IsValid() && Director->IsFighting() && Target.IsValid())
        Director->ApplyHit(Target.Get(), Attacker.Get(), Damage, bHeroAttack);
    Destroy();
}

ALoongCombatDragon::ALoongCombatDragon()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DragonVisual"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AnimatedMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedDragon"));
    AnimatedMesh->SetupAttachment(Mesh);
    AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Attack = CreateDefaultSubobject<ULoongAutoAttackComponent>(TEXT("DragonAttack"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded())
        Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(1.3, 0.7, 0.7));
}

void ALoongCombatDragon::Configure(const FLoongDragonAbility& Ability)
{
    Definition = Ability;
    if (Definition.FollowOffset.ContainsNaN())
        Definition.FollowOffset = FLoongDragonAbility().FollowOffset;
    if (!FMath::IsFinite(Definition.FollowRecoveryDistance) || Definition.FollowRecoveryDistance < 100.f)
        Definition.FollowRecoveryDistance = 2000.f;
    Attack->Director = Director;
    Attack->bHeroAttack = false;
    Attack->Cooldown = 0;
    Attack->Stats.AttackDamage = Ability.CompanionDamage;
    Attack->Stats.AttackInterval = Ability.CompanionInterval;
    Attack->Stats.AttackRange = Ability.CompanionRange;
    if (Ability.Mesh)
        Mesh->SetStaticMesh(Ability.Mesh);
    if (Ability.SkeletalMesh)
    {
        AnimatedMesh->SetSkeletalMesh(Ability.SkeletalMesh);
        Mesh->SetVisibility(false, false);
    }
    if (Ability.Material)
    {
        Mesh->SetMaterial(0, Ability.Material);
        AnimatedMesh->SetMaterial(0, Ability.Material);
    }
    else
    {
        auto* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (Base)
        {
            auto* Tint = UMaterialInstanceDynamic::Create(Base, this);
            Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.75f, 0.4f));
            Mesh->SetMaterial(0, Tint);
        }
    }
}

void ALoongCombatDragon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (IsActorBeingDestroyed() || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0 || !Director.IsValid() ||
        Director->IsActorBeingDestroyed() || !Director->IsFighting() || !Director->IsActiveCompanion(this) ||
        !IsValid(Director->Player) || Director->Player->IsActorBeingDestroyed() ||
        !IsValid(Director->PlayerHealth) || !Director->PlayerHealth->IsAlive())
        return;
    if (Director->IsMounted() && Director->GetSelectedCompanion() == this)
        return;
    const FVector Destination = Director->GetCombatAvatar()->GetActorLocation() + Definition.FollowOffset;
    if (Destination.ContainsNaN())
        return;
    const FVector Current = GetActorLocation();
    if (Current.ContainsNaN() ||
        FVector::DistSquared(Current, Destination) > FMath::Square(Definition.FollowRecoveryDistance))
        SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
    else
        SetActorLocation(FMath::VInterpTo(Current, Destination, DeltaTime, 5));
}

void ALoongCombatDragon::StopCompanion()
{
    SetActorTickEnabled(false);
    if (IsValid(Attack))
    {
        Attack->SetComponentTickEnabled(false);
        Attack->Director.Reset();
        Attack->OnShot.Clear();
    }
    if (Director.IsValid() && !Director->IsActorBeingDestroyed())
        Director->UnregisterCompanion(this);
    Director.Reset();
}

void ALoongCombatDragon::Destroyed()
{
    StopCompanion();
    Super::Destroyed();
}

void ALoongCombatDragon::EndPlay(const EEndPlayReason::Type Reason)
{
    StopCompanion();
    Super::EndPlay(Reason);
}

ALoongCombatDirector::ALoongCombatDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ALoongCombatDirector::StartCombat(APawn* InPlayer)
{
    if (State != ELoongCombatState::Preparing || !IsValid(InPlayer))
        return false;
    if (InPlayer->FindComponentByClass<ULoongAutoAttackComponent>())
        return false;
    if (!Definition)
        Definition = NewObject<ULoongCombatDefinition>(this);
    Player = InPlayer;
    Random.Initialize(Definition->RandomSeed);
    PlayerHealth = NewObject<ULoongCombatHealthComponent>(Player);
    Player->AddInstanceComponent(PlayerHealth);
    PlayerHealth->RegisterComponent();
    PlayerHealth->ResetHealth(Definition->HeroStats.MaxHealth);
    PlayerHealth->OnDeath.AddDynamic(this, &ALoongCombatDirector::PlayerDied);
    PlayerHealth->NativeDeath.AddUObject(this, &ALoongCombatDirector::PlayerDied);
    PlayerAttack = NewObject<ULoongAutoAttackComponent>(Player);
    Player->AddInstanceComponent(PlayerAttack);
    PlayerAttack->Director = this;
    PlayerAttack->bHeroAttack = true;
    PlayerAttack->RegisterComponent();
    if (auto* Character = Cast<ACharacter>(Player))
    {
        OriginalMoveSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
        bHasOriginalMoveSpeed = true;
        Character->GetCharacterMovement()->MaxWalkSpeed = Definition->HeroStats.MoveSpeed;
        if (Definition->CharacterMesh)
            Character->GetMesh()->SetSkeletalMesh(Definition->CharacterMesh);
    }
    State = ELoongCombatState::Fighting;
    for (const auto& Ability : Definition->Dragons)
        EquipDragon(Ability);
    if (Definition->bEquipElementTestPresets)
        for (const auto& Ability : Definition->ElementDragonPresets)
            EquipDragon(Ability);
    RecalculateStats();
    for (int32 i = 0; i < FMath::Clamp(Definition->InitialEnemies, 0, Definition->MaxEnemies); ++i)
    {
        float A = Random.FRandRange(0, 2 * PI);
        SpawnEnemyAt(GetActorLocation() + FVector(FMath::Cos(A) * 900, FMath::Sin(A) * 900, 100));
    }
    auto* PC = Cast<APlayerController>(Player->GetController());
    InstallCompanionInput(PC);
    if (PC && PC->GetLocalPlayer())
    {
        PC->bShowMouseCursor = true;
        Widget = CreateWidget<ULoongCombatWidget>(PC);
        if (Widget)
        {
            Widget->Setup(this);
            Widget->AddToViewport(100);
        }
    }
    return true;
}

FLoongCombatStats ALoongCombatDirector::GetEffectiveStats() const
{
    FLoongCombatStats Stats = Definition ? Definition->HeroStats : FLoongCombatStats();
    float Bonus = 0;
    float DamageBonus = 0;
    float SpeedBonus = 0;
    for (const auto& Pair : EquippedDragons)
        Bonus += Pair.Value.AttackBonusPercent;
    if (Definition)
        for (const auto& U : Definition->Upgrades)
        {
            int32 Rank = UpgradeLevels.FindRef(U.UpgradeId);
            if (U.Effect == ELoongUpgradeEffect::AttackDamage)
                DamageBonus += Rank * U.Magnitude;
            else if (U.Effect == ELoongUpgradeEffect::AttackSpeed)
                SpeedBonus += Rank * U.Magnitude;
        }
    Stats.AttackDamage *= FMath::Max(0.f, 1 + Bonus + DamageBonus);
    Stats.AttackInterval = FMath::Max(0.05f, Stats.AttackInterval / FMath::Max(0.1f, 1 + SpeedBonus));
    return Stats;
}

void ALoongCombatDirector::RecalculateStats()
{
    if (IsValid(PlayerAttack))
        PlayerAttack->Stats = GetEffectiveStats();
}

void ALoongCombatDirector::EquipDragon(FLoongDragonAbility Ability)
{
    if (Ability.DragonId.IsNone() || !IsFighting() || !IsValid(Player) || Player->IsActorBeingDestroyed() ||
        !IsValid(PlayerHealth) || !PlayerHealth->IsAlive() || Player->GetActorLocation().ContainsNaN())
        return;
    if (Ability.FollowOffset.ContainsNaN())
        Ability.FollowOffset = FLoongDragonAbility().FollowOffset;
    if (!FMath::IsFinite(Ability.FollowRecoveryDistance) || Ability.FollowRecoveryDistance < 100.f)
        Ability.FollowRecoveryDistance = 2000.f;
    if (IsMounted() && GetSelectedCompanion() &&
        GetSelectedCompanion()->Definition.DragonId == Ability.DragonId && !Dismount())
        return;
    RemoveDragon(Ability.DragonId);
    EquippedDragons.Add(Ability.DragonId, Ability);
    RecalculateStats();
    if (Ability.bSpawnCompanion)
    {
        auto* Dragon = GetWorld()->SpawnActor<ALoongCombatDragon>(
            Player->GetActorLocation() + Ability.FollowOffset, FRotator::ZeroRotator);
        if (Dragon)
        {
            Dragon->Director = this;
            Dragon->Configure(Ability);
            Companions.Add(Dragon);
            if (!GetSelectedCompanion() || (GetSelectedCompanion()->Definition.ElementAbilities.IsEmpty() &&
                                            !Ability.ElementAbilities.IsEmpty()))
                SelectedCompanion = Dragon;
        }
    }
}

void ALoongCombatDirector::RemoveDragon(FName Id)
{
    if (GetSelectedCompanion() && GetSelectedCompanion()->Definition.DragonId == Id)
    {
        Dismount(true);
        SelectedCompanion.Reset();
    }
    EquippedDragons.Remove(Id);
    ElementTriggerReadyAt.Remove(Id);
    // Unregister before destroying: EndPlay/Destroyed must not re-enter the removal iteration.
    const auto PreviousCompanions = Companions;
    Companions.RemoveAll(
        [Id](const auto& Dragon)
        {
            return !IsValid(Dragon) || Dragon->Definition.DragonId == Id;
        });
    const auto Targets = Enemies;
    for (const auto& Enemy : Targets)
        if (IsValid(Enemy) && IsValid(Enemy->Elements))
            Enemy->Elements->RemoveSource(Id);
    for (const auto& Dragon : PreviousCompanions)
        if (IsValid(Dragon) && Dragon->Definition.DragonId == Id)
        {
            CancelCompanionProjectiles(Dragon);
            Dragon->Destroy();
        }
    RecalculateStats();
}

bool ALoongCombatDirector::IsActiveCompanion(const ALoongCombatDragon* Dragon) const
{
    return IsValid(Dragon) && !Dragon->IsActorBeingDestroyed() && Dragon->Director.Get() == this &&
           Companions.Contains(Dragon) && EquippedDragons.Contains(Dragon->Definition.DragonId);
}

void ALoongCombatDirector::CancelCompanionProjectiles(ALoongCombatDragon* Dragon)
{
    for (int32 i = Projectiles.Num() - 1; i >= 0; --i)
        if (IsValid(Projectiles[i]) && Projectiles[i]->Attacker.GetEvenIfUnreachable() == Dragon)
        {
            Projectiles[i]->bConsumed = true;
            Projectiles[i]->Destroy();
            Projectiles.RemoveAt(i);
        }
}

void ALoongCombatDirector::UnregisterCompanion(ALoongCombatDragon* Dragon)
{
    if (!Dragon || Companions.Remove(Dragon) == 0)
        return;
    if (SelectedCompanion.Get() == Dragon)
    {
        Dismount(true);
        SelectedCompanion.Reset();
    }
    RemoveDragon(Dragon->Definition.DragonId);
    CancelCompanionProjectiles(Dragon);
}

const FLoongEnemyDefinition* ULoongCombatDefinition::FindEnemy(FName Id) const
{
    if (Id == Enemy.EnemyId)
        return &Enemy;
    return EnemyDefinitions.FindByPredicate(
        [Id](const FLoongEnemyDefinition& E)
        {
            return E.EnemyId == Id;
        });
}

const FLoongEnemySpawnPhase* ULoongCombatDefinition::FindSpawnPhase(float Seconds) const
{
    const FLoongEnemySpawnPhase* Best = nullptr;
    for (const auto& Phase : SpawnPhases)
        if (FMath::IsFinite(Phase.StartSeconds) && Phase.StartSeconds <= Seconds &&
            (!Best || Phase.StartSeconds > Best->StartSeconds))
            Best = &Phase;
    return Best;
}

const FLoongEnemyDefinition* ULoongCombatDefinition::SelectSpawnEnemy(float Seconds,
                                                                      FRandomStream& Stream) const
{
    const auto* Phase = FindSpawnPhase(Seconds);
    if (!Phase)
        return &Enemy;
    float Total = 0;
    for (const auto& W : Phase->Enemies)
        if (FMath::IsFinite(W.Weight) && W.Weight > 0 && FindEnemy(W.EnemyId))
            Total += W.Weight;
    if (Total <= 0)
        return &Enemy;
    float Roll = Stream.FRand() * Total;
    const FLoongEnemyDefinition* Last = &Enemy;
    for (const auto& W : Phase->Enemies)
        if (FMath::IsFinite(W.Weight) && W.Weight > 0)
            if (const auto* E = FindEnemy(W.EnemyId))
            {
                Last = E;
                Roll -= W.Weight;
                if (Roll <= 0)
                    return E;
            }
    return Last;
}

ALoongCombatEnemy* ALoongCombatDirector::SpawnEnemyAt(FVector Position)
{
    if (!Definition)
        return nullptr;
    const auto* E = Definition->SelectSpawnEnemy(Elapsed, Random);
    return E ? SpawnEnemyOfType(E->EnemyId, Position) : nullptr;
}

ALoongCombatEnemy* ALoongCombatDirector::SpawnEnemyOfType(FName EnemyId, FVector Position)
{
    Enemies.RemoveAll(
        [](const auto& E)
        {
            return !IsValid(E) || E->IsActorBeingDestroyed();
        });
    if (!IsFighting() || !Definition || Enemies.Num() >= FMath::Clamp(Definition->MaxEnemies, 0, 200))
        return nullptr;
    const auto* Spec = Definition->FindEnemy(EnemyId);
    if (!Spec || Spec->MaxHealth <= 0 || !FMath::IsFinite(Spec->MaxHealth) ||
        !FMath::IsFinite(Spec->MoveSpeed))
        return nullptr;
    auto* Enemy = GetWorld()->SpawnActor<ALoongCombatEnemy>(Position, FRotator::ZeroRotator);
    if (Enemy)
    {
        Enemy->Director = this;
        Enemy->Configure(*Spec);
        Enemies.Add(Enemy);
    }
    return Enemy;
}

void ALoongCombatDirector::FireEnemyProjectile(ALoongCombatEnemy* Enemy)
{
    Projectiles.RemoveAll(
        [](const auto& P)
        {
            return !IsValid(P) || P->IsActorBeingDestroyed();
        });
    if (!IsFighting() || !IsValid(Enemy) || !Enemy->Health->IsAlive() || !Player ||
        Projectiles.Num() >= Definition->MaxProjectiles)
        return;
    auto* Shot =
        GetWorld()->SpawnActor<ALoongCombatProjectile>(Enemy->GetActorLocation(), FRotator::ZeroRotator);
    if (!Shot)
        return;
    Shot->Director = this;
    Shot->Attacker = Enemy;
    Shot->bEnemyProjectile = true;
    Shot->Damage = Enemy->Definition.ContactDamage;
    Shot->Speed = FMath::Max(1.f, Enemy->Definition.ProjectileSpeed);
    Shot->FlightDirection =
        (GetCombatAvatar()->GetActorLocation() - Shot->GetActorLocation()).GetSafeNormal();
    Shot->SetActorRotation(Shot->FlightDirection.Rotation());
    if (Enemy->Definition.ProjectileMesh)
        Shot->Mesh->SetStaticMesh(Enemy->Definition.ProjectileMesh);
    if (!Enemy->Definition.AttackEffect.IsNull())
        if (auto* FX = Enemy->Definition.AttackEffect.LoadSynchronous())
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), FX, Enemy->GetActorLocation());
    Projectiles.Add(Shot);
}

float ALoongCombatDirector::GetElementDamageMultiplier() const
{
    float M = 1;
    if (!Definition)
        return M;
    for (const auto& U : Definition->Upgrades)
        if (U.Effect == ELoongUpgradeEffect::ElementDamage)
            M += UpgradeLevels.FindRef(U.UpgradeId) * U.Magnitude;
    return FMath::Max(0.f, M);
}

void ALoongCombatDirector::ApplyDragonElements(ALoongCombatEnemy* Enemy, AActor* Source)
{
    ++ElementHitAttempts;
    if (!IsFighting() || !IsValid(Enemy) || !IsValid(Enemy->Health) || !Enemy->Health->IsAlive() ||
        !IsValid(Enemy->Elements))
    {
        LastElementDiagnostic = TEXT("no valid surviving target / combat paused");
        return;
    }
    bool HasSkills = false;
    const bool Test = LoongElementDebug::TestMode();
    const auto* Companion = Cast<ALoongCombatDragon>(Source);
    if (Companion && !IsActiveCompanion(Companion))
    {
        LastElementDiagnostic = TEXT("companion no longer equipped");
        return;
    }
    for (const auto& Dragon : EquippedDragons)
    {
        // Hero hits retain the existing passive loadout. Companion hits use their own modules only.
        if (Companion && Companion->Definition.DragonId != Dragon.Key)
            continue;
        TSet<FName> SeenSkills;
        for (const auto& Skill : Dragon.Value.ElementAbilities)
        {
            HasSkills = true;
            const FString Prefix = Dragon.Key.ToString() + TEXT("/") + Skill.SkillId.ToString();
            if (Skill.SkillId.IsNone() || SeenSkills.Contains(Skill.SkillId) || !Skill.Effect.IsValid() ||
                !FMath::IsFinite(Skill.TriggerChance) || !FMath::IsFinite(Skill.TriggerCooldown))
            {
                LastElementDiagnostic = Prefix + TEXT(": invalid or duplicate skill configuration");
                ElementSkillDiagnostics.Add(Prefix, LastElementDiagnostic);
                continue;
            }
            SeenSkills.Add(Skill.SkillId);
            float& Ready = ElementTriggerReadyAt.FindOrAdd(Dragon.Key).FindOrAdd(Skill.SkillId);
            if (!Test && Elapsed + KINDA_SMALL_NUMBER < Ready)
            {
                LastElementDiagnostic =
                    FString::Printf(TEXT("%s: skill cooldown %.2fs"), *Prefix, Ready - Elapsed);
                ElementSkillDiagnostics.Add(Prefix, LastElementDiagnostic);
                continue;
            }
            if (!Test && Random.FRand() >= FMath::Clamp(Skill.TriggerChance, 0.f, 1.f))
            {
                LastElementDiagnostic = Prefix + TEXT(": probability roll did not trigger");
                ElementSkillDiagnostics.Add(Prefix, LastElementDiagnostic);
                continue;
            }
            auto Spec = Skill.Effect;
            Spec.DamagePerTick *= GetElementDamageMultiplier();
            if (Enemy->Elements->ApplyEffect(Spec, Dragon.Key, Source,
                                             IsValid(Source) ? Source->GetActorLocation()
                                                             : GetActorLocation()))
            {
                ++ElementApplySuccesses;
                LastElementDiagnostic = Prefix + TEXT(": applied to ") + Enemy->GetName();
                Ready = Elapsed + FMath::Max(0.f, Skill.TriggerCooldown);
            }
            else
                LastElementDiagnostic = Prefix + TEXT(": ") + Enemy->Elements->LastApplyDiagnostic;
            ElementSkillDiagnostics.Add(Prefix, LastElementDiagnostic);
        }
    }
    if (!HasSkills)
        LastElementDiagnostic =
            Companion
                ? FString::Printf(TEXT("companion %s: normal attack only (no element modules)"),
                                  *Companion->Definition.DragonId.ToString())
                : TEXT("no equipped element modules; check active Definition and Equip Element Test Presets");
}

void ALoongCombatDirector::FireProjectile(AActor* Source, ALoongCombatEnemy* Enemy, FLoongCombatStats Stats,
                                          bool bHero)
{
    Projectiles.RemoveAll(
        [](const auto& P)
        {
            return !IsValid(P) || P->IsActorBeingDestroyed();
        });
    if (!Definition || Projectiles.Num() >= Definition->MaxProjectiles || !IsFighting() || !IsValid(Source) ||
        Source->IsActorBeingDestroyed() || !IsValid(Enemy) || Enemy->IsActorBeingDestroyed() ||
        !IsValid(Enemy->Health) || !Enemy->Health->IsAlive() || !FMath::IsFinite(Stats.AttackDamage) ||
        Stats.AttackDamage <= 0 || !FMath::IsFinite(Stats.ProjectileSpeed) || Stats.ProjectileSpeed <= 0)
        return;
    auto* Companion = Cast<ALoongCombatDragon>(Source);
    if (Companion && !IsActiveCompanion(Companion))
        return;
    auto* Shot = GetWorld()->SpawnActor<ALoongCombatProjectile>(
        Source->GetActorLocation() + FVector(0, 0, 30), FRotator::ZeroRotator);
    if (!Shot)
        return;
    Shot->Director = this;
    Shot->Target = Enemy;
    Shot->Attacker = Source;
    Shot->Damage = Stats.AttackDamage;
    Shot->Speed = FMath::Max(1.f, Stats.ProjectileSpeed);
    Shot->bHeroAttack = bHero;
    Shot->bCompanionProjectile = Companion != nullptr;
    if (Definition->ProjectileMesh)
        Shot->Mesh->SetStaticMesh(Definition->ProjectileMesh);
    if (Definition->ProjectileMaterial)
        Shot->Mesh->SetMaterial(0, Definition->ProjectileMaterial);
    if (Definition->ProjectileEffect)
        UNiagaraFunctionLibrary::SpawnSystemAttached(Definition->ProjectileEffect, Shot->Mesh, NAME_None,
                                                     FVector::ZeroVector, FRotator::ZeroRotator,
                                                     EAttachLocation::KeepRelativeOffset, true);
    Projectiles.Add(Shot);
}

float ALoongCombatDirector::ApplyHit(ALoongCombatEnemy* Enemy, AActor* Source, float Damage, bool bHero)
{
    if (!IsFighting() || !Definition || !IsValid(Enemy) || !IsValid(Enemy->Health) ||
        !Enemy->Health->IsAlive())
        return 0;
    if (const auto* Companion = Cast<ALoongCombatDragon>(Source))
        if (!IsActiveCompanion(Companion))
            return 0;
    const bool ObservationTest = LoongElementDebug::TestMode();
    float Applied =
        Enemy->Health->ApplyCombatDamage(ObservationTest ? FMath::Min(Damage, 1.f) : Damage, Source);
    if (bHero && !Enemy->Health->IsAlive())
        LastElementDiagnostic =
            TEXT("direct hero hit killed target; element effects only apply to surviving targets");
    if (Definition->HitEffect)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Definition->HitEffect,
                                                       Enemy->GetActorLocation());
    if (Applied > 0 && Enemy->Health->IsAlive())
    {
        if (bHero || (IsValid(Source) && Source->IsA<ALoongCombatDragon>()))
            ApplyDragonElements(Enemy, Source);
    }
    if (bHero && Applied > 0 && Enemy->Health->IsAlive() && IsFighting())
    {
        float Multiplier = 1;
        for (const auto& U : Definition->Upgrades)
            if (U.Effect == ELoongUpgradeEffect::ElementDamage)
                Multiplier += UpgradeLevels.FindRef(U.UpgradeId) * U.Magnitude;
        for (const auto& Pair : EquippedDragons)
            if (!ObservationTest && Random.FRand() < FMath::Clamp(Pair.Value.HitEffectChance, 0.f, 1.f))
            {
                Applied +=
                    Enemy->Health->ApplyCombatDamage(Pair.Value.ExtraElementDamage * Multiplier, Source);
                if (Pair.Value.SkillEffect)
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Pair.Value.SkillEffect,
                                                                   Enemy->GetActorLocation());
                if (!Enemy->Health->IsAlive())
                    break;
            }
    }
    return Applied;
}

void ALoongCombatDirector::EnemyKilled(ALoongCombatEnemy* Enemy)
{
    if (!IsFighting() || !Enemy || !Enemies.Contains(Enemy))
        return;
    ++Kills;
    Enemies.Remove(Enemy);
    AddExperience(FMath::Max(0, Enemy->Definition.ExperienceReward));
}

void ALoongCombatDirector::AddExperience(int32 Amount)
{
    if ((State != ELoongCombatState::Fighting && State != ELoongCombatState::UpgradeSelection) || Amount <= 0)
        return;
    Experience = FMath::Min<int64>(static_cast<int64>(Experience) + Amount, 1000000);
    if (State == ELoongCombatState::Fighting && Experience >= ExperienceThreshold())
    {
        Experience -= ExperienceThreshold();
        ++Level;
        OfferUpgrades();
    }
}

bool ALoongCombatDirector::IsUpgradeEligible(const FLoongUpgradeDefinition& U) const
{
    if (U.UpgradeId.IsNone() || U.Weight <= 0 || U.MaxLevel <= UpgradeLevels.FindRef(U.UpgradeId))
        return false;
    for (const auto& P : U.Prerequisites)
        if (UpgradeLevels.FindRef(P.Key) < P.Value)
            return false;
    return true;
}

void ALoongCombatDirector::OfferUpgrades()
{
    ++UpgradeGeneration;
    UpgradeChoices.Reset();
    TArray<const FLoongUpgradeDefinition*> Pool;
    for (const auto& U : Definition->Upgrades)
        if (IsUpgradeEligible(U))
            Pool.Add(&U);
    for (int32 n = 0; n < 3 && Pool.Num() > 0; ++n)
    {
        float Total = 0;
        for (const auto* U : Pool)
            Total += U->Weight;
        float Roll = Random.FRandRange(0, Total);
        int32 Pick = Pool.Num() - 1;
        for (int32 i = 0; i < Pool.Num(); ++i)
        {
            Roll -= Pool[i]->Weight;
            if (Roll <= 0)
            {
                Pick = i;
                break;
            }
        }
        UpgradeChoices.Add(Pool[Pick]->UpgradeId);
        FName SelectedId = Pool[Pick]->UpgradeId;
        Pool.RemoveAll(
            [SelectedId](const FLoongUpgradeDefinition* U)
            {
                return U->UpgradeId == SelectedId;
            });
    }
    State = UpgradeChoices.Num() > 0 ? ELoongCombatState::UpgradeSelection : ELoongCombatState::Fighting;
    for (const auto& Enemy : Enemies)
        if (IsValid(Enemy) && IsValid(Enemy->Elements))
            Enemy->Elements->SetVisualsPaused(!IsFighting());
    if (auto* C = Cast<ACharacter>(Player))
    {
        C->GetCharacterMovement()->StopMovementImmediately();
        if (State == ELoongCombatState::UpgradeSelection)
            C->GetCharacterMovement()->DisableMovement();
    }
}

bool ALoongCombatDirector::ChooseUpgrade(FName Id, int32 ExpectedGeneration)
{
    if ((ExpectedGeneration >= 0 && ExpectedGeneration != UpgradeGeneration) ||
        State != ELoongCombatState::UpgradeSelection || !UpgradeChoices.Contains(Id))
        return false;
    const auto* U = Definition->Upgrades.FindByPredicate(
        [Id](const FLoongUpgradeDefinition& Other)
        {
            return Other.UpgradeId == Id;
        });
    if (!U || !IsUpgradeEligible(*U))
        return false;
    UpgradeLevels.FindOrAdd(Id)++;
    UpgradeChoices.Reset();
    RecalculateStats();
    State = ELoongCombatState::Fighting;
    for (const auto& Enemy : Enemies)
        if (IsValid(Enemy) && IsValid(Enemy->Elements))
            Enemy->Elements->SetVisualsPaused(false);
    if (auto* C = Cast<ACharacter>(Player))
        if (!IsMounted())
            C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (Experience >= ExperienceThreshold())
    {
        Experience -= ExperienceThreshold();
        ++Level;
        OfferUpgrades();
    }
    return true;
}

void ALoongCombatDirector::PlayerDied()
{
    EndCombat(false);
}

void ALoongCombatDirector::CleanupActors()
{
    Dismount(true);
    for (auto& P : Projectiles)
        if (IsValid(P))
            P->Destroy();
    Projectiles.Reset();
    for (auto& E : Enemies)
        if (IsValid(E))
        {
            E->Elements->ClearEffects();
            E->Destroy();
        }
    Enemies.Reset();
    const auto PreviousCompanions = Companions;
    Companions.Reset();
    for (auto& D : PreviousCompanions)
        if (IsValid(D))
            D->Destroy();
}

void ALoongCombatDirector::EndCombat(bool bVictory)
{
    if (State != ELoongCombatState::Fighting && State != ELoongCombatState::UpgradeSelection)
        return;
    State = bVictory ? ELoongCombatState::Victory : ELoongCombatState::Defeat;
    Result.bVictory = bVictory;
    Result.Kills = Kills;
    Result.Level = Level;
    Result.FightingSeconds = Elapsed;
    Result.TestReward = FMath::Max(0, Kills * 2 + (bVictory ? 100 : 0));
    ++SettlementCount;
    CleanupActors();
    UpgradeLevels.Reset();
    EquippedDragons.Reset();
    ElementTriggerReadyAt.Reset();
    RecalculateStats();
    State = ELoongCombatState::Results;
    OnSettled.Broadcast(Result);
    if (IsValid(PlayerAttack))
        PlayerAttack->SetComponentTickEnabled(false);
    if (auto* C = Cast<ACharacter>(Player); IsValid(C) && !C->IsActorBeingDestroyed())
    {
        C->GetCharacterMovement()->StopMovementImmediately();
        C->GetCharacterMovement()->DisableMovement();
        if (bHasOriginalMoveSpeed)
            C->GetCharacterMovement()->MaxWalkSpeed = OriginalMoveSpeed;
    }
}

void ALoongCombatDirector::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateElementDebug(DeltaTime);
    if (!IsValid(Player) || Player->IsActorBeingDestroyed() || !IsValid(PlayerHealth))
    {
        if (State == ELoongCombatState::Fighting || State == ELoongCombatState::UpgradeSelection)
            EndCombat(false);
        return;
    }
    if ((State == ELoongCombatState::Fighting || State == ELoongCombatState::UpgradeSelection) &&
        !PlayerHealth->IsAlive())
    {
        EndCombat(false);
        return;
    }
    if (IsMounted() && (!RiderController.IsValid() || RiderController->IsActorBeingDestroyed() ||
                        RiderController->GetPawn() != MountPawn || !GetSelectedCompanion()))
        Dismount(true);
    if (!IsFighting())
    {
        if (auto* C = Cast<ACharacter>(Player))
        {
            C->GetCharacterMovement()->StopMovementImmediately();
            C->ConsumeMovementInputVector();
        }
        return;
    }
    Elapsed += DeltaTime;
    if (Elapsed >= FMath::Max(1.f, Definition->DurationSeconds))
    {
        EndCombat(true);
        return;
    }
    Enemies.RemoveAll(
        [](const auto& E)
        {
            return !IsValid(E) || E->IsActorBeingDestroyed();
        });
    Projectiles.RemoveAll(
        [](const auto& P)
        {
            return !IsValid(P) || P->IsActorBeingDestroyed();
        });
    const auto* Phase = Definition->FindSpawnPhase(Elapsed);
    ActiveSpawnPhase = Phase ? static_cast<int32>(Phase - Definition->SpawnPhases.GetData()) : INDEX_NONE;
    SpawnCountdown -= DeltaTime;
    if (SpawnCountdown <= 0)
    {
        SpawnCountdown = Phase ? FMath::Max(0.1f, Definition->SpawnInterval * Phase->IntervalMultiplier)
                               : FMath::Max(0.35f, Definition->SpawnInterval / (1 + Elapsed / 60));
        const int32 Batch =
            Phase ? FMath::Clamp(Phase->BatchSize, 1, 30) : 1 + static_cast<int32>(Elapsed / 60);
        for (int32 n = 0; n < Batch; ++n)
        {
            float A = Random.FRandRange(0, 2 * PI);
            FVector P = GetCombatAvatar()->GetActorLocation() +
                        FVector(FMath::Cos(A) * 1300, FMath::Sin(A) * 1300, 0);
            P.X = FMath::Clamp(P.X, GetActorLocation().X - Definition->ArenaHalfWidth,
                               GetActorLocation().X + Definition->ArenaHalfWidth);
            P.Y = FMath::Clamp(P.Y, GetActorLocation().Y - Definition->ArenaHalfWidth,
                               GetActorLocation().Y + Definition->ArenaHalfWidth);
            P.Z = 100;
            SpawnEnemyAt(P);
        }
    }
}

void ALoongCombatDirector::ReturnToBase()
{
    if (State == ELoongCombatState::Results && !BaseMap.IsEmpty())
        UGameplayStatics::OpenLevel(this, FName(*BaseMap));
}

void ALoongCombatDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    CleanupActors();
    if (IsValid(Widget))
        Widget->RemoveFromParent();
    if (IsValid(PlayerHealth))
        PlayerHealth->NativeDeath.RemoveAll(this);
    if (IsValid(PlayerHealth))
        PlayerHealth->OnDeath.RemoveDynamic(this, &ALoongCombatDirector::PlayerDied);
    if (IsValid(PlayerAttack))
        PlayerAttack->DestroyComponent();
    if (IsValid(PlayerHealth))
        PlayerHealth->DestroyComponent();
    Super::EndPlay(Reason);
}

ALoongCombatGameMode::ALoongCombatGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.AddUnique(TEXT("LoongCombatMode"));
}

void ALoongCombatGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (Combat)
        return;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn())
        return;
    for (TActorIterator<ALoongCombatDirector> It(GetWorld()); It; ++It)
    {
        Combat = *It;
        break;
    }
    if (!Combat)
        Combat = GetWorld()->SpawnActor<ALoongCombatDirector>();
    if (!Combat)
        return;
    if (CombatDefinition)
        Combat->Definition = CombatDefinition;
    Combat->StartCombat(PC->GetPawn());
    bool bCamera = false;
    for (TActorIterator<ALoongArtTestCamera> It(GetWorld()); It; ++It)
    {
        bCamera = true;
        break;
    }
    if (!bCamera)
        GetWorld()->SpawnActor<ALoongArtTestCamera>();
}

FString ALoongCombatDirector::GetElementDebugSummary() const
{
    FString Text = FString::Printf(
        TEXT(
            "ELEMENT DEBUG test=%d state=%d\nDefinition=%s presets=%d equipped=%d elementHits=%d applied=%d\n"),
        LoongElementDebug::TestMode(), static_cast<int32>(State), *GetPathNameSafe(Definition),
        Definition ? Definition->bEquipElementTestPresets : false, EquippedDragons.Num(), ElementHitAttempts,
        ElementApplySuccesses);
    for (const auto& Dragon : EquippedDragons)
    {
        Text += FString::Printf(TEXT("%s : %d modules\n"), *Dragon.Key.ToString(),
                                Dragon.Value.ElementAbilities.Num());
        for (const auto& Skill : Dragon.Value.ElementAbilities)
        {
            const FString Key = Dragon.Key.ToString() + TEXT("/") + Skill.SkillId.ToString();
            const auto* Cooldowns = ElementTriggerReadyAt.Find(Dragon.Key);
            const float Remaining =
                Cooldowns ? FMath::Max(0.f, Cooldowns->FindRef(Skill.SkillId) - Elapsed) : 0.f;
            const FString* Reason = ElementSkillDiagnostics.Find(Key);
            Text += FString::Printf(TEXT("  %s chance=%.0f%% cooldown=%.2fs : %s\n"),
                                    *Skill.SkillId.ToString(), Skill.TriggerChance * 100.f, Remaining,
                                    Reason ? **Reason : TEXT("no eligible hero/companion hit yet"));
            Text += FString::Printf(TEXT("    visual=%s fallback=%d\n"),
                                    Skill.Effect.VisualEffect.IsNull()
                                        ? TEXT("None")
                                        : *Skill.Effect.VisualEffect.ToSoftObjectPath().ToString(),
                                    Skill.Effect.bUseFallbackVisual);
        }
    }
    Text += LastElementDiagnostic;
    return Text;
}

void ALoongCombatDirector::UpdateElementDebug(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
    if (!LoongElementDebug::Enabled())
        return;
    ElementDebugCountdown -= DeltaTime;
    if (ElementDebugCountdown > 0.f)
        return;
    const float Interval = LoongElementDebug::DrawInterval();
    ElementDebugCountdown = Interval;
    // Detailed configuration lives in the scrollable test panel, not over the combat HUD.
    if (GEngine)
        GEngine->RemoveOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()));
    int32 Drawn = 0;
    auto Targets = Enemies;
    const FVector Focus = IsValid(Player) ? Player->GetActorLocation() : GetActorLocation();
    Targets.RemoveAll(
        [](const auto& Enemy)
        {
            return !IsValid(Enemy) || !IsValid(Enemy->Health) || !Enemy->Health->IsAlive() ||
                   !IsValid(Enemy->Elements) || Enemy->Elements->GetEffectCount() == 0;
        });
    Targets.Sort(
        [Focus](const ALoongCombatEnemy& A, const ALoongCombatEnemy& B)
        {
            return FVector::DistSquared2D(A.GetActorLocation(), Focus) <
                   FVector::DistSquared2D(B.GetActorLocation(), Focus);
        });
    for (const auto& Enemy : Targets)
    {
        if (Drawn >= LoongElementDebug::MaxTargets())
            break;
        ++Drawn;
        Enemy->Elements->DrawDebugState(Interval + 0.05f);
    }
#endif
}
