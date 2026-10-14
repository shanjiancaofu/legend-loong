#include "LoongElementComponent.h"
#include "LoongCombatDemo.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "LoongElementDebug.h"
#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"

bool FLoongElementEffect::IsValid() const
{
    return (Element == ELoongElementKind::Fire || Element == ELoongElementKind::Ice ||
            Element == ELoongElementKind::Wind) &&
           !EffectId.IsNone() && FMath::IsFinite(Duration) && Duration > 0 && FMath::IsFinite(TickInterval) &&
           TickInterval >= 0.05f && MaxStacks >= 1 && MaxStacks <= 20 &&
           FMath::IsFinite(ApplicationCooldown) && ApplicationCooldown >= 0 &&
           FMath::IsFinite(DamagePerTick) && DamagePerTick >= 0 && FMath::IsFinite(SlowPerStack) &&
           SlowPerStack >= 0 && SlowPerStack <= 0.9f && FreezeAtStacks >= 1 &&
           FMath::IsFinite(FreezeDuration) && FreezeDuration >= 0 && FMath::IsFinite(Radius) && Radius >= 0 &&
           FMath::IsFinite(DisplacementPerTick) && !VisualOffset.ContainsNaN() &&
           !VisualScale.ContainsNaN() && VisualScale.X > 0 && VisualScale.Y > 0 && VisualScale.Z > 0;
}

ULoongElementComponent::ULoongElementComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    FallbackSphere = Sphere.Object;
    FallbackCone = Cone.Object;
    FallbackMaterial = Material.Object;
}

void ULoongElementComponent::CreateVisual(FLoongActiveElement& E)
{
    auto* Owner = GetOwner();
    auto* Root = IsValid(Owner) ? Owner->GetRootComponent() : nullptr;
    if (!IsValid(Root))
    {
        E.VisualDiagnostic = TEXT("no target root component");
        return;
    }
    if (!E.Spec.VisualEffect.IsNull())
    {
        if (auto* System = E.Spec.VisualEffect.LoadSynchronous())
        {
            // Set user parameters before activation; do not inherit scaled mesh roots.
            E.Visual = UNiagaraFunctionLibrary::SpawnSystemAttached(
                System, Root, NAME_None, E.Spec.VisualOffset, FRotator::ZeroRotator,
                EAttachLocation::KeepRelativeOffset, false, false, ENCPoolMethod::None, false);
            if (IsValid(E.Visual))
            {
                E.Visual->SetAbsolute(false, false, true);
                E.Visual->SetWorldScale3D(E.Spec.VisualScale);
                UpdateVisual(E);
                E.Visual->Activate(true);
                E.VisualDiagnostic = TEXT("Niagara: ") + System->GetPathName();
                return;
            }
        }
        E.VisualDiagnostic = TEXT("Niagara load/spawn failed; ");
    }
    if (!E.Spec.bUseFallbackVisual || !FallbackMaterial)
    {
        E.VisualDiagnostic += TEXT("fallback disabled/unavailable");
        return;
    }
    auto* Shape = E.Spec.Element == ELoongElementKind::Ice ? FallbackCone.Get() : FallbackSphere.Get();
    if (!Shape)
    {
        E.VisualDiagnostic += TEXT("fallback mesh unavailable");
        return;
    }
    E.FallbackVisual = NewObject<UInstancedStaticMeshComponent>(Owner);
    Owner->AddInstanceComponent(E.FallbackVisual);
    E.FallbackVisual->SetupAttachment(Root);
    E.FallbackVisual->SetMobility(EComponentMobility::Movable);
    E.FallbackVisual->SetAbsolute(false, false, true);
    E.FallbackVisual->SetRelativeLocation(E.Spec.VisualOffset);
    E.FallbackVisual->SetWorldScale3D(E.Spec.VisualScale);
    E.FallbackVisual->SetStaticMesh(Shape);
    E.FallbackVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    E.FallbackVisual->SetGenerateOverlapEvents(false);
    E.FallbackVisual->SetCanEverAffectNavigation(false);
    E.FallbackVisual->SetCastShadow(false);
    auto* Tint = UMaterialInstanceDynamic::Create(FallbackMaterial, E.FallbackVisual);
    const FLinearColor Color = E.Spec.Element == ELoongElementKind::Fire  ? FLinearColor(1.f, 0.12f, 0.01f)
                               : E.Spec.Element == ELoongElementKind::Ice ? FLinearColor(0.05f, 0.65f, 1.f)
                                                                          : FLinearColor(0.15f, 1.f, 0.3f);
    Tint->SetVectorParameterValue(TEXT("Color"), Color);
    E.FallbackVisual->SetMaterial(0, Tint);
    E.FallbackVisual->RegisterComponent();
    for (int32 i = 0; i < 4; ++i)
        E.FallbackVisual->AddInstance(FTransform::Identity);
    UpdateVisual(E);
    E.VisualDiagnostic += TEXT("fallback: prototype mesh");
}

