#include "Characters/SampleMoverModes.h"
#include "Characters/SampleMoverCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleNativeMoverModesTest,
    "GameAnimationSampleCpp.Migration.NativeMoverModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleNativeMoverModesTest::RunTest(const FString& Parameters)
{
    const auto* WalkingCdo = GetDefault<USampleMoverWalkingMode>();
    TestEqual(TEXT("Native walking run speed"), WalkingCdo->WalkSettings.RunSpeed, 375.0);
    TestEqual(TEXT("Native walking sprint speed"), WalkingCdo->WalkSettings.SprintSpeed, 585.0);
    TestEqual(TEXT("Native walking walk speed"), WalkingCdo->WalkSettings.WalkSpeed, 165.0);
    TestEqual(TEXT("Native walking crouch speed"), WalkingCdo->WalkSettings.CrouchSpeed, 165.0);
    TestTrue(TEXT("Native walking aligns velocity to slope"), WalkingCdo->WalkSettings.bSlopeRelativeVelocity);
    TestTrue(TEXT("Native walking shares stance settings"),
        WalkingCdo->SharedSettingsClasses.Contains(UStanceSettings::StaticClass()));
    TestEqual(TEXT("Native walking configures enter-slide transition"), WalkingCdo->Transitions.Num(), 1);
    if (WalkingCdo->Transitions.Num() == 1)
    {
        TestTrue(TEXT("Native walking transition class"),
            WalkingCdo->Transitions[0] && WalkingCdo->Transitions[0]->IsA<USampleMoverEnterSlideTransition>());
    }

    const auto* SlidingCdo = GetDefault<USampleMoverSlidingMode>();
    TestEqual(TEXT("Native sliding shallow slope angle"), SlidingCdo->SlideSettings.ShallowSlopeAngle, 10.0);
    TestEqual(TEXT("Native sliding steep slope angle"), SlidingCdo->SlideSettings.SteepSlopeAngle, 40.0);
    TestEqual(TEXT("Native sliding initial boost time"), SlidingCdo->SlideSettings.InitialBoostTime, 0.2);
    TestTrue(TEXT("Native sliding shares stance settings"),
        SlidingCdo->SharedSettingsClasses.Contains(UStanceSettings::StaticClass()));
    TestEqual(TEXT("Native sliding configures exit-slide transition"), SlidingCdo->Transitions.Num(), 1);
    if (SlidingCdo->Transitions.Num() == 1)
    {
        TestTrue(TEXT("Native sliding transition class"),
            SlidingCdo->Transitions[0] && SlidingCdo->Transitions[0]->IsA<USampleMoverExitSlideTransition>());
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());

    for (const TCHAR* PawnPath : {
        TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"),
        TEXT("/Game/Blueprints/SandboxCharacter_Mover_Ragdoll.SandboxCharacter_Mover_Ragdoll_C")
    })
    {
        UClass* PawnClass = LoadClass<ASampleMoverCharacter>(nullptr, PawnPath);
        if (!TestNotNull(PawnPath, PawnClass))
        {
            return false;
        }
        ASampleMoverCharacter* Pawn = World->SpawnActor<ASampleMoverCharacter>(PawnClass);
        auto* Mover = Pawn ? Pawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
        if (!TestNotNull(TEXT("Spawned Mover component"), Mover))
        {
            return false;
        }
        TArray<FName> ExpectedModes = {FName(TEXT("Walking")), FName(TEXT("Falling")),
            FName(TEXT("Flying")), FName(TEXT("Sliding")), FName(TEXT("Traversing"))};
        if (FString(PawnPath).Contains(TEXT("Ragdoll")))
        {
            ExpectedModes.Add(FName(TEXT("Ragdoll")));
        }
        for (const FName ExpectedMode : ExpectedModes)
        {
            const UBaseMovementMode* Mode = Mover->MovementModes.FindRef(ExpectedMode);
            if (!TestNotNull(*ExpectedMode.ToString(), Mode))
            {
                return false;
            }
            TestTrue(*FString::Printf(TEXT("%s uses a native mode class"), *ExpectedMode.ToString()),
                Mode->GetClass()->HasAnyClassFlags(CLASS_Native));
            for (const UBaseMovementModeTransition* Transition : Mode->Transitions)
            {
                TestTrue(*FString::Printf(TEXT("%s transitions use native classes"), *ExpectedMode.ToString()),
                    Transition && Transition->GetClass()->HasAnyClassFlags(CLASS_Native));
            }
        }
    }

    auto* EnterSlide = NewObject<USampleMoverEnterSlideTransition>();
    auto* ExitSlide = NewObject<USampleMoverExitSlideTransition>();
    FSimulationTickParams Params;
    Params.ProposedMove.LinearVelocity = FVector(450.0, 0.0, 0.0);
    TestEqual(TEXT("Enter slide requires crouch tag"),
        EnterSlide->Evaluate(Params).NextMode, NAME_None);
    Params.ProposedMove.LinearVelocity = FVector(150.0, 0.0, 0.0);
    TestEqual(TEXT("Exit slide returns to Walking below threshold"),
        ExitSlide->Evaluate(Params).NextMode, FName(TEXT("Walking")));
    return true;
}

#endif
