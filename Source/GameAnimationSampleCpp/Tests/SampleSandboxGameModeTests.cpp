#include "../SampleSandboxGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSandboxGameModeMigrationTest,
    "GameAnimationSampleCpp.Migration.SandboxGameMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSandboxGameModeMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Sandbox = LoadClass<ASampleSandboxGameMode>(nullptr, TEXT("/Game/Blueprints/GM_Sandbox.GM_Sandbox_C"));
    UClass* Ragdoll = LoadClass<ASampleSandboxGameMode>(nullptr, TEXT("/Game/Blueprints/GM_Sandbox_Ragdoll.GM_Sandbox_Ragdoll_C"));
    IConsoleVariable* Selection = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.PawnClass"));
    if (!TestNotNull(TEXT("Saved sandbox class"), Sandbox) ||
        !TestNotNull(TEXT("Saved ragdoll class"), Ragdoll) ||
        !TestNotNull(TEXT("Original selection console variable"), Selection))
    {
        return false;
    }
    TestTrue(TEXT("The ragdoll GameMode retains its authored inheritance"), Ragdoll->IsChildOf(Sandbox));
    const UFunction* Reset = Sandbox->FindFunctionByName(TEXT("ResetAllPlayers"));
    TestTrue(TEXT("Reset resolves to the native implementation"),
        Reset && Reset->GetOuter() == ASampleSandboxGameMode::StaticClass());
    TestNull(TEXT("The obsolete Blueprint event was removed"), Sandbox->FindFunctionByName(TEXT("OnDataDrivenCVarChanged")));

    const int32 Previous = Selection->GetInt();
    const auto SetBy = static_cast<EConsoleVariableFlags>(Selection->GetFlags() & ECVF_SetByMask);
    ON_SCOPE_EXIT { Selection->Set(Previous, SetBy); };

    ASampleSandboxGameMode* Defaults = Sandbox->GetDefaultObject<ASampleSandboxGameMode>();
    ASampleSandboxGameMode* RagdollDefaults = Ragdoll->GetDefaultObject<ASampleSandboxGameMode>();
    Selection->Set(0, SetBy);
    TestEqual(TEXT("Choice zero loads the CMC pawn"), GetNameSafe(Defaults->GetDefaultPawnClassForController(nullptr)),
        FString(TEXT("SandboxCharacter_CMC_C")));
    Selection->Set(1, SetBy);
    TestEqual(TEXT("Choice one loads the Mover pawn"), GetNameSafe(Defaults->GetDefaultPawnClassForController(nullptr)),
        FString(TEXT("SandboxCharacter_Mover_C")));
    Selection->Set(-1, SetBy);
    TestEqual(TEXT("Negative choice uses the sandbox default"), Defaults->GetDefaultPawnClassForController(nullptr),
        Defaults->DefaultPawnClass.Get());
    Selection->Set(200, SetBy);
    TestEqual(TEXT("Out-of-range choice uses the ragdoll subclass default"), RagdollDefaults->GetDefaultPawnClassForController(nullptr),
        RagdollDefaults->DefaultPawnClass.Get());
    return true;
}

#endif
