#include "../SampleSmartObjectTasks.h"
#include "../SampleSmartObjectInteractionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "InputAction.h"
#include "MoverComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSmartObjectApproachTest,
    "GameAnimationSampleCpp.Migration.SmartObjectApproachTask",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSmartObjectApproachTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USampleMotionMatchApproachTask>(nullptr,
        TEXT("/Game/Blueprints/SmartObjects/TasksAndConditions/STT_PlayAnimFromBestCost.STT_PlayAnimFromBestCost_C"));
    if (!TestNotNull(TEXT("Imported approach task derives from the native class"), Class))
    {
        return false;
    }
    const auto* ActorField = FindFProperty<FObjectPropertyBase>(Class, TEXT("Actor"));
    const auto* DestinationField = FindFProperty<FStructProperty>(Class, TEXT("Destination"));
    const auto* MoverField = FindFProperty<FObjectPropertyBase>(Class, TEXT("PossibleOwnerMoverComponent"));
    const auto* AnimationField = FindFProperty<FObjectPropertyBase>(Class, TEXT("Smart Object Animation Component"));
    if (!TestTrue(TEXT("The task fields remain available for StateTree bindings"),
        ActorField && DestinationField && DestinationField->Struct == TBaseStructure<FTransform>::Get() &&
        MoverField && AnimationField))
    {
        return false;
    }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ACharacter* Character = World->SpawnActor<ACharacter>();
    AAIController* Controller = World->SpawnActor<AAIController>();
    if (!TestTrue(TEXT("AI controller and character spawned"), Character && Controller))
    {
        return false;
    }
    Controller->Possess(Character);

    auto* Task = NewObject<USampleMotionMatchApproachTask>(GetTransientPackage(), Class);
    ActorField->SetObjectPropertyValue_InContainer(Task, Character);
    const FVector Origin = Character->GetActorLocation();
    const FVector Destination = Origin + FVector(30.0, 40.0, 0.0);
    *DestinationField->ContainerPtrToValuePtr<FTransform>(Task) = FTransform(Destination);
    Task->InitializeApproachTask();
    TestEqual(TEXT("Missing path uses the straight-line distance"), Task->ReadApproachDistance(), 50.0);
    Character->GetCharacterMovement()->Velocity = FVector(100.0, 50.0, 0.0);
    TestEqual(TEXT("A CMC character supplies the actor velocity"),
        Task->ReadApproachVelocity(), Character->GetVelocity());
    FScriptDelegate MoveHandler;
    MoveHandler.BindUFunction(Task, TEXT("OnMovementCompleted"));
    TestTrue(TEXT("The existing move callback is bound"), Controller->ReceiveMoveCompleted.Contains(MoveHandler));

    const FNavPathSharedPtr Path = MakeShared<FNavigationPath, ESPMode::ThreadSafe>(
        TArray<FVector>{Origin, Origin + FVector(0.0, 40.0, 0.0), Destination});
    FAIMoveRequest Request(Destination);
    Request.SetAcceptanceRadius(1.0f);
    if (TestTrue(TEXT("The controller starts following the test path"),
        Controller->GetPathFollowingComponent()->RequestMove(Request, Path).IsValid()))
    {
        TestEqual(TEXT("A path uses the full route length"), Task->ReadApproachDistance(), 70.0);
    }
    ActorField->SetObjectPropertyValue_InContainer(Task, nullptr);
    Task->InitializeApproachTask();
    TestFalse(TEXT("Reusing the task releases its previous AI binding"),
        Controller->ReceiveMoveCompleted.Contains(MoveHandler));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSmartObjectInputBindingsTest,
    "GameAnimationSampleCpp.Migration.SmartObjectInputBindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSmartObjectInputBindingsTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USampleSmartObjectInteractionComponent>(nullptr,
        TEXT("/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation.AC_SmartObjectAnimation_C"));
    if (!TestNotNull(TEXT("Imported interaction component derives from the native class"), Class))
    {
        return false;
    }
    const auto* Enabled = FindFProperty<FBoolProperty>(Class, TEXT("bUseNativePlayerInteractionInput"));
    if (!TestTrue(TEXT("Native input is enabled only after the asset migration"),
        Enabled && Enabled->GetPropertyValue_InContainer(Class->GetDefaultObject())))
    {
        return false;
    }

    auto* Component = NewObject<USampleSmartObjectInteractionComponent>(GetTransientPackage(), Class);
    auto* Input = NewObject<UEnhancedInputComponent>();
    Component->BindPlayerInteractionInput(*Input);
    const auto& Bindings = Input->GetActionEventBindings();
    if (!TestEqual(TEXT("Both authored actions are registered"), Bindings.Num(), 2))
    {
        return false;
    }

    const UInputAction* Interact = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Interact.IA_Interact"));
    const UInputAction* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Move.IA_Move"));
    TestNotNull(TEXT("Interaction action exists"), Interact);
    TestNotNull(TEXT("Movement cancellation action exists"), Move);
    TestTrue(TEXT("The interaction action triggers its native handler"),
        Bindings[0]->GetAction() == Interact && Bindings[0]->GetTriggerEvent() == ETriggerEvent::Triggered &&
        Bindings[0]->IsBoundToObject(Component));
    TestTrue(TEXT("Movement cancellation dispatches to the same component"),
        Bindings[1]->GetAction() == Move && Bindings[1]->GetTriggerEvent() == ETriggerEvent::Triggered &&
        Bindings[1]->IsBoundToObject(Component));

    TestFalse(TEXT("Native base keeps opt-in binding disabled"),
        Enabled->GetPropertyValue_InContainer(USampleSmartObjectInteractionComponent::StaticClass()->GetDefaultObject()));
    return true;
}

#endif
