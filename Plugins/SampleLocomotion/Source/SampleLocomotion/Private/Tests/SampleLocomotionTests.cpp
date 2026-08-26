#include "SampleLocomotionAnimInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleLocomotionCharacterTest,
    "SampleLocomotion.Character.NativeMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleLocomotionCharacterTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }

    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    ASampleLocomotionCharacter* Character = World->SpawnActor<ASampleLocomotionCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Native character"), Character) ||
        !TestNotNull(TEXT("Player controller"), Controller))
    {
        return false;
    }

    Controller->Possess(Character);
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float InitialSpeed = Movement->MaxWalkSpeed;

    Character->SetRequestedGait(ESampleGait::Walk);
    TestTrue(TEXT("Walking is slower than running"), Movement->MaxWalkSpeed < InitialSpeed);
    Character->SetRequestedGait(ESampleGait::Sprint);
    TestTrue(TEXT("Sprinting is faster than running"), Movement->MaxWalkSpeed > InitialSpeed);
    Character->SetRequestedGait(static_cast<ESampleGait>(255));
    TestTrue(TEXT("An invalid gait leaves sprint selected"), Character->GetRequestedGait() == ESampleGait::Sprint);

    Character->SetStrafing(true);
    TestTrue(TEXT("Strafe follows control rotation"),
        Movement->bUseControllerDesiredRotation && !Movement->bOrientRotationToMovement);

    Character->MoveRelativeToYaw(FVector2D(0.0, 0.5), 90.0f);
    TestTrue(TEXT("Camera-space forward preserves analog magnitude"),
        Character->ConsumeMovementInputVector().Equals(FVector(0.0, 0.5, 0.0), 0.001));

    Character->MoveRelativeToYaw(FVector2D(1.0, 1.0), 0.0f);
    const FVector Diagonal = Character->ConsumeMovementInputVector();
    TestTrue(TEXT("Diagonal input is clamped"), FMath::IsNearlyEqual(Diagonal.Size(), 1.0, 0.001));
    TestTrue(TEXT("Diagonal input keeps its heading"), FMath::IsNearlyEqual(Diagonal.X, Diagonal.Y, 0.001));

    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(0.0, 100.0, -300.0);
    const FSampleLocomotionSnapshot Sample = FSampleLocomotionSnapshot::Capture(Character);
    TestTrue(TEXT("Snapshot reports falling and strafe"), Sample.bIsFalling && Sample.bIsStrafing);
    TestEqual(TEXT("Ground speed excludes vertical movement"), Sample.GroundSpeed, 100.0f);
    TestEqual(TEXT("Rightward velocity is positive 90 degrees"), Sample.DirectionDegrees, 90.0f);
    TestEqual(TEXT("Missing owners return a neutral snapshot"),
        FSampleLocomotionSnapshot::Capture(nullptr).GroundSpeed, 0.0f);
    return true;
}

#endif
