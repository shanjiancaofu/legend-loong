#include "LoongCombatDemo.h"
#include "LoongMountPawn.h"
#include "LoongCombatWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

bool ALoongCombatDirector::IsMounted() const
{
    return IsValid(MountPawn) && !MountPawn->IsActorBeingDestroyed();
}

ALoongCombatDragon* ALoongCombatDirector::GetSelectedCompanion() const
{
    if (SelectedCompanion.IsValid() && IsActiveCompanion(SelectedCompanion.Get()))
        return SelectedCompanion.Get();
    for (const auto& Dragon : Companions)
        if (IsActiveCompanion(Dragon))
            return Dragon;
    return nullptr;
}

AActor* ALoongCombatDirector::GetCombatAvatar() const
{
    return IsMounted() ? static_cast<AActor*>(MountPawn) : static_cast<AActor*>(Player);
}

void ALoongCombatDirector::InstallCompanionInput(APlayerController* PC)
{
    if (!PC || !PC->IsLocalController())
        return;
    EnableInput(PC);
    InputComponent->Priority = 100;
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ALoongCombatDirector::MountKeyPressed);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ALoongCombatDirector::SkillOnePressed);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ALoongCombatDirector::SkillTwoPressed);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ALoongCombatDirector::SkillThreePressed);
}

void ALoongCombatDirector::MountKeyPressed()
{
    ToggleMount();
}

void ALoongCombatDirector::SkillOnePressed()
{
    CommandCompanionSkill(0);
}
void ALoongCombatDirector::SkillTwoPressed()
{
    CommandCompanionSkill(1);
}
void ALoongCombatDirector::SkillThreePressed()
{
    CommandCompanionSkill(2);
}

bool ALoongCombatDirector::SelectCompanion(FName Id)
{
    if (!IsFighting() || !Definition || !IsValid(Player) || Player->IsActorBeingDestroyed() ||
        !IsValid(PlayerHealth) || !PlayerHealth->IsAlive())
        return false;
    const FLoongDragonAbility* Found = Definition->Dragons.FindByPredicate(
        [Id](const auto& Entry)
        {
            return Entry.DragonId == Id;
        });
    if (!Found)
        Found = Definition->ElementDragonPresets.FindByPredicate(
            [Id](const auto& Entry)
            {
                return Entry.DragonId == Id;
            });
    if (!Found)
    {
        CompanionFeedback = TEXT("没有找到这个伴战配置");
        return false;
    }
    const auto Ability = *Found;
    if (IsMounted() && !Dismount())
        return false;
    const auto Previous = Companions;
    for (const auto& Dragon : Previous)
        if (IsValid(Dragon))
            RemoveDragon(Dragon->Definition.DragonId);
    auto Equipped = Ability;
    // A transient entity must not replace a passive authored preset with the same ID.
    Equipped.DragonId = FName(*(TEXT("ActiveCompanion_") + Id.ToString()));
    Equipped.bSpawnCompanion = true;
    Equipped.AttackBonusPercent = 0.f;
    Equipped.HitEffectChance = 0.f;
    EquipDragon(Equipped);
    for (const auto& Dragon : Companions)
        if (IsActiveCompanion(Dragon) && Dragon->Definition.DragonId == Equipped.DragonId)
        {
            SelectedCompanion = Dragon;
            LastCompanionPreset = Id;
            CompanionFeedback = TEXT("当前伴战：") + Id.ToString();
            return true;
        }
    CompanionFeedback = TEXT("伴战生成失败");
    return false;
}

bool ALoongCombatDirector::UnequipCompanions()
{
    if (!IsFighting())
        return false;
    if (IsMounted() && !Dismount())
        return false;
    if (LastCompanionPreset.IsNone())
        if (auto* Current = GetSelectedCompanion())
            LastCompanionPreset = Current->Definition.DragonId;
    const auto Previous = Companions;
    for (const auto& Dragon : Previous)
        if (IsValid(Dragon))
            RemoveDragon(Dragon->Definition.DragonId);
    SelectedCompanion.Reset();
    CompanionFeedback = TEXT("已卸下实体伴战；独立被动配置保留");
    return true;
}

