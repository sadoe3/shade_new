#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "../AI/ShadeAwarenessComponent.h"
#include "../AbilitySystem/ShadeAwarenessSet.h"
#include "../AbilitySystem/ShadeAwarenessEffects.h"
#include "../AbilitySystem/ShadeHealthEffects.h"
#include "../AbilitySystem/ShadeGameplayTags.h"
#include "../Character/ShadeEnemyCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShadeAwarenessObserverTest, "Shade.Enemy.AwarenessObserver",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShadeAwarenessObserverTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine"), GEngine)) return false;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
        .ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AwarenessObserverTestWorld"),
        nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = World->SpawnActor<AShadeEnemyCharacter>(Spawn);
    if (!TestNotNull(TEXT("Enemy"), Enemy)) return false;
    auto* ASC = Enemy->GetAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Enemy, Enemy);
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 100.f);
    auto* Observer = NewObject<UShadeAwarenessComponent>(Enemy);
    Enemy->AddInstanceComponent(Observer);
    Observer->RegisterComponent();
    TestFalse(TEXT("Null ASC is rejected"), Observer->Initialize(nullptr));
    TestTrue(TEXT("Observer starts after attributes are ready"), Observer->Initialize(ASC));
    TestTrue(TEXT("Zero alertness grants Calm"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Calm));
    TestTrue(TEXT("Repeated initialization succeeds"), Observer->Initialize(ASC));
    TestEqual(TEXT("Repeated initialization does not stack Calm"), ASC->GetTagCount(ShadeGameplayTags::State_Awareness_Calm), 1);
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 40.f);
    TestTrue(TEXT("Positive alertness grants Suspicious"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Suspicious));
    TestFalse(TEXT("Previous Calm is removed"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Calm));
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 100.f);
    TestTrue(TEXT("Upper bound grants Alert"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Alert));
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 200.f);
    TestEqual(TEXT("Maximum-only change preserves current alertness"), Enemy->GetAwarenessSet()->GetAlertness(), 100.f);
    TestTrue(TEXT("Maximum-only change re-evaluates Suspicious"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Suspicious));
    TestFalse(TEXT("Alert removed on larger maximum"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Alert));
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetMaxAlertnessAttribute(), 50.f);
    TestTrue(TEXT("Lower maximum and clamp produce Alert"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Alert));
    TestEqual(TEXT("Nested attribute notifications keep one Alert"), ASC->GetTagCount(ShadeGameplayTags::State_Awareness_Alert), 1);

    const auto OtherEffect = ASC->ApplyGameplayEffectToSelf(GetDefault<UShadeAwarenessAlertEffect>(), 1.f, ASC->MakeEffectContext());
    Observer->Shutdown();
    TestEqual(TEXT("Shutdown preserves another owner's same-tag effect"), ASC->GetTagCount(ShadeGameplayTags::State_Awareness_Alert), 1);
    ASC->RemoveActiveGameplayEffect(OtherEffect);
    ASC->SetNumericAttributeBase(UShadeAwarenessSet::GetAlertnessAttribute(), 0.f);
    TestFalse(TEXT("Shutdown disconnects attribute notifications"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Calm));
    Observer->Shutdown();
    TestTrue(TEXT("Reconnection takes a current snapshot"), Observer->Initialize(ASC));
    TestTrue(TEXT("Reconnection restores Calm"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Calm));
    ASC->ApplyGameplayEffectToSelf(GetDefault<UShadeDeadEffect>(), 1.f, ASC->MakeEffectContext());
    TestTrue(TEXT("Death shuts the observer down"), Observer->GetCurrentState() == EShadeAwarenessState::Inactive);
    TestFalse(TEXT("Death removes owned awareness state"), ASC->HasMatchingGameplayTag(ShadeGameplayTags::State_Awareness_Calm));
    TestFalse(TEXT("Dead owner cannot restart observation"), Observer->Initialize(ASC));
    return true;
}

#endif
