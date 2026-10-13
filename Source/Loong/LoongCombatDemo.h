#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "LoongCombatTypes.h"
#include "LoongCombatArtTest.h"
#include "LoongElementComponent.h"
#include "LoongCombatDemo.generated.h"
class ULoongCombatWidget;
class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UCharacterMovementComponent;
class ALoongCombatDirector;
class ALoongCombatEnemy;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLoongCombatDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLoongCombatDamaged, float, Damage, AActor*, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoongCombatSettled, FLoongCombatResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLoongCombatShot, FVector, Origin, FVector, Target);
UCLASS(ClassGroup = (Loong), meta = (BlueprintSpawnableComponent))
class LOONG_API ULoongCombatHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly)
    float MaxHealth = 100;

    UPROPERTY(BlueprintReadOnly)
    float CurrentHealth = 100;

    UPROPERTY(BlueprintAssignable)
    FLoongCombatDeath OnDeath;
    FSimpleMulticastDelegate NativeDeath;

    UPROPERTY(BlueprintAssignable)
    FLoongCombatDamaged OnDamaged;

    UFUNCTION(BlueprintCallable)
    void ResetHealth(float InMaxHealth);

    UFUNCTION(BlueprintCallable)
    float ApplyCombatDamage(float Damage, AActor* Source);

    UFUNCTION(BlueprintPure)
    bool IsAlive() const
    {
        return CurrentHealth > 0;
    }
};
UCLASS(ClassGroup = (Loong), meta = (BlueprintSpawnableComponent))
class LOONG_API ULoongAutoAttackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULoongAutoAttackComponent();

    UPROPERTY(BlueprintAssignable)
    FLoongCombatShot OnShot;

    UPROPERTY(BlueprintReadOnly)
    FLoongCombatStats Stats;
    TWeakObjectPtr<ALoongCombatDirector> Director;
    bool bHeroAttack = false;
    float Cooldown = 0;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* Fn) override;
    ALoongCombatEnemy* FindTarget() const;
};
UCLASS()
class LOONG_API ALoongCombatEnemy : public AActor
{
    GENERATED_BODY()
public:
    ALoongCombatEnemy();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<ULoongCombatHealthComponent> Health;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> AnimatedMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<ULoongElementComponent> Elements;

    UPROPERTY(BlueprintReadOnly)
    FLoongEnemyDefinition Definition;
    TWeakObjectPtr<ALoongCombatDirector> Director;
    float ContactCooldown = 0;
    float RangedCooldown = 0;
    float LastMeasuredMoveSpeed = 0.f;
    bool bDeathReported = false;
    void Configure(const FLoongEnemyDefinition& InDefinition);
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void Died();
};
UCLASS()
class LOONG_API ALoongCombatProjectile : public AActor
{
    GENERATED_BODY()
public:
    ALoongCombatProjectile();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;
    TWeakObjectPtr<ALoongCombatDirector> Director;
    TWeakObjectPtr<ALoongCombatEnemy> Target;
    TWeakObjectPtr<AActor> Attacker;
    float Damage = 0;
    float Speed = 1600;
    float LifeSeconds = 0;
    bool bHeroAttack = false;
    bool bConsumed = false;
    bool bEnemyProjectile = false;
    FVector FlightDirection = FVector::ForwardVector;
    virtual void Tick(float DeltaTime) override;
    void ResolveHit();
};
UCLASS()
class LOONG_API ALoongCombatDragon : public AActor
{
    GENERATED_BODY()
public:
    ALoongCombatDragon();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> AnimatedMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<ULoongAutoAttackComponent> Attack;
    TWeakObjectPtr<ALoongCombatDirector> Director;
    FLoongDragonAbility Definition;
    void Configure(const FLoongDragonAbility& Ability);
    virtual void Tick(float DeltaTime) override;
};
UCLASS()
class LOONG_API ALoongCombatDirector : public AActor
{
    GENERATED_BODY()
public:
    ALoongCombatDirector();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<ULoongCombatDefinition> Definition;

    UPROPERTY(BlueprintReadOnly)
    ELoongCombatState State = ELoongCombatState::Preparing;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<APawn> Player;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<ULoongCombatHealthComponent> PlayerHealth;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<ULoongAutoAttackComponent> PlayerAttack;

