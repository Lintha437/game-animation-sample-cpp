#include "../SampleSandboxPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSandboxControllerMigrationTest,
    "GameAnimationSampleCpp.Migration.SandboxController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSandboxControllerMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleSandboxPlayerController>(
        nullptr, TEXT("/Game/Blueprints/PC_Sandbox.PC_Sandbox_C"));
    if (!TestNotNull(TEXT("Saved sandbox player controller"), Class))
    {
        return false;
    }
    const UFunction* Teleport = Class->FindFunctionByName(TEXT("TeleportToTarget"));
    TestTrue(TEXT("Teleport resolves to the native controller"),
        Teleport && Teleport->GetOuter() == ASampleSandboxPlayerController::StaticClass());
    const UFunction* NextPawn = Class->FindFunctionByName(TEXT("NextPawn"));
    const UFunction* NextVisualOverride = Class->FindFunctionByName(TEXT("NextVisualOverride"));
    TestTrue(TEXT("Pawn changes retain reliable server dispatch"),
        NextPawn && (NextPawn->FunctionFlags & FUNC_NetFuncFlags) == (FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
    TestTrue(TEXT("Visual changes retain unreliable server dispatch"),
        NextVisualOverride && (NextVisualOverride->FunctionFlags & FUNC_NetFuncFlags) == (FUNC_Net | FUNC_NetServer));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    ASampleSandboxPlayerController* Controller = World->SpawnActor<ASampleSandboxPlayerController>(Class);
    ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>();
    if (!TestNotNull(TEXT("Sandbox controller instance"), Controller) || !TestNotNull(TEXT("Possessable pawn"), Pawn))
    {
        return false;
    }
    Controller->TeleportToTarget();
    Controller->Possess(Pawn);
    Controller->SetControlRotation(FRotator::ZeroRotator);
    const FVector OriginalLocation = Pawn->GetActorLocation();
    const FRotator OriginalRotation = Pawn->GetActorRotation();

    FVector Start;
    FRotator View;
    Controller->GetActorEyesViewPoint(Start, View);
    Controller->TeleportToTarget();
    TestTrue(TEXT("A clear view uses the end of the trace"),
        Pawn->GetActorLocation().Equals(Start + View.Vector() * 10000.0, 0.1));
    TestTrue(TEXT("Teleport preserves pawn rotation"), Pawn->GetActorRotation().Equals(OriginalRotation));

    Pawn->SetActorLocation(OriginalLocation);
    Controller->GetActorEyesViewPoint(Start, View);
    AActor* Blocker = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Blocking geometry actor"), Blocker))
    {
        return false;
    }
    UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
    Blocker->SetRootComponent(Box);
    Box->InitBoxExtent(FVector(30.0));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box->SetWorldLocation(Start + View.Vector() * 500.0);
    Box->RegisterComponent();

    Controller->TeleportToTarget();
    TestTrue(TEXT("A blocking hit applies sphere radius and surface clearance"),
        Pawn->GetActorLocation().Equals(Start + View.Vector() * 388.0, 1.0));
    return true;
}

#endif
