#include "Animation/SampleAnimationDrawContext.h"
#include "Animation/SampleMoverAnimationDiagnostics.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleAnimationDiagnosticsFormattingTest,
    "GameAnimationSampleCpp.Migration.AnimationDiagnosticsFormatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleAnimationDiagnosticsFormattingTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Current State field text"),
        USampleMoverAnimationDiagnostics::FormatGraphDiagnosticLine(
            ESampleAnimationDiagnosticField::CurrentState, TEXT("Grounded")),
        FString(TEXT("Current State: Grounded")));
    TestEqual(TEXT("Last Transition field text"),
        USampleMoverAnimationDiagnostics::FormatGraphDiagnosticLine(
            ESampleAnimationDiagnosticField::LastTransition, TEXT("Start Moving")),
        FString(TEXT("Last Transition: Start Moving")));
    TestEqual(TEXT("Current Anim field text"),
        USampleMoverAnimationDiagnostics::FormatGraphDiagnosticLine(
            ESampleAnimationDiagnosticField::CurrentAnim, TEXT("Jog_Fwd")),
        FString(TEXT("Current Anim: Jog_Fwd")));
    TestEqual(TEXT("Last MM Cost field text"),
        USampleMoverAnimationDiagnostics::FormatGraphDiagnosticLine(
            ESampleAnimationDiagnosticField::LastMotionMatchingCost, TEXT("0.125")),
        FString(TEXT("Last MM Cost: 0.125")));

    TestEqual(TEXT("State row text"),
        USampleMoverAnimationDiagnostics::FormatStateLine(
            TEXT("Grounded"), TEXT("Standing"), TEXT("Running"), TEXT("Forward")),
        FString(TEXT("Grounded | Standing | Running | Forward")));

    TestTrue(TEXT("Null animation instance returns zero rotation"),
        USampleAnimationDrawContext::ResolveDrawRotation(nullptr) == FRotator::ZeroRotator);

    return true;
}

#endif
