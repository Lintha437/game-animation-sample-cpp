#include "Characters/SampleCmcCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSampleCmcFoleyVolumeTest,
    "GameAnimationSampleCpp.Movement.CmcFoleyVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCmcFoleyVolumeTest::RunTest(const FString& Parameters)
{
    const auto* Character = GetDefault<ASampleCmcCharacter>();
    const double Extreme = std::numeric_limits<double>::max();
    const struct { double Input; double Expected; } Jump[] = {
        {-Extreme, 0.5}, {-1.0, 0.5}, {0.0, 0.5}, {125.0, 0.625},
        {250.0, 0.75}, {375.0, 0.875}, {500.0, 1.0}, {501.0, 1.0}, {Extreme, 1.0}
    }, Landing[] = {
        {Extreme, 0.5}, {900.0, 0.5}, {0.0, 0.5}, {-499.0, 0.5}, {-500.0, 0.5},
        {-600.0, 0.75}, {-700.0, 1.0}, {-800.0, 1.25}, {-900.0, 1.5},
        {-901.0, 1.5}, {-Extreme, 1.5}
    };
    for (const auto& Case : Jump)
    {
        TestEqual(FString::Printf(TEXT("Jump volume at %.17g cm/s"), Case.Input),
            Character->CalculateJumpFoleyVolume(Case.Input), Case.Expected);
    }
    for (const auto& Case : Landing)
    {
        TestEqual(FString::Printf(TEXT("Landing volume at %.17g cm/s"), Case.Input),
            Character->CalculateLandingFoleyVolume(Case.Input), Case.Expected);
    }

    // A float conversion would erase these small, representable changes at the midpoints.
    TestTrue(TEXT("Jump retains sub-float speed precision"),
        Character->CalculateJumpFoleyVolume(250.000001) > 0.75);
    TestTrue(TEXT("Landing retains sub-float speed precision"),
        Character->CalculateLandingFoleyVolume(-700.000001) > 1.0);

    for (const FName Name : {FName(TEXT("CalculateJumpFoleyVolume")), FName(TEXT("CalculateLandingFoleyVolume"))})
    {
        const UFunction* Function = ASampleCmcCharacter::StaticClass()->FindFunctionByName(Name);
        if (TestNotNull(TEXT("Reflected native volume function"), Function))
        {
            TestTrue(TEXT("Native pure const function"), Function->HasAllFunctionFlags(
                FUNC_Native | FUNC_BlueprintPure | FUNC_Const));
            TestEqual(TEXT("One input and one return"), static_cast<int32>(Function->NumParms), 2);
            const FName Input = Name == TEXT("CalculateJumpFoleyVolume")
                ? FName(TEXT("GroundSpeedBeforeJump")) : FName(TEXT("LandingVelocityZ"));
            TestNotNull(TEXT("Double input"), FindFProperty<FDoubleProperty>(Function, Input));
            TestNotNull(TEXT("Double return"), CastField<FDoubleProperty>(Function->GetReturnProperty()));
        }
    }
    return true;
}

#endif
