#include "ShadeAwarenessComponent.h"

#include "AbilitySystemComponent.h"
#include "../AbilitySystem/ShadeAwarenessEffects.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeGameplayTags.h"

UShadeAwarenessComponent::UShadeAwarenessComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UShadeAwarenessComponent::Initialize(UAbilitySystemComponent* InAbilitySystem)
{
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsValid(InAbilitySystem) ||
        InAbilitySystem->GetOwnerActor() != GetOwner() ||
        !InAbilitySystem->GetSet<UShadeAwarenessSet>() ||
        InAbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead))
    {
        Shutdown();
        return false;
    }
    if (AbilitySystem != InAbilitySystem)
    {
        Shutdown();
        AbilitySystem = InAbilitySystem;
        AlertnessHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
            UShadeAwarenessSet::GetAlertnessAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
        // With Alertness fixed at 100, MaxAlertness 100 -> 200 changes Alert to Suspicious.
        // The maximum can change without an Alertness notification, so subscribe to both.
        MaximumHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
            UShadeAwarenessSet::GetMaxAlertnessAttribute()).AddUObject(this, &ThisClass::HandleAttributeChanged);
        DeadHandle = AbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead)
            .AddUObject(this, &ThisClass::HandleDeadTagChanged);
    }
    UpdateState();
    return StateEffectHandle.IsValid();
}

void UShadeAwarenessComponent::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
    UpdateState();
}

void UShadeAwarenessComponent::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0) Shutdown();
}

void UShadeAwarenessComponent::UpdateState()
{
    if (!IsValid(AbilitySystem)) return;
    if (AbilitySystem->HasMatchingGameplayTag(ShadeGameplayTags::State_Dead))
    {
        Shutdown();
        return;
    }
    const auto* Attributes = AbilitySystem->GetSet<UShadeAwarenessSet>();
    if (!Attributes) return;
    const float Alertness = Attributes->GetAlertness();
    const auto Next = Alertness <= 0.f ? EShadeAwarenessState::Calm :
        (Alertness >= Attributes->GetMaxAlertness() ? EShadeAwarenessState::Alert : EShadeAwarenessState::Suspicious);
    if (Next == CurrentState) return;

    TSubclassOf<UGameplayEffect> EffectClass = UShadeAwarenessCalmEffect::StaticClass();
    if (Next == EShadeAwarenessState::Suspicious) EffectClass = UShadeAwarenessSuspiciousEffect::StaticClass();
    else if (Next == EShadeAwarenessState::Alert) EffectClass = UShadeAwarenessAlertEffect::StaticClass();
    const auto Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.f, AbilitySystem->MakeEffectContext());
    if (!ensure(Spec.IsValid())) return;

    // Only remove the effect this observer owns; never erase other systems' effects by tag.
    AbilitySystem->RemoveActiveGameplayEffect(StateEffectHandle);
    StateEffectHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    CurrentState = StateEffectHandle.IsValid() ? Next : EShadeAwarenessState::Inactive;
}

void UShadeAwarenessComponent::Shutdown()
{
    if (IsValid(AbilitySystem))
    {
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(
            UShadeAwarenessSet::GetAlertnessAttribute()).Remove(AlertnessHandle);
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(
            UShadeAwarenessSet::GetMaxAlertnessAttribute()).Remove(MaximumHandle);
        AbilitySystem->RegisterGameplayTagEvent(ShadeGameplayTags::State_Dead).Remove(DeadHandle);
        AbilitySystem->RemoveActiveGameplayEffect(StateEffectHandle);
    }
    AlertnessHandle.Reset();
    MaximumHandle.Reset();
    DeadHandle.Reset();
    StateEffectHandle.Invalidate();
    CurrentState = EShadeAwarenessState::Inactive;
    AbilitySystem = nullptr;
}

void UShadeAwarenessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Shutdown();
    Super::EndPlay(EndPlayReason);
}
