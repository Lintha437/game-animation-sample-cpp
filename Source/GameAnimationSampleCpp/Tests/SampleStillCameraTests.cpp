#include "../SampleSandboxFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleStillCameraTest,
    "GameAnimationSampleCpp.Migration.StillCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleStillCameraTest::RunTest(const FString& Parameters)
{
    UClass* CameraClass = LoadClass<ASampleStillCamera>(nullptr, TEXT("/Game/Widgets/WidgetData/StillCam.StillCam_C"));
    if (!TestNotNull(TEXT("Imported still camera derives from the native actor"), CameraClass))
    {
        return false;
    }
    const auto* TargetField = FindFProperty<FObjectPropertyBase>(CameraClass, TEXT("TargetActor"));
    const auto* LookAt = FindFProperty<FBoolProperty>(CameraClass, TEXT("LookAtTarget"));
    const auto* Follow = FindFProperty<FBoolProperty>(CameraClass, TEXT("FollowTarget"));
    const auto* LastLocation = FindFProperty<FStructProperty>(CameraClass, TEXT("LastTargetLocation"));
    if (!TestTrue(TEXT("The camera retains its authored fields"),
        TargetField && LookAt && Follow && LastLocation && LastLocation->Struct == TBaseStructure<FVector>::Get()))
    {
        return false;
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Camera = World->SpawnActor<ASampleStillCamera>(CameraClass);
    auto* Target = World->SpawnActor<ATargetPoint>();
    if (!TestTrue(TEXT("Camera and target spawned"), Camera && Target))
    {
        return false;
    }
    TestTrue(TEXT("The saved camera ticks natively"), Camera->PrimaryActorTick.bCanEverTick);
    const UFunction* TickEvent = Camera->FindFunction(TEXT("ReceiveTick"));
    TestTrue(TEXT("The old Blueprint Tick has no bytecode"), TickEvent && TickEvent->Script.IsEmpty());

    const FVector Start(100, 100, 20);
    const FVector FirstTarget(150, 200, 20);
    const FVector SecondTarget(250, 100, 20);
    const FVector FollowOffset = SecondTarget - FirstTarget;
    Camera->SetActorLocation(Start);
    Target->SetActorLocation(FirstTarget);
    TargetField->SetObjectPropertyValue_InContainer(Camera, Target);
    LookAt->SetPropertyValue_InContainer(Camera, true);
    Follow->SetPropertyValue_InContainer(Camera, false);
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Look-only leaves the camera at its placed position"), Camera->GetActorLocation().Equals(Start));
    TestTrue(TEXT("Look-only remembers the target's position"),
        LastLocation->ContainerPtrToValuePtr<FVector>(Camera)->Equals(FirstTarget));
    TestTrue(TEXT("Look-only aims at the target"), Camera->GetActorForwardVector().Equals((FirstTarget - Start).GetSafeNormal()));

    Target->SetActorLocation(SecondTarget);
    Follow->SetPropertyValue_InContainer(Camera, true);
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Follow adds only the target's movement"), Camera->GetActorLocation().Equals(Start + FollowOffset));
    TestTrue(TEXT("Look uses the camera position before following"), Camera->GetActorForwardVector().Equals(FVector::ForwardVector));
    TestTrue(TEXT("Follow saves the new target position"),
        LastLocation->ContainerPtrToValuePtr<FVector>(Camera)->Equals(SecondTarget));

    TargetField->SetObjectPropertyValue_InContainer(Camera, nullptr);
    Camera->Tick(1.f / 60.f);
    TestTrue(TEXT("Losing the target keeps the camera in place"), Camera->GetActorLocation().Equals(Start + FollowOffset));
    TestTrue(TEXT("Losing the target keeps the cached position"),
        LastLocation->ContainerPtrToValuePtr<FVector>(Camera)->Equals(SecondTarget));
    return true;
}

#endif
