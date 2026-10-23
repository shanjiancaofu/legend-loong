#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LoongMountPawn.generated.h"
class ALoongCombatDirector;
/** Temporary collision/control host; the original dragon owns visuals and abilities. */
UCLASS()
class LOONG_API ALoongMountPawn : public ACharacter
{
    GENERATED_BODY()
public:
    ALoongMountPawn();
    TWeakObjectPtr<ALoongCombatDirector> Director;
    float DashRemaining = 0.f;
    float DashCooldown = 0.f;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Destroyed() override;
    void MoveForward(float Value);
    void MoveBackward(float Value);
    void MoveRight(float Value);
    void MoveLeft(float Value);
    void AttackPressed();
    void AttackReleased();
    void ToggleRide();
    void StartDash();
    void SkillOne();
    void SkillTwo();
    void SkillThree();
    FVector GetAimPoint() const;

private:
    uint8 MovementKeys = 0;
    bool bAttackHeld = false;
    FVector DashDirection = FVector::ForwardVector;
    void MoveAxis(float Forward, float Right);
};
