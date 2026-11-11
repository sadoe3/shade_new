#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ShadeAwarenessComponent.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

enum class EShadeAwarenessState : uint8
{
    Inactive,
    Calm,
    Suspicious,
    Alert
};

// Authority-side state observer. Sensing and periodic gain/decay are separate work.
UCLASS()
class SHADE_API UShadeAwarenessComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShadeAwarenessComponent();
    // Call after the owner's ASC and awareness attributes have been initialized.
    bool Initialize(UAbilitySystemComponent* InAbilitySystem);
    void Shutdown();
    // Local observer state, not a replicated client-facing value. Clients use GAS tags.
    EShadeAwarenessState GetCurrentState() const { return CurrentState; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleAttributeChanged(const FOnAttributeChangeData& Data);
    void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
    void UpdateState();

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    FDelegateHandle AlertnessHandle;
    FDelegateHandle MaximumHandle;
    FDelegateHandle DeadHandle;
    FActiveGameplayEffectHandle StateEffectHandle;
    EShadeAwarenessState CurrentState = EShadeAwarenessState::Inactive;
};
