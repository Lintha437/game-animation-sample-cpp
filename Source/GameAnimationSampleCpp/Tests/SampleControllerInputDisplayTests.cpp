#include "../SampleControllerInputDisplay.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleControllerInputDisplayMigrationTest,
    "GameAnimationSampleCpp.Migration.ControllerInputDisplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleControllerInputDisplayMigrationTest::RunTest(const FString& Parameters)
{
    const TArray<FKey> Keys = { EKeys::W, EKeys::Gamepad_LeftTriggerAxis, EKeys::W };
    TestEqual(TEXT("Null controller returns zero for analog keys"),
        USampleControllerInputDisplay::SampleAnalogKeys(nullptr, Keys), 0.0f);
    TestEqual(TEXT("Null controller returns zero for an empty analog key array"),
        USampleControllerInputDisplay::SampleAnalogKeys(nullptr, TArray<FKey>()), 0.0f);
    TestTrue(TEXT("Null controller returns a zero keyboard vector"),
        USampleControllerInputDisplay::SampleWSAD(nullptr).IsZero());

    float Opacity = -1.0f;
    double TranslationX = -1.0;
    double TranslationY = -1.0;
    USampleControllerInputDisplay::SampleLeftStick(
        nullptr, FVector::ZeroVector, Opacity, TranslationX, TranslationY);
    TestEqual(TEXT("Null left-stick input has zero opacity"), Opacity, 0.0f);
    TestEqual(TEXT("Null left-stick input has zero X translation"), TranslationX, 0.0);
    TestEqual(TEXT("Null left-stick input has zero Y translation"), TranslationY, 0.0);

    Opacity = -1.0f;
    TranslationX = -1.0;
    TranslationY = -1.0;
    USampleControllerInputDisplay::SampleRightStick(
        nullptr, FVector::ZeroVector, Opacity, TranslationX, TranslationY);
    TestEqual(TEXT("Null right-stick input has zero opacity"), Opacity, 0.0f);
    TestEqual(TEXT("Null right-stick input has zero X translation"), TranslationX, 0.0);
    TestEqual(TEXT("Null right-stick input has zero Y translation"), TranslationY, 0.0);

    USampleControllerInputDisplay::SampleLeftStick(
        nullptr, FVector(3.0, 4.0, 0.0), Opacity, TranslationX, TranslationY);
    TestEqual(TEXT("Left-stick opacity uses the unbounded XY magnitude"), Opacity, 5.0f);
    TestEqual(TEXT("Left-stick translation clamps the contribution to unit length"), TranslationX, 24.0);
    TestEqual(TEXT("Left-stick translation inverts Y"), TranslationY, -32.0);

    USampleControllerInputDisplay::SampleRightStick(
        nullptr, FVector(3.0, 4.0, 0.0), Opacity, TranslationX, TranslationY);
    TestEqual(TEXT("Right-stick opacity uses the unbounded XY magnitude"), Opacity, 5.0f);
    TestEqual(TEXT("Right-stick translation clamps the contribution to unit length"), TranslationX, 24.0);
    TestEqual(TEXT("Right-stick translation preserves Y"), TranslationY, 32.0);

    USampleControllerInputDisplay::SampleLeftStick(
        nullptr, FVector(0.2, 0.0, 0.0), Opacity, TranslationX, TranslationY);
    TestEqual(TEXT("Stick opacity is zero at the strict 0.2 threshold"), Opacity, 0.0f);

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world for mouse interpolation"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    const FVector Current(0.8, -0.4, 0.0);
    World->DeltaTimeSeconds = 0.025f;
    const FVector Interpolated = USampleControllerInputDisplay::SampleMouse(World, nullptr, Current);
    TestTrue(TEXT("Null mouse input interpolates the previous value toward zero at speed 10"),
        Interpolated.Equals(FVector(0.6, -0.3, 0.0), 1.e-4));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