bool ALoongCombatDirector::RecallCompanion()
{
    if (!IsFighting() || !IsValid(Player) || !IsValid(PlayerHealth) || !PlayerHealth->IsAlive())
    {
        CompanionFeedback = TEXT("当前状态不能召回，请先恢复战斗");
        return false;
    }
    if (IsMounted())
    {
        CompanionFeedback = TEXT("骑乘中无需召回，请先下龙");
        return false;
    }
    auto* Dragon = GetSelectedCompanion();
    const bool bRestoreAbsent = Dragon == nullptr;
    if (!Dragon)
    {
        FName Preset = LastCompanionPreset;
        if (Preset.IsNone() && Definition)
        {
            for (const auto& Entry : Definition->Dragons)
                if (Entry.bSpawnCompanion && !Entry.DragonId.IsNone())
                {
                    Preset = Entry.DragonId;
                    break;
                }
            if (Preset.IsNone())
                for (const auto& Entry : Definition->ElementDragonPresets)
                    if (!Entry.DragonId.IsNone())
                    {
                        Preset = Entry.DragonId;
                        break;
                    }
        }
        if (Preset.IsNone())
        {
            CompanionFeedback = TEXT("没有可召回的伴战配置，请先选择一个测试预设");
            return false;
        }
        if (!SelectCompanion(Preset))
            return false;
        Dragon = GetSelectedCompanion();
    }
    if (!Dragon)
        return false;
    Dragon->SetActorLocation(Player->GetActorLocation() + Dragon->Definition.FollowOffset, false, nullptr,
                             ETeleportType::TeleportPhysics);
    CompanionFeedback = bRestoreAbsent ? TEXT("伴战已恢复并召回") : TEXT("伴战已召回");
    return true;
}

