#include "Animation/SampleGameplayAnimNotifyStates.h"
#include "Characters/SampleMoverCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MoverDataModelTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleGameplayAnimNotifyStateMigrationTest,
    "GameAnimationSampleCpp.Migration.GameplayAnimNotifyStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleGameplayAnimNotifyStateMigrationTest::RunTest(const FString& Parameters)
{
    auto* Movement = NewObject<USampleOverrideMovementModeAnimNotifyState>();
    auto* Collision = NewObject<USampleOverrideCapsuleCollisionAnimNotifyState>();
    Collision->Response = ECR_Overlap;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Mesh owner"), Owner))
    {
        return false;
    }

    UCapsuleComponent* Root = NewObject<UCapsuleComponent>(Owner);
    Owner->SetRootComponent(Root);
    Root->RegisterComponent();
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    const FName Profile(TEXT("CharacterCapsule"));
    Root->SetCollisionProfileName(Profile);
    const FCollisionResponseContainer Original = Root->GetCollisionResponseToChannels();
    const FCollisionResponseContainer Overlap(ECR_Overlap);
    const FAnimNotifyEventReference EventReference;

    Collision->bRestoreCapsuleProfile = false;
    Collision->NotifyBegin(Mesh, nullptr, 1.0f, EventReference);
    TestTrue(TEXT("Begin applies the instance response to every root channel"), Root->GetCollisionResponseToChannels() == Overlap);
    Collision->NotifyEnd(Mesh, nullptr, EventReference);
    TestTrue(TEXT("Reset disabled preserves the override"), Root->GetCollisionResponseToChannels() == Overlap);

    Collision->bRestoreCapsuleProfile = true;
    Collision->NotifyEnd(Mesh, nullptr, EventReference);
    TestEqual(TEXT("Reset restores the authored profile"), Root->GetCollisionProfileName(), Profile);
    TestTrue(TEXT("Reset restores profile responses"), Root->GetCollisionResponseToChannels() == Original);

    Movement->NotifyBegin(Mesh, nullptr, 1.0f, EventReference);
    Movement->NotifyEnd(Mesh, nullptr, EventReference);
    TestTrue(TEXT("Movement notify ignores an unrelated owner"), Root->GetCollisionResponseToChannels() == Original);
    Collision->NotifyBegin(nullptr, nullptr, 1.0f, EventReference);
    Collision->NotifyEnd(nullptr, nullptr, EventReference);
    Movement->NotifyBegin(nullptr, nullptr, 1.0f, EventReference);
    Movement->NotifyEnd(nullptr, nullptr, EventReference);

    // Exercise the real authored component and its initialized simulation queue.
    UClass* PawnClass = LoadClass<ASampleMoverCharacter>(nullptr,
        TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
    if (!TestNotNull(TEXT("Saved Mover pawn class"), PawnClass))
    {
        return false;
    }
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn = World->SpawnActor<ASampleMoverCharacter>(PawnClass, FVector(0.0, 0.0, 500.0),
        FRotator::ZeroRotator, SpawnParameters);
    if (!TestNotNull(TEXT("Spawned Mover pawn"), Pawn))
    {
        return false;
    }
    const auto* MoverProperty = FindFProperty<FObjectPropertyBase>(PawnClass, TEXT("CharacterMover"));
    auto* Mover = MoverProperty ? Cast<UCharacterMoverComponent>(MoverProperty->GetObjectPropertyValue_InContainer(Pawn)) : nullptr;
    auto* PawnMesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
    if (!TestNotNull(TEXT("Authored CharacterMover reference"), Mover) ||
        !TestNotNull(TEXT("Authored pawn mesh"), PawnMesh))
    {
        return false;
    }
    if (!Pawn->HasActorBegunPlay())
    {
        Pawn->DispatchBeginPlay();
    }
    if (!TestTrue(TEXT("Mover has begun play"), Mover->HasBegunPlay()))
    {
        return false;
    }

    const FName Walking(TEXT("Walking"));
    const FName Flying(TEXT("Flying"));
    const FName Falling(TEXT("Falling"));
    for (const FName Mode : {Walking, Flying, Falling})
    {
        if (!TestNotNull(*FString::Printf(TEXT("Registered fixture mode %s"), *Mode.ToString()),
            Mover->FindMovementModeByName(Mode)))
        {
            return false;
        }
    }
    Movement->ModeOnBegin = Flying;
    Movement->ModeOnEnd = Walking;

    // Seed the cached simulation result explicitly, as in the Mover state test.
    // This leaves the backend's real queued-mode path observable without ticking the world.
    const auto SetSimulatedMode = [Mover](FName Mode)
    {
        FMoverSyncState State = Mover->GetSyncState();
        State.MovementMode = Mode;
        Mover->OnSimulationStateInitialized(State, FMoverAuxStateContext());
    };
    SetSimulatedMode(Walking);
    Mover->QueueNextMode(Falling);
    if (!TestEqual(TEXT("Backend accepts a distinct queued fixture mode"), Mover->GetNextMovementModeName(), Falling))
    {
        return false;
    }
    Movement->NotifyBegin(PawnMesh, nullptr, 1.0f, EventReference);
    TestEqual(TEXT("Begin queues the authored instance override"), Mover->GetNextMovementModeName(), Flying);
    TestEqual(TEXT("Begin does not immediately replace the simulated mode"), Mover->GetMovementModeName(), Walking);

    Movement->NotifyEnd(PawnMesh, nullptr, EventReference);
    TestEqual(TEXT("End before override is simulated leaves the queued override intact"), Mover->GetNextMovementModeName(), Flying);

    SetSimulatedMode(Flying);
    Movement->NotifyEnd(PawnMesh, nullptr, EventReference);
    TestEqual(TEXT("End queues the authored ending mode when override is current"), Mover->GetNextMovementModeName(), Walking);
    TestEqual(TEXT("End leaves the simulated override until simulation advances"), Mover->GetMovementModeName(), Flying);

    SetSimulatedMode(Falling);
    Mover->QueueNextMode(Flying);
    Movement->NotifyEnd(PawnMesh, nullptr, EventReference);
    TestEqual(TEXT("End after a different simulated mode preserves the existing queue"), Mover->GetNextMovementModeName(), Flying);

    // A native sibling of the authored Blueprint has the same reflected component field
    // and a functional queue. The original Blueprint cast rejects it on both callbacks.
    auto* UnrelatedPawn = World->SpawnActor<ASampleNotifyMoverFixture>(
        FVector(500.0, 0.0, 500.0), FRotator::ZeroRotator, SpawnParameters);
    if (!TestNotNull(TEXT("Unrelated native Mover pawn"), UnrelatedPawn))
    {
        return false;
    }
    auto* UnrelatedMover = UnrelatedPawn->CharacterMover.Get();
    auto* UnrelatedMesh = UnrelatedPawn->FindComponentByClass<USkeletalMeshComponent>();
    const auto* UnrelatedField = FindFProperty<FObjectPropertyBase>(UnrelatedPawn->GetClass(), TEXT("CharacterMover"));
    if (!TestNotNull(TEXT("Unrelated Mover component"), UnrelatedMover) ||
        !TestNotNull(TEXT("Unrelated Mover mesh"), UnrelatedMesh) ||
        !TestTrue(TEXT("Unrelated native pawn satisfies the former component lookup"),
            UnrelatedField && UnrelatedField->GetObjectPropertyValue_InContainer(UnrelatedPawn) == UnrelatedMover) ||
        !TestFalse(TEXT("Unrelated pawn is outside the authored hierarchy"), UnrelatedPawn->IsA(PawnClass)))
    {
        return false;
    }
    if (!UnrelatedPawn->HasActorBegunPlay())
    {
        UnrelatedPawn->DispatchBeginPlay();
    }
    FMoverSyncState UnrelatedState = UnrelatedMover->GetSyncState();
    UnrelatedState.MovementMode = Flying;
    UnrelatedMover->OnSimulationStateInitialized(UnrelatedState, FMoverAuxStateContext());
    UnrelatedMover->QueueNextMode(Falling);
    if (!TestEqual(TEXT("Unrelated backend accepts a distinct sentinel"), UnrelatedMover->GetNextMovementModeName(), Falling) ||
        !TestEqual(TEXT("Unrelated simulated mode matches the authored override"), UnrelatedMover->GetMovementModeName(), Flying))
    {
        return false;
    }
    Movement->NotifyBegin(UnrelatedMesh, nullptr, 1.0f, EventReference);
    TestEqual(TEXT("Begin leaves unrelated native Mover queue intact"), UnrelatedMover->GetNextMovementModeName(), Falling);
    Movement->NotifyEnd(UnrelatedMesh, nullptr, EventReference);
    TestEqual(TEXT("End leaves unrelated native Mover queue intact even when override matches"), UnrelatedMover->GetNextMovementModeName(), Falling);
    return true;
}

#endif
