#include "LoongMountPawn.h"
#include "LoongCombatDemo.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

ALoongMountPawn::ALoongMountPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(55.f, 88.f);
    GetCharacterMovement()->MaxWalkSpeed = 650.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 720, 0);
    bUseControllerRotationYaw = false;
}
void ALoongMountPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->bBlockInput = true;
    Input->Priority = 200;
    auto BindMovement = [this, Input](FKey Key, uint8 Mask)
    {
        FInputKeyBinding Press(FInputChord(Key), IE_Pressed);
        Press.KeyDelegate.GetDelegateForManualSet().BindLambda(
            [this, Mask]()
            {
                MovementKeys |= Mask;
            });
        Input->KeyBindings.Add(Press);
        FInputKeyBinding Release(FInputChord(Key), IE_Released);
        Release.KeyDelegate.GetDelegateForManualSet().BindLambda(
            [this, Mask]()
            {
                MovementKeys &= ~Mask;
            });
        Input->KeyBindings.Add(Release);
    };
    BindMovement(EKeys::W, 1);
    BindMovement(EKeys::S, 2);
    BindMovement(EKeys::D, 4);
    BindMovement(EKeys::A, 8);
    Input->BindKey(EKeys::F, IE_Pressed, this, &ALoongMountPawn::ToggleRide);
    Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALoongMountPawn::AttackPressed);
    Input->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ALoongMountPawn::AttackReleased);
    Input->BindKey(EKeys::LeftShift, IE_Pressed, this, &ALoongMountPawn::StartDash);
    Input->BindKey(EKeys::One, IE_Pressed, this, &ALoongMountPawn::SkillOne);
    Input->BindKey(EKeys::Two, IE_Pressed, this, &ALoongMountPawn::SkillTwo);
    Input->BindKey(EKeys::Three, IE_Pressed, this, &ALoongMountPawn::SkillThree);
}
void ALoongMountPawn::MoveAxis(float Forward, float Right)
{
    if (!Director.IsValid() || !Director->IsFighting() || !Director->IsMounted())
        return;
    auto* PC = Cast<APlayerController>(Controller);
    const float Yaw =
        PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : -45.f;
    AddMovementInput(FRotator(0, Yaw, 0).RotateVector(FVector(Forward, Right, 0)));
}
void ALoongMountPawn::MoveForward(float Value)
{
    MoveAxis(Value, 0);
}
void ALoongMountPawn::MoveBackward(float Value)
{
    MoveAxis(-Value, 0);
}
void ALoongMountPawn::MoveRight(float Value)
{
    MoveAxis(0, Value);
}
void ALoongMountPawn::MoveLeft(float Value)
{
    MoveAxis(0, -Value);
}
void ALoongMountPawn::AttackPressed()
{
    bAttackHeld = true;
}
void ALoongMountPawn::AttackReleased()
{
    bAttackHeld = false;
}
void ALoongMountPawn::ToggleRide()
{
    if (Director.IsValid())
        Director->ToggleMount();
}
void ALoongMountPawn::SkillOne()
{
    if (Director.IsValid())
        Director->CommandCompanionSkill(0);
}
void ALoongMountPawn::SkillTwo()
{
    if (Director.IsValid())
        Director->CommandCompanionSkill(1);
}
void ALoongMountPawn::SkillThree()
{
    if (Director.IsValid())
        Director->CommandCompanionSkill(2);
}
FVector ALoongMountPawn::GetAimPoint() const
{
    const auto* PC = Cast<APlayerController>(Controller);
    FVector Origin, Direction;
    if (PC && PC->DeprojectMousePositionToWorld(Origin, Direction) && FMath::Abs(Direction.Z) > 0.001f)
    {
        const float Distance = (GetActorLocation().Z - Origin.Z) / Direction.Z;
        if (Distance > 0)
            return Origin + Direction * Distance;
    }
    return GetActorLocation() + GetActorForwardVector() * 600.f;
}
void ALoongMountPawn::StartDash()
{
    if (!Director.IsValid() || !Director->IsFighting() || DashCooldown > 0)
        return;
    DashDirection = GetVelocity().GetSafeNormal2D();
    if (DashDirection.IsNearlyZero())
        DashDirection = GetActorForwardVector();
    DashRemaining = 0.2f;
    DashCooldown = 1.5f;
    const float VerticalSpeed = GetCharacterMovement()->Velocity.Z;
    GetCharacterMovement()->Velocity = DashDirection * 1400.f + FVector(0, 0, VerticalSpeed);
}
void ALoongMountPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsActorBeingDestroyed() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
        return;
    if (!Director.IsValid() || !Director->IsFighting())
    {
        bAttackHeld = false;
        MovementKeys = 0;
        ConsumeMovementInputVector();
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        if (Director.IsValid())
            if (auto* Dragon = Director->GetSelectedCompanion())
                Dragon->Attack->bAttackRequested = false;
        return;
    }
    if (GetCharacterMovement()->MovementMode == MOVE_None)
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        // Release state may be flushed when focus is lost or a UI captures the mouse.
        if (!PC->IsInputKeyDown(EKeys::W))
            MovementKeys &= ~1;
        if (!PC->IsInputKeyDown(EKeys::S))
            MovementKeys &= ~2;
        if (!PC->IsInputKeyDown(EKeys::D))
            MovementKeys &= ~4;
        if (!PC->IsInputKeyDown(EKeys::A))
            MovementKeys &= ~8;
        if (!PC->IsInputKeyDown(EKeys::LeftMouseButton))
            bAttackHeld = false;
    }
    MoveAxis(((MovementKeys & 1) ? 1.f : 0.f) - ((MovementKeys & 2) ? 1.f : 0.f),
             ((MovementKeys & 4) ? 1.f : 0.f) - ((MovementKeys & 8) ? 1.f : 0.f));
    DashCooldown = FMath::Max(0.f, DashCooldown - DeltaSeconds);
    DashRemaining = FMath::Max(0.f, DashRemaining - DeltaSeconds);
    GetCharacterMovement()->MaxWalkSpeed = DashRemaining > 0 ? 1400.f : 650.f;
    if (DashRemaining > 0)
        AddMovementInput(DashDirection);
    if (Director->Definition)
    {
        const FVector Center = Director->GetActorLocation();
        const float Radius = FMath::Max(100.f, Director->Definition->ArenaHalfWidth - 55.f);
        FVector Position = GetActorLocation();
        Position.X = FMath::Clamp(Position.X, Center.X - Radius, Center.X + Radius);
        Position.Y = FMath::Clamp(Position.Y, Center.Y - Radius, Center.Y + Radius);
        if (!Position.Equals(GetActorLocation()))
            SetActorLocation(Position);
    }
    if (bAttackHeld)
        Director->MountedBasicAttack();
}
void ALoongMountPawn::Destroyed()
{
    if (Director.IsValid())
        Director->MountPawnEnded(this);
    Super::Destroyed();
}
void ALoongMountPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Director.IsValid())
        Director->MountPawnEnded(this);
    Super::EndPlay(Reason);
}