bool ALoongCombatDirector::ToggleMount()
{
    if (!IsFighting())
        return false;
    if (IsMounted())
        return Dismount();
    auto* Dragon = GetSelectedCompanion();
    auto* PC = IsValid(Player) ? Cast<APlayerController>(Player->GetController()) : nullptr;
    if (PC && PC->GetPawn() != Player)
        return false;
    if (!Dragon || !PC || !IsValid(PlayerHealth) || !PlayerHealth->IsAlive() ||
        Player->GetAttachParentActor() != nullptr || Player->IsActorBeingDestroyed())
    {
        CompanionFeedback = TEXT("需要先装备伴战并由玩家控制角色");
        return false;
    }
    if (FVector::DistSquared2D(Player->GetActorLocation(), Dragon->GetActorLocation()) > FMath::Square(550.f))
    {
        CompanionFeedback = TEXT("距离太远，请先召回伴战");
        return false;
    }
    FCollisionQueryParams MountQuery(SCENE_QUERY_STAT(LoongMount), false);
    MountQuery.AddIgnoredActor(Player);
    MountQuery.AddIgnoredActor(Dragon);
    if (GetWorld()->OverlapBlockingTestByChannel(Player->GetActorLocation(), FQuat::Identity, ECC_Pawn,
                                                 FCollisionShape::MakeCapsule(55.f, 88.f), MountQuery))
    {
        CompanionFeedback = TEXT("这里空间不足，请移到空地再上龙");
        return false;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Host = GetWorld()->SpawnActor<ALoongMountPawn>(
        ALoongMountPawn::StaticClass(), Player->GetActorLocation(), Player->GetActorRotation(), Params);
    if (!Host)
        return false;
    // Use the currently occupied safe capsule location; original rider collision is disabled below.
    MountPawn = Host;
    Host->Director = this;
    SelectedCompanion = Dragon;
    RiderController = PC;
    RiderScale = Player->GetActorScale3D();
    bRiderCollision = Player->GetActorEnableCollision();
    bRiderTick = Player->IsActorTickEnabled();
    PC->StopMovement();
    if (auto* Character = Cast<ACharacter>(Player))
    {
        RiderMovementMode = Character->GetCharacterMovement()->MovementMode;
        RiderCustomMovementMode = Character->GetCharacterMovement()->CustomMovementMode;
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->DisableMovement();
    }
    Player->ConsumeMovementInputVector();
    Player->SetActorEnableCollision(false);
    Player->SetActorTickEnabled(false);
    Player->AttachToComponent(Host->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    Player->SetActorRelativeLocation(FVector(0, 0, 140));
    Player->SetActorRelativeRotation(FRotator::ZeroRotator);
    Dragon->AttachToComponent(Host->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    Dragon->SetActorRelativeLocation(FVector::ZeroVector);
    Dragon->SetActorRelativeRotation(FRotator::ZeroRotator);
    Dragon->Attack->bAutomatic = false;
    Dragon->Attack->bAttackRequested = false;
    Dragon->Attack->bUseAimPoint = true;
    if (PlayerAttack)
        PlayerAttack->SetComponentTickEnabled(false);
    PC->Possess(Host);
    // BP_TopDownController binds every possessed Pawn's OnDestroyed to character respawn.
    // This temporary host is not a dead player. Its own lifecycle callback restores the rider.
    // Remove only this controller's binding from the host; retain original rider death behavior.
    Host->OnDestroyed.RemoveAll(PC);
    if (PC->GetPawn() != Host)
    {
        Dismount(true);
        return false;
    }
    PC->bShowMouseCursor = true;
    PC->bAutoManageActiveCameraTarget = false;
    CompanionFeedback = TEXT("骑乘中：WASD 移动，左键攻击，Shift 冲刺，1/2/3 技能，F 下龙");
    return true;
}

bool ALoongCombatDirector::Dismount(bool bForce)
{
    if (!MountPawn || bRestoringRider)
        return false;
    TGuardValue<bool> Guard(bRestoringRider, true);
    auto* Host = MountPawn.Get();
    FVector Landing = Host->GetActorLocation();
    auto* Dragon = SelectedCompanion.Get();
    if (IsValid(Player))
    {
        float Radius = 42.f, HalfHeight = 88.f;
        if (auto* Character = Cast<ACharacter>(Player))
        {
            Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
            HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(LoongDismount), false);
        Query.AddIgnoredActor(Host);
        Query.AddIgnoredActor(Player);
        if (Dragon)
            Query.AddIgnoredActor(Dragon);
        bool Found = false;
        for (int32 i = 0; i < 8; ++i)
        {
            const float Angle = i * PI / 4.f;
            FVector Candidate = Landing + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * 145.f;
            if (Definition &&
                (FMath::Abs(Candidate.X - GetActorLocation().X) > Definition->ArenaHalfWidth - Radius ||
                 FMath::Abs(Candidate.Y - GetActorLocation().Y) > Definition->ArenaHalfWidth - Radius))
                continue;
            FHitResult Floor;
            if (GetWorld()->LineTraceSingleByChannel(Floor, Candidate + FVector(0, 0, 120),
                                                     Candidate - FVector(0, 0, 300), ECC_Visibility, Query))
                Candidate.Z = Floor.ImpactPoint.Z + HalfHeight + 2.f;
            const auto Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
            // The mount capsule is shorter than the blueprint rider. Sweeping the taller rider
            // from the mount's grounded center starts inside the floor, falsely rejecting every exit.
            FVector SweepStart = Landing;
            SweepStart.Z = FMath::Max(SweepStart.Z, Candidate.Z) + 2.f;
            FHitResult Block;
            if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Shape,
                                                          Query) &&
                !GetWorld()->SweepSingleByChannel(Block, SweepStart, Candidate, FQuat::Identity, ECC_Pawn,
                                                  Shape, Query))
            {
                Landing = Candidate;
                Found = true;
                break;
            }
        }
        if (!Found && !bForce)
        {
            CompanionFeedback = TEXT("周围没有安全下龙位置，请移动到空地");
            return false;
        }
    }
    MountPawn = nullptr;
    if (IsValid(Dragon))
    {
        Dragon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Dragon->Attack->bAutomatic = true;
        Dragon->Attack->bUseAimPoint = false;
        Dragon->Attack->bAttackRequested = false;
    }
    if (IsValid(Player))
    {
        Player->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        Player->SetActorLocation(Landing, false, nullptr, ETeleportType::TeleportPhysics);
        Player->SetActorScale3D(RiderScale);
        Player->SetActorEnableCollision(bRiderCollision);
        Player->SetActorTickEnabled(bRiderTick);
        RestoreRider();
    }
    if (IsValid(PlayerAttack))
        PlayerAttack->SetComponentTickEnabled(IsFighting());
    if (IsValid(Host) && !Host->IsActorBeingDestroyed())
    {
        Host->Director.Reset();
        Host->Destroy();
    }
    CompanionFeedback = TEXT("已下龙，恢复并肩作战");
    return true;
}

void ALoongCombatDirector::RestoreRider()
{
    if (!IsValid(Player))
        return;
    if (RiderController.IsValid() && !RiderController->IsActorBeingDestroyed() && !GetWorld()->bIsTearingDown)
    {
        RiderController->Possess(Player);
        RiderController->bShowMouseCursor = true;
        RiderController->bAutoManageActiveCameraTarget = false;
    }
    if (auto* Character = Cast<ACharacter>(Player))
    {
        Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(RiderMovementMode),
                                                           RiderCustomMovementMode);
        if (!IsFighting())
            Character->GetCharacterMovement()->DisableMovement();
    }
    RiderController.Reset();
}

