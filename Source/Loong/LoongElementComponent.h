#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LoongCombatTypes.h"
#include "LoongElementComponent.generated.h"
class ALoongCombatDirector;
class ULoongCombatHealthComponent;
class UNiagaraComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLoongElementChanged, FName, EffectId, int32, Stacks);
USTRUCT()
struct FLoongActiveElement
{
    GENERATED_BODY()
    UPROPERTY()
    FLoongElementEffect Spec;
    FName SourceId;
    TWeakObjectPtr<AActor> Instigator;
    FVector SourcePosition = FVector::ZeroVector;
    float Remaining = 0;
    float UntilTick = 0;
    float SinceApplication = 0;
    float FrozenRemaining = 0;
    int32 Stacks = 1;
    int32 FreezeProgress = 0;
    int32 PeriodicTicks = 0;
    float LastTickDamage = 0.f;
    int32 LastTickHits = 0;
    float LastTickDisplacement = 0.f;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> Visual;
};
/** Updates in combat time only. Uses existing health and registered targets. */
UCLASS(ClassGroup = (Loong), meta = (BlueprintSpawnableComponent))
class LOONG_API ULoongElementComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULoongElementComponent();
    TWeakObjectPtr<ALoongCombatDirector> Director;

    UPROPERTY(BlueprintAssignable)
    FLoongElementChanged OnElementChanged;

    UFUNCTION(BlueprintCallable)
    bool ApplyEffect(FLoongElementEffect Effect, FName SourceId, AActor* Instigator, FVector SourcePosition);

    UFUNCTION(BlueprintCallable)
    void RemoveSource(FName SourceId);

    UFUNCTION(BlueprintCallable)
    void RemoveEffect(FName EffectId);

    UFUNCTION(BlueprintCallable)
    void ClearEffects();

    UFUNCTION(BlueprintPure)
    float GetMovementMultiplier() const;

    UFUNCTION(BlueprintPure)
    bool IsFrozen() const;

    UFUNCTION(BlueprintPure)
    int32 GetStacks(FName EffectId, FName SourceId) const;

    UFUNCTION(BlueprintPure)
    int32 GetEffectCount() const
    {
        return Active.Num();
    }

    void AdvanceCombatTime(float DeltaSeconds);
    FString BuildDebugText() const;
    FString LastApplyDiagnostic;
    int32 SuccessfulApplications = 0;
    int32 PeriodicApplications = 0;
    float LastActualDamage = 0.f;
    FVector LastActualDisplacement = FVector::ZeroVector;
    int32 LastWindHits = 0;
    TArray<TPair<FVector, FVector>> LastWindMoves;
    void DrawDebugState(float Lifetime) const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY()
    TArray<FLoongActiveElement> Active;
    void ApplyPeriodic(const FLoongActiveElement& Effect);
    void RemoveAt(int32 Index);
};