void ULoongElementComponent::UpdateVisual(FLoongActiveElement& E)
{
    if (IsValid(E.Visual))
    {
        E.Visual->SetVariableFloat(TEXT("User.Duration"), E.Remaining);
        E.Visual->SetVariableFloat(TEXT("User.Radius"), E.Spec.Radius);
        E.Visual->SetVariableInt(TEXT("User.Stacks"), E.Stacks);
        E.Visual->SetVariableBool(TEXT("User.Frozen"), E.FrozenRemaining > 0);
    }
    if (!IsValid(E.FallbackVisual))
        return;
    for (int32 i = 0; i < E.FallbackVisual->GetInstanceCount(); ++i)
    {
        const float Angle =
            i * PI * 0.5f + (E.Spec.Element == ELoongElementKind::Ice ? 0.f : E.VisualTime * 3.f);
        float Radius = 45.f;
        float Height = 0.f;
        FVector Scale(0.18f);
        if (E.Spec.Element == ELoongElementKind::Fire)
        {
            Height = 15.f + FMath::Fmod(E.VisualTime * 65.f + i * 22.f, 85.f);
            Scale = FVector(0.18f, 0.18f, 0.35f + E.Stacks * 0.08f);
        }
        else if (E.Spec.Element == ELoongElementKind::Ice)
        {
            Scale = E.FrozenRemaining > 0 ? FVector(0.42f, 0.42f, 1.2f) : FVector(0.25f, 0.25f, 0.55f);
        }
        else
        {
            // Sparse orbit markers reveal the damage radius without covering enemy projectiles.
            Radius = E.Spec.Radius;
            Height = -65.f;
            Scale = FVector(0.35f, 0.15f, 0.15f);
        }
        const FVector Position(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Height);
        E.FallbackVisual->UpdateInstanceTransform(
            i, FTransform(FRotator(0, FMath::RadiansToDegrees(Angle), 0), Position, Scale), false,
            i == E.FallbackVisual->GetInstanceCount() - 1, true);
    }
}

void ULoongElementComponent::SetVisualsPaused(bool bPaused)
{
    for (const auto& E : Active)
        if (IsValid(E.Visual))
            E.Visual->SetPaused(bPaused);
}