void ALoongCombatDirector::MountPawnEnded(ALoongMountPawn* Pawn)
{
    if (MountPawn == Pawn && !bRestoringRider)
        Dismount(true);
}

void ALoongCombatDirector::MountedBasicAttack()
{
    auto* Dragon = GetSelectedCompanion();
    if (!IsFighting() || !IsMounted() || !Dragon || (Widget && Widget->IsPointerOverControls()))
        return;
    Dragon->Attack->AimPoint = MountPawn->GetAimPoint();
    Dragon->Attack->bAttackRequested = true;
}

bool ALoongCombatDirector::CommandCompanionSkill(int32 Slot)
{
    auto* Dragon = GetSelectedCompanion();
    if (!IsFighting() || !Dragon || !Dragon->Definition.ElementAbilities.IsValidIndex(Slot))
    {
        CompanionFeedback = TEXT("当前伴战没有这个技能槽");
        return false;
    }
    if (IsMounted())
        Dragon->Attack->AimPoint = MountPawn->GetAimPoint();
    auto* Target = Dragon->Attack->FindTarget();
    const auto& Skill = Dragon->Definition.ElementAbilities[Slot];
    if (!Target || !Skill.Effect.IsValid() || Skill.SkillId.IsNone() ||
        !FMath::IsFinite(Skill.TriggerCooldown))
    {
        CompanionFeedback = TEXT("射程内没有有效目标或技能配置无效");
        return false;
    }
    float& Ready = ElementTriggerReadyAt.FindOrAdd(Dragon->Definition.DragonId).FindOrAdd(Skill.SkillId);
    if (Elapsed + KINDA_SMALL_NUMBER < Ready)
    {
        CompanionFeedback = TEXT("技能冷却中");
        return false;
    }
    auto Effect = Skill.Effect;
    Effect.DamagePerTick *= GetElementDamageMultiplier();
    if (!Target->Elements->ApplyEffect(Effect, Dragon->Definition.DragonId, Dragon,
                                       Dragon->GetActorLocation()))
        return false;
    Ready = Elapsed + FMath::Max(0.25f, Skill.TriggerCooldown);
    CompanionFeedback = TEXT("已施放：") + Skill.SkillId.ToString();
    return true;
}

FString ALoongCombatDirector::GetCompanionStatus() const
{
    auto* Dragon = GetSelectedCompanion();
    FString Text =
        Dragon ? TEXT("当前：") + Dragon->Definition.DragonId.ToString() : TEXT("当前：未装备实体伴战");
    Text += IsMounted() ? TEXT(" · 骑乘\nWASD 移动 / 左键攻击 / Shift 冲刺 / F 下龙\n")
                        : TEXT(" · 并肩\nF 上龙（距离 550cm 内）\n");
    if (Dragon)
    {
        Text += FString::Printf(TEXT("普攻冷却 %.1fs\n"), FMath::Max(0.f, Dragon->Attack->Cooldown));
        for (int32 i = 0; i < Dragon->Definition.ElementAbilities.Num(); ++i)
        {
            const auto& Skill = Dragon->Definition.ElementAbilities[i];
            const auto* Cooldowns = ElementTriggerReadyAt.Find(Dragon->Definition.DragonId);
            const float Ready = Cooldowns ? Cooldowns->FindRef(Skill.SkillId) : 0.f;
            Text += FString::Printf(TEXT("%d：%s · %.1fs\n"), i + 1, *Skill.SkillId.ToString(),
                                    FMath::Max(0.f, Ready - Elapsed));
        }
    }
    return Text + CompanionFeedback;
}
