#include "SampleTraversalProbeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleTraversalProbeTest,
    "SampleLocomotion.Traversal.GeometryAndClearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleTraversalProbeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }

    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ACharacter* Character = World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("Capsule owner"), Character))
    {
        return false;
    }

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    Character->SetActorLocation(FVector(0.0, 0.0, Capsule->GetScaledCapsuleHalfHeight()));
    USampleTraversalProbeComponent* Probe = NewObject<USampleTraversalProbeComponent>(Character);
    Probe->RegisterComponent();

    auto AddBlockingBox = [World](const FVector& Center, const FVector& Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Box->InitBoxExtent(Extent);
        Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Actor->SetRootComponent(Box);
        Box->RegisterComponent();
        Actor->SetActorLocation(Center);
        return Box;
    };

    FSampleTraversalOpportunity Candidate;
    TestFalse(TEXT("Empty scenery has no opportunity"),
        Probe->FindOpportunity(Capsule, FVector::ForwardVector, Candidate));

    AddBlockingBox(FVector(155.0, 0.0, 50.0), FVector(55.0, 105.0, 50.0));
    TestTrue(TEXT("A reachable ledge with a clear top is found"),
        Probe->FindOpportunity(Capsule, FVector::ForwardVector, Candidate));
    TestTrue(TEXT("Height is measured from the capsule base"),
        FMath::IsNearlyEqual(Candidate.Height, 100.0f, 0.1f));

    AddBlockingBox(FVector(150.0, 0.0, 235.0), FVector(60.0, 105.0, 15.0));
    TestFalse(TEXT("A ceiling in the destination capsule rejects the opportunity"),
        Probe->FindOpportunity(Capsule, FVector::ForwardVector, Candidate));
    TestTrue(TEXT("Failure clears previous outputs"), Candidate.CandidateCapsuleCenter.IsZero());
    TestFalse(TEXT("A vertical heading cannot find a ledge"),
        Probe->FindOpportunity(Capsule, FVector::UpVector, Candidate));
    return true;
}

#endif