bool ULoongElementComponent::ApplyEffect(FLoongElementEffect Spec, FName SourceId, AActor* Instigator,
                                         FVector SourcePosition)
{
    auto* Health = GetOwner() ? GetOwner()->FindComponentByClass<ULoongCombatHealthComponent>() : nullptr;
    if (!Director.IsValid() || !Director->IsFighting())
    {
        LastApplyDiagnostic = TEXT("combat not Fighting (upgrade pause / ended)");
        return false;
    }
    if (!Health || !Health->IsAlive())
    {
        LastApplyDiagnostic = TEXT("target dead or missing health");
        return false;
    }
    if (SourcePosition.ContainsNaN() || !Spec.IsValid() || SourceId.IsNone())
    {
        LastApplyDiagnostic = TEXT("invalid effect parameters or source ID");
        return false;
    }
    auto* Entry = Active.FindByPredicate(
        [&](const FLoongActiveElement& E)
        {
            return E.Spec.EffectId == Spec.EffectId && E.SourceId == SourceId;
        });
    if (Entry)
    {
        if (Entry->Spec.Element != Spec.Element)
        {
            LastApplyDiagnostic = TEXT("source/effect ID conflicts with another element");
            return false;
        }
        if (Entry->SinceApplication + KINDA_SMALL_NUMBER < Entry->Spec.ApplicationCooldown)
        {
            LastApplyDiagnostic = FString::Printf(TEXT("application cooldown %.2fs"),
                                                  Entry->Spec.ApplicationCooldown - Entry->SinceApplication);
            return false;
        }
        Entry->Stacks = FMath::Min(Entry->Stacks + 1, Entry->Spec.MaxStacks);
        Entry->SinceApplication = 0;
        if (Entry->Spec.RefreshRule == ELoongElementRefresh::RefreshDuration)
            Entry->Remaining = Entry->Spec.Duration;
        else if (Entry->Spec.RefreshRule == ELoongElementRefresh::ExtendDuration)
            Entry->Remaining = FMath::Min(Entry->Remaining + Entry->Spec.Duration,
                                          Entry->Spec.Duration * Entry->Spec.MaxStacks);
        Entry->Instigator = Instigator;
        Entry->SourcePosition = SourcePosition;
    }
    else
    {
        FLoongActiveElement E;
        E.Spec = Spec;
        E.SourceId = SourceId;
        E.Instigator = Instigator;
        E.SourcePosition = SourcePosition;
        E.Remaining = Spec.Duration;
        E.UntilTick = Spec.TickInterval;
        CreateVisual(E);
        Active.Add(E);
        Entry = &Active.Last();
    }
    if (Entry->Spec.Element == ELoongElementKind::Ice)
    {
        ++Entry->FreezeProgress;
        if (Entry->FreezeProgress >= Entry->Spec.FreezeAtStacks && Entry->FrozenRemaining <= 0)
        {
            Entry->FrozenRemaining = Entry->Spec.FreezeDuration;
            Entry->FreezeProgress = 0;
        }
    }
    UpdateVisual(*Entry);
    ++SuccessfulApplications;
    LastApplyDiagnostic =
        FString::Printf(TEXT("applied %s from %s"), *Spec.EffectId.ToString(), *SourceId.ToString());
    OnElementChanged.Broadcast(Spec.EffectId, Entry->Stacks);
    return true;
}

void ULoongElementComponent::RemoveAt(int32 Index)
{
    FName Id = Active[Index].Spec.EffectId;
    if (IsValid(Active[Index].Visual))
        Active[Index].Visual->DestroyComponent();
    if (IsValid(Active[Index].FallbackVisual))
        Active[Index].FallbackVisual->DestroyComponent();
    Active.RemoveAt(Index);
    OnElementChanged.Broadcast(Id, 0);
}

void ULoongElementComponent::RemoveSource(FName SourceId)
{
    for (int32 i = Active.Num() - 1; i >= 0; --i)
        if (Active[i].SourceId == SourceId)
            RemoveAt(i);
}

void ULoongElementComponent::RemoveEffect(FName EffectId)
{
    for (int32 i = Active.Num() - 1; i >= 0; --i)
        if (Active[i].Spec.EffectId == EffectId)
            RemoveAt(i);
}

void ULoongElementComponent::ClearEffects()
{
    while (Active.Num())
        RemoveAt(Active.Num() - 1);
}

int32 ULoongElementComponent::GetStacks(FName EffectId, FName SourceId) const
{
    const auto* E = Active.FindByPredicate(
        [&](const FLoongActiveElement& Entry)
        {
            return Entry.SourceId == SourceId && Entry.Spec.EffectId == EffectId;
        });
    return E ? E->Stacks : 0;
}

bool ULoongElementComponent::IsFrozen() const
{
    for (const auto& E : Active)
        if (E.FrozenRemaining > 0)
            return true;
    return false;
}

float ULoongElementComponent::GetMovementMultiplier() const
{
    if (IsFrozen())
        return 0;
    float Slow = 0;
    for (const auto& E : Active)
        if (E.Spec.Element == ELoongElementKind::Ice)
            Slow = FMath::Max(Slow, FMath::Clamp(E.Spec.SlowPerStack * E.Stacks, 0.f, 0.9f));
    return 1.f - Slow;
}