    UPROPERTY(BlueprintReadOnly)
    float Elapsed = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Kills = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly)
    int32 Experience = 0;

    UPROPERTY(BlueprintReadOnly)
    FLoongCombatResult Result;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> UpgradeChoices;

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, int32> UpgradeLevels;

    UPROPERTY(BlueprintAssignable)
    FLoongCombatSettled OnSettled;

    UPROPERTY()
    TArray<TObjectPtr<ALoongCombatEnemy>> Enemies;

    UPROPERTY()
    TArray<TObjectPtr<ALoongCombatDragon>> Companions;

    UPROPERTY()
    TArray<TObjectPtr<ALoongCombatProjectile>> Projectiles;

    UFUNCTION(BlueprintCallable)
    bool StartCombat(APawn* InPlayer);

    UFUNCTION(BlueprintCallable)
    bool ChooseUpgrade(FName Id, int32 ExpectedGeneration = -1);

    UFUNCTION(BlueprintCallable)
    void EquipDragon(FLoongDragonAbility Ability);

    UFUNCTION(BlueprintCallable)
    void RemoveDragon(FName Id);

    UFUNCTION(BlueprintCallable)
    void EndCombat(bool bVictory);

    UFUNCTION(BlueprintCallable)
    void ReturnToBase();

    UFUNCTION(BlueprintPure)
    int32 ExperienceThreshold() const
    {
        return 15 + Level * 10;
    }

    UFUNCTION(BlueprintPure)
    bool IsFighting() const
    {
        return State == ELoongCombatState::Fighting;
    }

    UFUNCTION(BlueprintPure)
    FLoongCombatStats GetEffectiveStats() const;
    bool IsUpgradeEligible(const FLoongUpgradeDefinition& Upgrade) const;
    ALoongCombatEnemy* SpawnEnemyAt(FVector Position);
    ALoongCombatEnemy* SpawnEnemyOfType(FName EnemyId, FVector Position);
    void FireEnemyProjectile(ALoongCombatEnemy* Enemy);
    void ApplyDragonElements(ALoongCombatEnemy* Enemy, AActor* Source);
    float GetElementDamageMultiplier() const;
    FString GetElementDebugSummary() const;
    int32 GetEquippedDragonCount() const
    {
        return EquippedDragons.Num();
    }
    void UpdateElementDebug(float DeltaTime);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 ActiveSpawnPhase = INDEX_NONE;
    void EnemyKilled(ALoongCombatEnemy* Enemy);
    void AddExperience(int32 Amount);
    void FireProjectile(AActor* Attacker, ALoongCombatEnemy* Target, FLoongCombatStats Stats, bool bHero);
    float ApplyHit(ALoongCombatEnemy* Enemy, AActor* Source, float Damage, bool bHero);

    UFUNCTION()
    void PlayerDied();
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    FString BaseMap = TEXT("/Game/Loong/Maps/L_Base");
    int32 SettlementCount = 0;
    int32 UpgradeGeneration = 0;
    FRandomStream Random;

private:
    UPROPERTY()
    TMap<FName, FLoongDragonAbility> EquippedDragons;
    float SpawnCountdown = 0;
    float ElementDebugCountdown = 0.f;
    int32 ElementHitAttempts = 0;
    int32 ElementApplySuccesses = 0;
    TMap<FString, FString> ElementSkillDiagnostics;
    FString LastElementDiagnostic = TEXT("no hero hit yet");
    TMap<FName, TMap<FName, float>> ElementTriggerReadyAt;
    float OriginalMoveSpeed = 0;
    bool bHasOriginalMoveSpeed = false;

    UPROPERTY()
    TObjectPtr<ULoongCombatWidget> Widget;
    void RecalculateStats();
    void OfferUpgrades();
    void CleanupActors();
};
UCLASS()
class LOONG_API ALoongCombatGameMode : public ALoongCombatArtTestGameMode
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<ULoongCombatDefinition> CombatDefinition;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<ALoongCombatDirector> Combat;
    ALoongCombatGameMode();
    virtual void Tick(float DeltaTime) override;
};
