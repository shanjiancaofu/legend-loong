#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LoongCombatTypes.generated.h"
class USkeletalMesh;
class UStaticMesh;
class UNiagaraSystem;
class UMaterialInterface;
class UTexture2D;
UENUM(BlueprintType)
enum class ELoongCombatState : uint8
{
    Preparing,
    Fighting,
    UpgradeSelection,
    Victory,
    Defeat,
    Results
};
UENUM(BlueprintType)
enum class ELoongUpgradeEffect : uint8
{
    AttackSpeed,
    AttackDamage,
    ElementDamage
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongCombatStats
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealth = 150;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackDamage = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackInterval = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackRange = 1100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileSpeed = 1600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MoveSpeed = 500;
};
UENUM(BlueprintType)
enum class ELoongEnemyAttackPattern : uint8
{
    Melee,
    Ranged
};
UENUM(BlueprintType)
enum class ELoongElementKind : uint8
{
    Fire,
    Ice,
    Wind
};
UENUM(BlueprintType)
enum class ELoongElementRefresh : uint8
{
    RefreshDuration,
    KeepDuration,
    ExtendDuration
};

USTRUCT(BlueprintType)
struct LOONG_API FLoongElementEffect
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EffectId = TEXT("Burn");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELoongElementKind Element = ELoongElementKind::Fire;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
    float Duration = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05"))
    float TickInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxStacks = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float ApplicationCooldown = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELoongElementRefresh RefreshRule = ELoongElementRefresh::RefreshDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float DamagePerTick = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "0.9"))
    float SlowPerStack = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
    int32 FreezeAtStacks = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float FreezeDuration = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float Radius = 250.f;
    /** Positive pushes away from the source, negative pulls toward it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DisplacementPerTick = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UNiagaraSystem> VisualEffect;

    /** Visible prototype feedback when no Niagara component can be spawned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseFallbackVisual = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector VisualOffset = FVector(0, 0, 30);

    /** World scale, independent of the target mesh/root scale. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector VisualScale = FVector::OneVector;
    bool IsValid() const;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongElementAbility
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SkillId = TEXT("FireOnHit");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriggerChance = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriggerCooldown = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLoongElementEffect Effect;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongEnemyDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EnemyId = TEXT("MeleeTest");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PlaceholderTint = FLinearColor(0.7f, 0.12f, 0.08f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector VisualScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELoongEnemyAttackPattern AttackPattern = ELoongEnemyAttackPattern::Melee;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PreferredDistance = 650.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RangedAttackRange = 1000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProjectileSpeed = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackInterval = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UStaticMesh> ProjectileMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UNiagaraSystem> AttackEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UNiagaraSystem> DeathEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealth = 45;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MoveSpeed = 130;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ContactDamage = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ContactInterval = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ContactRange = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExperienceReward = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UMaterialInterface> Material;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongDragonAbility
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DragonId = TEXT("WindTest");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FLoongElementAbility> ElementAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackBonusPercent = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HitEffectChance = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExtraElementDamage = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSpawnCompanion = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CompanionDamage = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CompanionInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CompanionRange = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector FollowOffset = FVector(-160, 100, 150);

    /** Recover to FollowOffset when separation from that destination exceeds this distance (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
    float FollowRecoveryDistance = 2000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UMaterialInterface> Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UNiagaraSystem> SkillEffect;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongUpgradeDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName UpgradeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELoongUpgradeEffect Effect = ELoongUpgradeEffect::AttackDamage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Rarity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxLevel = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Weight = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Magnitude = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, int32> Prerequisites;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UTexture2D> Icon;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongCombatResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly)
    bool bVictory = false;

    UPROPERTY(BlueprintReadOnly)
    int32 Kills = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly)
    int32 TestReward = 0;

    UPROPERTY(BlueprintReadOnly)
    float FightingSeconds = 0;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongEnemySpawnWeight
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EnemyId = TEXT("MeleeTest");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PlaceholderTint = FLinearColor(0.7f, 0.12f, 0.08f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector VisualScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float Weight = 1.f;
};
USTRUCT(BlueprintType)
struct LOONG_API FLoongEnemySpawnPhase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float StartSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
    float IntervalMultiplier = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "30"))
    int32 BatchSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FLoongEnemySpawnWeight> Enemies;
};
UCLASS(BlueprintType)
class LOONG_API ULoongCombatDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    ULoongCombatDefinition();

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLoongCombatStats HeroStats;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FLoongEnemyDefinition Enemy;
    /** MeleeTest resolves to the legacy Enemy above; additional IDs live here. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FLoongEnemyDefinition> EnemyDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FLoongEnemySpawnPhase> SpawnPhases;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "500"))
    int32 MaxProjectiles = 120;
    const FLoongEnemyDefinition* FindEnemy(FName Id) const;
    const FLoongEnemySpawnPhase* FindSpawnPhase(float Seconds) const;
    const FLoongEnemyDefinition* SelectSpawnEnemy(float Seconds, FRandomStream& Random) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FLoongDragonAbility> Dragons;
    /** Optional test loadouts; not automatically equipped into old demo assets. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FLoongDragonAbility> ElementDragonPresets;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bEquipElementTestPresets = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FLoongUpgradeDefinition> Upgrades;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float DurationSeconds = 180;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float SpawnInterval = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxEnemies = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 InitialEnemies = 12;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ArenaHalfWidth = 2600;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RandomSeed = 20261012;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<USkeletalMesh> CharacterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> ProjectileMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UMaterialInterface> ProjectileMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UNiagaraSystem> HitEffect;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UNiagaraSystem> ProjectileEffect;
};
