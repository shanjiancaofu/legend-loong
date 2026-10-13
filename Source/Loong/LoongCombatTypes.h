#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LoongCombatTypes.generated.h"
class USkeletalMesh; class UStaticMesh; class UNiagaraSystem; class UMaterialInterface; class UTexture2D;
UENUM(BlueprintType) enum class ELoongCombatState : uint8 { Preparing, Fighting, UpgradeSelection, Victory, Defeat, Results };
UENUM(BlueprintType) enum class ELoongUpgradeEffect : uint8 { AttackSpeed, AttackDamage, ElementDamage };
USTRUCT(BlueprintType) struct LOONG_API FLoongCombatStats
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth=150;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackDamage=25;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackInterval=0.6f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackRange=1100;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProjectileSpeed=1600;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeed=500;
};
USTRUCT(BlueprintType) struct LOONG_API FLoongEnemyDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EnemyId=TEXT("MeleeTest");
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth=45;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeed=130;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float ContactDamage=5;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float ContactInterval=1.2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float ContactRange=100;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ExperienceReward=8;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> SkeletalMesh;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UMaterialInterface> Material;
};
USTRUCT(BlueprintType) struct LOONG_API FLoongDragonAbility
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DragonId=TEXT("WindTest");
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackBonusPercent=0.1f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float HitEffectChance=0.25f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExtraElementDamage=12;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSpawnCompanion=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float CompanionDamage=15;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float CompanionInterval=1.0f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float CompanionRange=1000;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector FollowOffset=FVector(-160,100,150);
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> SkeletalMesh;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UMaterialInterface> Material;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNiagaraSystem> SkillEffect;
};
USTRUCT(BlueprintType) struct LOONG_API FLoongUpgradeDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName UpgradeId;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) ELoongUpgradeEffect Effect=ELoongUpgradeEffect::AttackDamage;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Rarity=1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxLevel=10;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight=1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude=0.2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName,int32> Prerequisites;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UTexture2D> Icon;
};
USTRUCT(BlueprintType) struct LOONG_API FLoongCombatResult
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bVictory=false;
 UPROPERTY(BlueprintReadOnly) int32 Kills=0;
 UPROPERTY(BlueprintReadOnly) int32 Level=1;
 UPROPERTY(BlueprintReadOnly) int32 TestReward=0;
 UPROPERTY(BlueprintReadOnly) float FightingSeconds=0;
};
UCLASS(BlueprintType) class LOONG_API ULoongCombatDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 ULoongCombatDefinition();
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FLoongCombatStats HeroStats;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FLoongEnemyDefinition Enemy;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FLoongDragonAbility> Dragons;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FLoongUpgradeDefinition> Upgrades;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float DurationSeconds=180;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpawnInterval=2;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxEnemies=30;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 InitialEnemies=12;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float ArenaHalfWidth=2600;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RandomSeed=20261012;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMesh> CharacterMesh;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMesh> ProjectileMesh;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UMaterialInterface> ProjectileMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> HitEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> ProjectileEffect;
};
