#include "../SampleCmcCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    bool SetEnumValue(UObject& Owner, FName Name, uint8 Value)
    {
        FProperty* Property = Owner.GetClass()->FindPropertyByName(Name);
        if (FEnumProperty* Enum = CastField<FEnumProperty>(Property))
        {
            Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(&Owner), uint64(Value));
            return true;
        }
        if (FByteProperty* Byte = CastField<FByteProperty>(Property))
        {
            Byte->SetPropertyValue_InContainer(&Owner, Value);
            return true;
        }
        return false;
    }

    int64 GetEnumValue(const UObject& Owner, FName Name)
    {
        const FProperty* Property = Owner.GetClass()->FindPropertyByName(Name);
        if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
        {
            return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(&Owner));
        }
        if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
        {
            return Byte->GetPropertyValue_InContainer(&Owner);
        }
        return INDEX_NONE;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCmcMovementMigrationTest,
    "GameAnimationSampleCpp.Migration.CmcMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCmcMovementMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleCmcCharacter>(
        nullptr, TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
    if (!TestNotNull(TEXT("Saved CMC class"), Class))
    {
        return false;
    }

    const FName NativeNames[] = {
        TEXT("GetMovementInputScaleValue"), TEXT("CanSprint"), TEXT("UpdateRotation_PreCMC"),
        TEXT("CalculateMaxAcceleration"), TEXT("CalculateGroundFriction"), TEXT("CalculateMaxSpeed"),
    };
    for (FName Name : NativeNames)
    {
        const UFunction* Function = Class->FindFunctionByName(Name);
        TestTrue(FString::Printf(TEXT("%s resolves to native code"), *Name.ToString()),
            Function && Function->GetOuter() == ASampleCmcCharacter::StaticClass() && Function->Script.IsEmpty());
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    ASampleCmcCharacter* Character = World->SpawnActor<ASampleCmcCharacter>(Class);
    if (!TestNotNull(TEXT("CMC character"), Character))
    {
        return false;
    }

    const FVector2D RawInput(0.3, 0.4);
    if (!TestTrue(TEXT("Authored stick mode"), SetEnumValue(*Character, TEXT("MovementStickMode"), 0)))
    {
        return false;
    }
    TestTrue(TEXT("Fixed input normalizes"), Character->GetMovementInputScaleValue(RawInput).Equals(FVector2D(0.6, 0.8)));
    TestEqual(TEXT("Zero input stays zero"), Character->GetMovementInputScaleValue(FVector2D::ZeroVector), FVector2D::ZeroVector);
    SetEnumValue(*Character, TEXT("MovementStickMode"), 2);
    TestEqual(TEXT("Variable input keeps magnitude"), Character->GetMovementInputScaleValue(RawInput), RawInput);

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->Velocity = FVector(500.0, 0.0, 800.0);
    if (!TestTrue(TEXT("Authored gait"), SetEnumValue(*Character, TEXT("Gait"), 1)))
    {
        return false;
    }
    TestEqual(TEXT("Run acceleration"), Character->CalculateMaxAcceleration(), 800.0);
    TestEqual(TEXT("Run ground friction"), Character->CalculateGroundFriction(), 5.0);
    SetEnumValue(*Character, TEXT("Gait"), 2);
    TestEqual(TEXT("Sprint acceleration ignores vertical velocity"), Character->CalculateMaxAcceleration(), 550.0);
    TestEqual(TEXT("Sprint ground friction at speed cap"), Character->CalculateGroundFriction(), 3.0);

    FStructProperty* State = FindFProperty<FStructProperty>(Class, TEXT("CharacterInputState"));
    if (!TestNotNull(TEXT("Authored input state"), State))
    {
        return false;
    }
    FBoolProperty* Aim = FindFProperty<FBoolProperty>(State->Struct, TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8"));
    FBoolProperty* Strafe = FindFProperty<FBoolProperty>(State->Struct, TEXT("WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9"));
    FBoolProperty* Sprint = FindFProperty<FBoolProperty>(State->Struct, TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936"));
    if (!TestNotNull(TEXT("Aim intent"), Aim) || !TestNotNull(TEXT("Strafe intent"), Strafe) ||
        !TestNotNull(TEXT("Sprint intent"), Sprint))
    {
        return false;
    }
    void* StateData = State->ContainerPtrToValuePtr<void>(Character);
    Strafe->SetPropertyValue_InContainer(StateData, false);
    Aim->SetPropertyValue_InContainer(StateData, true);
    Sprint->SetPropertyValue_InContainer(StateData, false);
    Movement->SetMovementMode(MOVE_Falling);
    Character->UpdateRotation_PreCMC();
    TestTrue(TEXT("Aiming uses controller rotation"), Movement->bUseControllerDesiredRotation);
    TestFalse(TEXT("Aiming disables orient-to-movement"), Movement->bOrientRotationToMovement);
    TestEqual(TEXT("Falling limits turn rate"), Movement->RotationRate.Yaw, 200.0);
    TestFalse(TEXT("Sprint requires intent"), Character->CanSprint());
    Aim->SetPropertyValue_InContainer(StateData, false);
    Sprint->SetPropertyValue_InContainer(StateData, true);
    Movement->SetMovementMode(MOVE_Walking);
    Character->UpdateRotation_PreCMC();
    TestTrue(TEXT("Ordinary movement orients to input"), Movement->bOrientRotationToMovement);
    TestEqual(TEXT("Grounded turns are immediate"), Movement->RotationRate.Yaw, -1.0);
    TestTrue(TEXT("Orient-to-movement allows requested sprint"), Character->CanSprint());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCmcGaitMigrationTest,
    "GameAnimationSampleCpp.Migration.CmcGait",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCmcGaitMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleCmcCharacter>(
        nullptr, TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
    if (!TestNotNull(TEXT("Saved CMC class"), Class))
    {
        return false;
    }
    const UFunction* Update = Class->FindFunctionByName(TEXT("UpdateMovementBeforeSimulation"));
    TestTrue(TEXT("Pre-movement update resolves to native code"),
        Update && Update->GetOuter() == ASampleCmcCharacter::StaticClass());
    TestNull(TEXT("Original gait graph was removed"), Class->FindFunctionByName(TEXT("GetDesiredGait")));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    ASampleCmcCharacter* Character = World->SpawnActor<ASampleCmcCharacter>(Class);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("CMC character"), Character) || !TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }
    const FName Gait(TEXT("Gait"));
    const FName StickMode(TEXT("MovementStickMode"));
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    SetEnumValue(*Character, StickMode, 0);
    Character->UpdateMovementBeforeSimulation();
    TestEqual(TEXT("Fixed single-gait mode runs without an input component"), GetEnumValue(*Character, Gait), int64(1));
    TestEqual(TEXT("Idle walking deceleration"), Movement->BrakingDecelerationWalking, 2000.0f);

    Controller->Player = NewObject<ULocalPlayer>(GEngine);
    Controller->Possess(Character);
    Character->PawnClientRestart();
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Character->InputComponent);
    UInputAction* CameraMove = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Move.IA_Move"));
    UInputAction* WorldMove = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/IA_Move_WorldSpace.IA_Move_WorldSpace"));
    UFunction* PreTick = Class->FindFunctionByName(TEXT("PreCMCTick"));
    if (!TestNotNull(TEXT("Possession creates an Enhanced Input component"), Input) ||
        !TestNotNull(TEXT("Camera-relative action"), CameraMove) ||
        !TestNotNull(TEXT("World-relative action"), WorldMove) ||
        !TestNotNull(TEXT("Pre-CMC dispatch entry"), PreTick))
    {
        return false;
    }

    UEnhancedPlayerInput* PlayerInput = NewObject<UEnhancedPlayerInput>(Controller);
    Controller->PlayerInput = PlayerInput;
    const TArray<UInputComponent*> Stack{Input};
    const auto ApplyMove = [&](const UInputAction* Action, double Strength)
    {
        PlayerInput->InjectInputForAction(Action, FInputActionValue(FVector2D(0.0, Strength)));
        PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
        TestTrue(TEXT("Enhanced Input updates the bound raw action value"),
            FMath::IsNearlyEqual(Input->GetBoundActionValue(Action).Get<FVector2D>().Size(), Strength, 1.e-4));
        Character->ProcessEvent(PreTick, nullptr);
    };

    TestTrue(TEXT("Select fixed dual-gait mode"), SetEnumValue(*Character, StickMode, 1));
    Character->ProcessEvent(PreTick, nullptr);
    TestEqual(TEXT("Pre-CMC dispatch applies gait at zero action magnitude"), GetEnumValue(*Character, Gait), int64(0));
    ApplyMove(CameraMove, 0.3);
    TestEqual(TEXT("Fixed dual-gait mode walks at below-threshold raw input"), GetEnumValue(*Character, Gait), int64(0));
    TestEqual(TEXT("Queued input reduces walking deceleration"), Movement->BrakingDecelerationWalking, 500.0f);
    ApplyMove(WorldMove, 0.9);
    TestEqual(TEXT("World-relative input above threshold runs"), GetEnumValue(*Character, Gait), int64(1));
    TestEqual(TEXT("Pre-CMC update sets the standing speed"), Movement->MaxWalkSpeed,
        static_cast<float>(Character->CalculateMaxSpeed()));

    FStructProperty* State = FindFProperty<FStructProperty>(Class, TEXT("CharacterInputState"));
    FBoolProperty* Sprint = State ? FindFProperty<FBoolProperty>(
        State->Struct, TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936")) : nullptr;
    if (!TestNotNull(TEXT("Sprint intent"), Sprint))
    {
        return false;
    }
    Sprint->SetPropertyValue_InContainer(State->ContainerPtrToValuePtr<void>(Character), true);
    SetEnumValue(*Character, StickMode, 3);
    Character->ConsumeMovementInputVector();
    ApplyMove(CameraMove, 0.2);
    TestEqual(TEXT("Sprint intent with partial movement runs"), GetEnumValue(*Character, Gait), int64(1));
    ApplyMove(CameraMove, 0.9);
    TestEqual(TEXT("Sprint intent with full movement sprints"), GetEnumValue(*Character, Gait), int64(2));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCmcInputMigrationTest,
    "GameAnimationSampleCpp.Migration.CmcInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCmcInputMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleCmcCharacter>(
        nullptr, TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
    if (!TestNotNull(TEXT("Saved CMC class"), Class))
    {
        return false;
    }
    const FBoolProperty* NativeInput = FindFProperty<FBoolProperty>(Class, TEXT("bUseNativeInputHandlers"));
    TestTrue(TEXT("The saved pawn enables native action handlers"),
        NativeInput && NativeInput->GetPropertyValue_InContainer(Class->GetDefaultObject()));
    const FBoolProperty* NativeIntent = FindFProperty<FBoolProperty>(Class, TEXT("bUseNativeInputIntentHandlers"));
    TestTrue(TEXT("The saved pawn enables native intent handlers"),
        NativeIntent && NativeIntent->GetPropertyValue_InContainer(Class->GetDefaultObject()));
    TestNull(TEXT("The authored server event was removed"), Class->FindFunctionByName(TEXT("UpdateInputState_Server")));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    ASampleCmcCharacter* Character = World->SpawnActor<ASampleCmcCharacter>(Class);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("CMC character"), Character) || !TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }

    Controller->Player = NewObject<ULocalPlayer>(GEngine);
    Controller->Possess(Character);
    Character->PawnClientRestart();
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Character->InputComponent);
    if (!TestNotNull(TEXT("Possession creates Enhanced Input"), Input))
    {
        return false;
    }
    UEnhancedPlayerInput* PlayerInput = NewObject<UEnhancedPlayerInput>(Controller);
    Controller->PlayerInput = PlayerInput;
    const TArray<UInputComponent*> Stack{Input};
    const auto FindAction = [&](const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Input/%s.%s"), Name, Name);
        const UInputAction* Action = LoadObject<UInputAction>(nullptr, *Path);
        int32 Bindings = 0;
        for (const auto& Binding : Input->GetActionEventBindings())
        {
            Bindings += Binding->GetAction() == Action && Binding->GetTriggerEvent() == ETriggerEvent::Triggered &&
                Binding->IsBoundToObject(Character);
        }
        TestEqual(FString::Printf(TEXT("%s has exactly one pawn-owned Triggered binding"), Name), Bindings, 1);
        return Action;
    };
    const UInputAction* CameraMove = FindAction(TEXT("IA_Move"));
    const UInputAction* WorldMove = FindAction(TEXT("IA_Move_WorldSpace"));
    const UInputAction* Look = FindAction(TEXT("IA_Look"));
    const UInputAction* GamepadLook = FindAction(TEXT("IA_Look_Gamepad"));
    const UInputAction* Crouch = FindAction(TEXT("IA_Crouch"));
    const UInputAction* Jump = FindAction(TEXT("IA_Jump"));
    const UInputAction* Walk = FindAction(TEXT("IA_Walk"));
    const UInputAction* Sprint = FindAction(TEXT("IA_Sprint"));
    const UInputAction* Strafe = FindAction(TEXT("IA_Strafe"));
    const UInputAction* Aim = FindAction(TEXT("IA_Aim"));
    if (!CameraMove || !WorldMove || !Look || !GamepadLook || !Crouch || !Jump ||
        !Walk || !Sprint || !Strafe || !Aim)
    {
        AddError(TEXT("An authored input action is missing."));
        return false;
    }
    const auto ApplyAction = [&](const UInputAction* Action, FInputActionValue Value)
    {
        PlayerInput->InjectInputForAction(Action, Value);
        PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
    };

    SetEnumValue(*Character, TEXT("MovementStickMode"), 2);
    Controller->SetControlRotation(FRotator(0.0, 90.0, 0.0));
    Character->ConsumeMovementInputVector();
    ApplyAction(CameraMove, FVector2D(0.0, 0.3));
    TestTrue(TEXT("Camera-relative input uses control yaw and retains analog strength"),
        Character->ConsumeMovementInputVector().Equals(FVector(0.0, 0.3, 0.0), 1.e-4));
    ApplyAction(WorldMove, FVector2D(0.0, 0.3));
    TestTrue(TEXT("World-relative input uses world axes at full strength"),
        Character->ConsumeMovementInputVector().Equals(FVector::ForwardVector, 1.e-4));

    const FVector2D LookValue(2.0, -3.0);
    Controller->RotationInput = FRotator::ZeroRotator;
    Controller->AddYawInput(LookValue.X);
    Controller->AddPitchInput(LookValue.Y);
    const FRotator ExpectedLook = Controller->RotationInput;
    TestFalse(TEXT("The local controller accepts look input"), ExpectedLook.IsNearlyZero());
    Controller->RotationInput = FRotator::ZeroRotator;
    ApplyAction(Look, LookValue);
    TestTrue(TEXT("Mouse deltas reach the controller once"), Controller->RotationInput.Equals(ExpectedLook, 1.e-4));
    World->DeltaTimeSeconds = 0.02f;
    Controller->RotationInput = FRotator::ZeroRotator;
    ApplyAction(GamepadLook, LookValue);
    TestTrue(TEXT("Gamepad rate uses world delta time"),
        Controller->RotationInput.Equals(ExpectedLook * World->GetDeltaSeconds(), 1.e-4));

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Falling);
    ApplyAction(Crouch, true);
    TestFalse(TEXT("Falling rejects a crouch request"), Movement->bWantsToCrouch);
    ApplyAction(Crouch, false);
    Movement->SetMovementMode(MOVE_Walking);
    ApplyAction(Crouch, true);
    TestTrue(TEXT("Grounded input requests crouch"), Movement->bWantsToCrouch);

    const FStructProperty* InputState = FindFProperty<FStructProperty>(Class, TEXT("CharacterInputState"));
    if (!TestNotNull(TEXT("Shared input state"), InputState))
    {
        return false;
    }
    TestTrue(TEXT("The authored state is replicated"), InputState->HasAnyPropertyFlags(CPF_Net));
    const auto FindIntent = [&](const TCHAR* Name)
    {
        const FBoolProperty* Property = FindFProperty<FBoolProperty>(InputState->Struct, Name);
        TestNotNull(Name, Property);
        return Property;
    };
    const FBoolProperty* WantsWalk = FindIntent(TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3"));
    const FBoolProperty* WantsSprint = FindIntent(TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936"));
    const FBoolProperty* WantsStrafe = FindIntent(TEXT("WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9"));
    const FBoolProperty* WantsAim = FindIntent(TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8"));
    const FBoolProperty* WantsCrouch = FindIntent(TEXT("WantsToCrouch_9_47D0791142F36BB93FFD9EA12F96F2B9"));
    if (!WantsWalk || !WantsSprint || !WantsStrafe || !WantsAim || !WantsCrouch)
    {
        return false;
    }
    void* State = InputState->ContainerPtrToValuePtr<void>(Character);
    WantsCrouch->SetPropertyValue_InContainer(State, true);
    ApplyAction(Walk, true);
    TestTrue(TEXT("The walk press sets walk intent"), WantsWalk->GetPropertyValue_InContainer(State));
    ApplyAction(Walk, false);
    ApplyAction(Sprint, true);
    TestTrue(TEXT("The sprint press sets sprint intent"), WantsSprint->GetPropertyValue_InContainer(State));
    TestFalse(TEXT("Sprint clears the walk toggle"), WantsWalk->GetPropertyValue_InContainer(State));
    ApplyAction(Walk, true);
    TestFalse(TEXT("Walking cannot be enabled during sprint"), WantsWalk->GetPropertyValue_InContainer(State));
    ApplyAction(Sprint, false);
    TestFalse(TEXT("The sprint release clears sprint intent"), WantsSprint->GetPropertyValue_InContainer(State));
    const bool bWasStrafing = WantsStrafe->GetPropertyValue_InContainer(State);
    ApplyAction(Strafe, true);
    TestEqual(TEXT("The strafe press toggles strafe intent"), WantsStrafe->GetPropertyValue_InContainer(State), !bWasStrafing);
    ApplyAction(Aim, true);
    TestTrue(TEXT("The aim press sets aim intent"), WantsAim->GetPropertyValue_InContainer(State));
    ApplyAction(Aim, false);
    TestFalse(TEXT("The aim release clears aim intent"), WantsAim->GetPropertyValue_InContainer(State));
    TestTrue(TEXT("The other actions preserve crouch intent"), WantsCrouch->GetPropertyValue_InContainer(State));

    UFunction* Commit = Class->FindFunctionByName(TEXT("ServerCommitInputIntent"));
    if (!TestNotNull(TEXT("Native input intent RPC"), Commit))
    {
        return false;
    }
    TestEqual(TEXT("The native RPC preserves unreliable server dispatch"),
        Commit->FunctionFlags & FUNC_NetFuncFlags, FUNC_Net | FUNC_NetServer);
    const FStructProperty* IntentParameter = FindFProperty<FStructProperty>(Commit, TEXT("Intent"));
    if (!TestNotNull(TEXT("Native input intent parameter"), IntentParameter))
    {
        return false;
    }
    FStructOnScope CallParams(Commit);
    auto& Submitted = *IntentParameter->ContainerPtrToValuePtr<FSampleCmcInputIntent>(CallParams.GetStructMemory());
    Submitted.bWantsToAim = true;
    Character->ProcessEvent(Commit, CallParams.GetStructMemory());
    TestTrue(TEXT("Authority writes submitted aim into the shared state"), WantsAim->GetPropertyValue_InContainer(State));
    TestFalse(TEXT("The RPC carries crouch intent as part of the snapshot"), WantsCrouch->GetPropertyValue_InContainer(State));
    return true;
}

#endif
