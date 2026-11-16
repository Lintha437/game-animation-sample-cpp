#include "../SampleSmartObjectInteractionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSmartObjectCancellationStateTest,
    "GameAnimationSampleCpp.Migration.SmartObjectCancellationState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSmartObjectCancellationStateTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USampleSmartObjectInteractionComponent>(nullptr,
        TEXT("/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation.AC_SmartObjectAnimation_C"));
    if (!TestNotNull(TEXT("Imported interaction component derives from the native class"), Class))
    {
        return false;
    }

    const auto* Active = FindFProperty<FBoolProperty>(Class, TEXT("bPlayerInteractionActive"));
    const auto* IsInteracting = FindFProperty<FBoolProperty>(Class, TEXT("PlayerIsInteracting"));
    if (!TestTrue(TEXT("Native and authored interaction state fields are available"), Active && IsInteracting))
    {
        return false;
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());

    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Authoritative interaction owner"), Owner))
    {
        return false;
    }
    auto* Component = NewObject<USampleSmartObjectInteractionComponent>(Owner, Class);
    if (!TestNotNull(TEXT("Imported interaction component instance"), Component))
    {
        return false;
    }
    Owner->AddInstanceComponent(Component);
    Component->RegisterComponent();

    if (!TestTrue(TEXT("The cleanup fixture runs on authority"), Owner->HasAuthority()) ||
        !TestTrue(TEXT("The interaction component is registered"), Component->IsRegistered()))
    {
        return false;
    }
    Active->SetPropertyValue_InContainer(Component, true);
    IsInteracting->SetPropertyValue_InContainer(Component, true);

    Component->ClearPlayerInteractionState();

    // NPCs may legitimately retain a Smart Object slot after this player cancels.
    // Verify the player's own interaction state instead of using shared slot occupancy.
    TestFalse(TEXT("Cancellation cleanup clears the replicated activity bit"),
        Active->GetPropertyValue_InContainer(Component));
    TestFalse(TEXT("Cancellation cleanup clears the authored interaction state"),
        IsInteracting->GetPropertyValue_InContainer(Component));
    return true;
}

#endif