void ULoongElementComponent::ApplyPeriodic(const FLoongActiveElement& E)
{
    if (E.Spec.Element == ELoongElementKind::Ice)
        return;
    if (E.Spec.Element == ELoongElementKind::Fire)
    {
        ++PeriodicApplications;
        LastActualDamage = 0.f;
        LastWindHits = 0;
        LastActualDisplacement = FVector::ZeroVector;
        if (auto* H = GetOwner()->FindComponentByClass<ULoongCombatHealthComponent>())
            LastActualDamage = H->ApplyCombatDamage(E.Spec.DamagePerTick * E.Stacks, E.Instigator.Get());
        return;
    }
    if (!Director.IsValid())
        return;
    // Wind acts around the marked target; snapshot allows deaths to unregister safely.
    ++PeriodicApplications;
    LastWindHits = 0;
    LastWindMoves.Reset();
    LastActualDamage = 0.f;
    LastActualDisplacement = FVector::ZeroVector;
    const FVector Center = GetOwner()->GetActorLocation();
    const auto Targets = Director->Enemies;
    for (const auto& T : Targets)
    {
        if (!Director->IsFighting())
            break;
        if (IsValid(T) && !T->IsActorBeingDestroyed() && IsValid(T->Health) && T->Health->IsAlive() &&
            FVector::DistSquared2D(Center, T->GetActorLocation()) <= FMath::Square(E.Spec.Radius))
        {
            ++LastWindHits;
            LastActualDamage +=
                T->Health->ApplyCombatDamage(E.Spec.DamagePerTick * E.Stacks, E.Instigator.Get());
            if (T->Health->IsAlive())
            {
                FVector Away = T->GetActorLocation() - E.SourcePosition;
                Away.Z = 0;
                const float Displacement = E.Spec.DisplacementPerTick < 0
                                               ? -FMath::Min(-E.Spec.DisplacementPerTick, Away.Size2D())
                                               : E.Spec.DisplacementPerTick;
                FVector P = T->GetActorLocation() + Away.GetSafeNormal() * Displacement;
                if (Director->Definition)
                {
                    const auto O = Director->GetActorLocation();
                    const float R = Director->Definition->ArenaHalfWidth;
                    P.X = FMath::Clamp(P.X, O.X - R, O.X + R);
                    P.Y = FMath::Clamp(P.Y, O.Y - R, O.Y + R);
                }
                const FVector Previous = T->GetActorLocation();
                T->SetActorLocation(P);
                LastActualDisplacement += P - Previous;
                if (LastWindMoves.Num() < 6)
                    LastWindMoves.Emplace(Previous, P);
            }
        }
    }
}

void ULoongElementComponent::AdvanceCombatTime(float DeltaSeconds)
{
    if (!Director.IsValid() || !Director->IsFighting() || DeltaSeconds <= 0 || !FMath::IsFinite(DeltaSeconds))
        return;
    auto* H = GetOwner()->FindComponentByClass<ULoongCombatHealthComponent>();
    if (!H || !H->IsAlive())
    {
        ClearEffects();
        return;
    }
    // Iterate immutable copies: periodic damage can synchronously kill this actor and clear Active.
    const auto Snapshot = Active;
    for (const auto& Copy : Snapshot)
    {
        int32 Index = Active.IndexOfByPredicate(
            [&](const FLoongActiveElement& E)
            {
                return E.SourceId == Copy.SourceId && E.Spec.EffectId == Copy.Spec.EffectId;
            });
        if (Index == INDEX_NONE)
            continue;
        auto& E = Active[Index];
        const float Step = FMath::Min(DeltaSeconds, E.Remaining);
        E.Remaining -= Step;
        E.SinceApplication += DeltaSeconds;
        E.FrozenRemaining = FMath::Max(0.f, E.FrozenRemaining - DeltaSeconds);
        E.VisualTime += Step;
        UpdateVisual(E);
        E.UntilTick -= Step;
        const int32 Ticks = FMath::Min(
            64,
            FMath::Max(0, FMath::FloorToInt((-E.UntilTick + KINDA_SMALL_NUMBER) / E.Spec.TickInterval) + 1));
        if (E.UntilTick <= KINDA_SMALL_NUMBER)
        {
            E.UntilTick += Ticks * E.Spec.TickInterval;
            for (int32 t = 0; t < Ticks; ++t)
            {
                ApplyPeriodic(Copy);
                const int32 CurrentIndex = Active.IndexOfByPredicate(
                    [&](const FLoongActiveElement& Current)
                    {
                        return Current.SourceId == Copy.SourceId &&
                               Current.Spec.EffectId == Copy.Spec.EffectId;
                    });
                if (CurrentIndex != INDEX_NONE && Copy.Spec.Element != ELoongElementKind::Ice)
                {
                    auto& Current = Active[CurrentIndex];
                    ++Current.PeriodicTicks;
                    Current.LastTickDamage = LastActualDamage;
                    Current.LastTickHits = LastWindHits;
                    Current.LastTickDisplacement = LastActualDisplacement.Size2D();
                }
                if (!H->IsAlive() || !Director->IsFighting())
                    break;
            }
        }
        if (!H->IsAlive())
        {
            ClearEffects();
            return;
        }
        Index = Active.IndexOfByPredicate(
            [&](const FLoongActiveElement& Current)
            {
                return Current.SourceId == Copy.SourceId && Current.Spec.EffectId == Copy.Spec.EffectId;
            });
        if (Index != INDEX_NONE && Active[Index].Remaining <= KINDA_SMALL_NUMBER)
            RemoveAt(Index);
    }
}

void ULoongElementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearEffects();
    Super::EndPlay(Reason);
}

FString ULoongElementComponent::BuildDebugText() const
{
    FString Text;
    for (const auto& E : Active)
    {
        const TCHAR* Kind = E.Spec.Element == ELoongElementKind::Fire  ? TEXT("FIRE")
                            : E.Spec.Element == ELoongElementKind::Ice ? TEXT("ICE")
                                                                       : TEXT("WIND");
        Text += FString::Printf(TEXT("%s %s x%d remaining=%.2fs tick=%.2fs damage/tick=%.1f\n"), Kind,
                                *E.SourceId.ToString(), E.Stacks, E.Remaining, E.UntilTick,
                                E.Spec.DamagePerTick * E.Stacks);
        Text += FString::Printf(TEXT("  actual last tick=%.1f ticks=%d area hits=%d moved=%.1fcm\n"),
                                E.LastTickDamage, E.PeriodicTicks, E.LastTickHits, E.LastTickDisplacement);
        Text += TEXT("  visual=") + E.VisualDiagnostic + TEXT("\n");
    }
    if (Active.IsEmpty())
        Text = TEXT("NO ACTIVE ELEMENT\n");
    const auto* Enemy = Cast<ALoongCombatEnemy>(GetOwner());
    Text += FString::Printf(
        TEXT(
            "slow=%.0f%% frozen=%d actual speed=%.1f cm/s\nlast periodic damage=%.1f wind hits=%d displacement=%.1fcm ticks=%d\n%s"),
        (1.f - GetMovementMultiplier()) * 100.f, IsFrozen(), Enemy ? Enemy->LastMeasuredMoveSpeed : 0.f,
        LastActualDamage, LastWindHits, LastActualDisplacement.Size2D(), PeriodicApplications,
        *LastApplyDiagnostic);
    return Text;
}

void ULoongElementComponent::DrawDebugState(float Lifetime) const
{
#if !UE_BUILD_SHIPPING
    if (!LoongElementDebug::Enabled() || !GetOwner() || Active.IsEmpty())
        return;
    const FVector Position = GetOwner()->GetActorLocation();
    FColor Color = IsFrozen() ? FColor::Cyan : FColor::White;
    FString Label;
    for (const auto& E : Active)
    {
        if (!Label.IsEmpty())
            Label += TEXT(" | ");
        const TCHAR* Kind = E.Spec.Element == ELoongElementKind::Fire  ? TEXT("FIRE")
                            : E.Spec.Element == ELoongElementKind::Ice ? TEXT("ICE")
                                                                       : TEXT("WIND");
        Label += FString::Printf(TEXT("%s x%d"), Kind, E.Stacks);
        if (E.Spec.Element == ELoongElementKind::Wind)
            DrawDebugCircle(GetWorld(), Position - FVector(0, 0, 40), E.Spec.Radius, 32, Color, false,
                            Lifetime, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
    }
    for (const auto& Move : LastWindMoves)
    {
        DrawDebugLine(GetWorld(), Move.Key, Move.Value, FColor::Green, false, Lifetime, 0, 2.f);
        DrawDebugPoint(GetWorld(), Move.Value, 12.f, FColor::Green, false, Lifetime);
    }
    Label += FString::Printf(TEXT("\nslow %.0f%%  frozen %d"), (1.f - GetMovementMultiplier()) * 100.f,
                             IsFrozen());
    // DrawDebugString appends entries even for the same actor. Replace one attached label instead.
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (auto* PC = It->Get())
            if (auto* HUD = PC->GetHUD())
                HUD->AddDebugText(Label, GetOwner(), Lifetime, FVector(0, 0, 110), FVector(0, 0, 110), Color,
                                  false, false, true, nullptr, 0.85f, true);
#endif
}
