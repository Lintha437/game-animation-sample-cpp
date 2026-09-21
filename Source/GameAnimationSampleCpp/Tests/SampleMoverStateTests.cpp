#include "../SampleMoverCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MoverDataModelTypes.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleMoverStateMigrationTest,
    "GameAnimationSampleCpp.Migration.MoverState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleMoverStateMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleMoverCharacter>(nullptr,
        TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
    if (!TestNotNull(TEXT("Saved Mover pawn"), Class))
    {
        return false;
    }
    for (const FName Name : {FName(TEXT("Get_Speed")), FName(TEXT("CustomTeleport"))})
    {
        const UFunction* Function = Class->FindFunctionByName(Name);
        TestTrue(Name.ToString(), Function && Function->GetOuter() == ASampleMoverCharacter::StaticClass());
    }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    ASampleMoverCharacter* Pawn = World->SpawnActor<ASampleMoverCharacter>(Class);
    if (!TestNotNull(TEXT("Spawned Mover pawn"), Pawn) ||
        !TestNotNull(TEXT("Authored Mover component"), Pawn->FindComponentByClass<UCharacterMoverComponent>()))
    {
        return false;
    }
    const FStructProperty* Input = FindFProperty<FStructProperty>(Class, TEXT("MoverDefaultInputs_PreSim"));
    const FStructProperty* Player = FindFProperty<FStructProperty>(Class, TEXT("PlayerInputState"));
    const FStructProperty* Custom = FindFProperty<FStructProperty>(Class, TEXT("MoverCustomInputs_PreSim"));
    const FStructProperty* Initial = FindFProperty<FStructProperty>(Class, TEXT("InitialTransform"));
    if (!Input || Input->Struct != FCharacterDefaultInputs::StaticStruct() || !Player || !Custom || !Initial)
    {
        AddError(TEXT("Required sample snapshot fields are missing."));
        return false;
    }
    const auto* Sprint = FindFProperty<FBoolProperty>(Player->Struct, TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936"));
    const auto* Walk = FindFProperty<FBoolProperty>(Player->Struct, TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3"));
    const auto* Rotation = FindFProperty<FByteProperty>(Custom->Struct, TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9"));
    UFunction* GetGait = Class->FindFunctionByName(TEXT("Get_Gait"));
    const auto* Gait = GetGait ? CastField<FByteProperty>(GetGait->GetReturnProperty()) : nullptr;
    const auto* Deflection = FindFProperty<FNumericProperty>(Class, TEXT("SmoothedAnalogInputAmount"));
    if (!Sprint || !Walk || !Rotation || !Gait || !Gait->Enum || !Deflection)
    {
        AddError(TEXT("Gait, rotation, intent, or deflection type has changed."));
        return false;
    }
    FStructOnScope GaitParameters(GetGait);
    const auto ReadGait = [&]()
    {
        Pawn->ProcessEvent(GetGait, GaitParameters.GetStructMemory());
        return Gait->GetPropertyValue_InContainer(GaitParameters.GetStructMemory());
    };
    void* PlayerData = Player->ContainerPtrToValuePtr<void>(Pawn);
    void* RotationData = Custom->ContainerPtrToValuePtr<void>(Pawn);
    auto* State = Input->ContainerPtrToValuePtr<FCharacterDefaultInputs>(Pawn);
    Walk->SetPropertyValue_InContainer(PlayerData, true);
    TestEqual(TEXT("The typed query follows the walk toggle"), ReadGait(), uint8{0});
    Sprint->SetPropertyValue_InContainer(PlayerData, true);
    Rotation->SetPropertyValue_InContainer(RotationData, 0);
    TestEqual(TEXT("Facing movement permits sprint over walk"), ReadGait(), uint8{2});
    Rotation->SetPropertyValue_InContainer(RotationData, 2);
    TestEqual(TEXT("Aim limits sprint to run"), ReadGait(), uint8{1});

    Rotation->SetPropertyValue_InContainer(RotationData, 1);
    State->OrientationIntent = FVector::ForwardVector;
    State->SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ForwardVector);
    TestEqual(TEXT("Forward strafe permits sprint"), ReadGait(), uint8{2});
    State->SetMoveInput(EMoveInputType::DirectionalIntent, -FVector::ForwardVector);
    TestEqual(TEXT("Backward strafe limits sprint to run"), ReadGait(), uint8{1});
    Sprint->SetPropertyValue_InContainer(PlayerData, false);
    Walk->SetPropertyValue_InContainer(PlayerData, false);
    IConsoleVariable* AnalogStyle = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.AnalogInputStyle"));
    if (TestNotNull(TEXT("Authored analog style CVar"), AnalogStyle))
    {
        const int32 Original = AnalogStyle->GetInt();
        const auto Priority = static_cast<EConsoleVariableFlags>(AnalogStyle->GetFlags() & ECVF_SetByMask);
        ON_SCOPE_EXIT { AnalogStyle->Set(Original, Priority); };
        AnalogStyle->Set(1, Priority);
        Deflection->SetFloatingPointPropertyValue(Deflection->ContainerPtrToValuePtr<void>(Pawn), 0.6);
        TestEqual(TEXT("Small deflection is walk"), ReadGait(), uint8{0});
        Deflection->SetFloatingPointPropertyValue(Deflection->ContainerPtrToValuePtr<void>(Pawn), 0.8);
        TestEqual(TEXT("Large deflection is run"), ReadGait(), uint8{1});
    }

    auto* Mover = Pawn->FindComponentByClass<UCharacterMoverComponent>();
    FMoverSyncState GroundedState = Mover->GetSyncState();
    GroundedState.MovementMode = TEXT("Walking");
    Mover->OnSimulationStateInitialized(GroundedState, FMoverAuxStateContext());
    TestEqual(TEXT("The orientation fixture is grounded"), Pawn->ResolveMovementModeValue(), uint8{0});
    Rotation->SetPropertyValue_InContainer(RotationData, 0);
    State->SetMoveInput(EMoveInputType::DirectionalIntent, FVector::RightVector * 0.5);
    TestEqual(TEXT("Ground facing preserves the directional input"), Pawn->ResolveOrientationIntent(), FVector::RightVector * 0.5);
    State->SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
    State->OrientationIntent = FVector::ForwardVector;
    Pawn->CaptureBasedMovement(FTransform(FRotator(0.0, 90.0, 0.0)), FMoverTimeStep());
    const FVector BasedFacing = Pawn->ResolveOrientationIntent();
    TestTrue(*FString::Printf(TEXT("Stationary facing follows the moving base (%s)"), *BasedFacing.ToString()),
        BasedFacing.Equals(FVector::RightVector));

    const FTransform Target(FRotator(0.0, 90.0, 0.0), FVector(500.0, 250.0, 100.0));
    Pawn->CustomTeleport(Target, false, true);
    TestTrue(TEXT("Absolute teleport reaches the destination"), Pawn->GetActorLocation().Equals(Target.GetLocation()));
    TestTrue(TEXT("Absolute destination becomes reset transform"),
        Initial->ContainerPtrToValuePtr<FTransform>(Pawn)->Equals(Pawn->GetActorTransform()));
    TestTrue(TEXT("Absolute teleport updates the orientation intent"),
        State->OrientationIntent.Equals(FVector::RightVector));
    const FTransform Offset(FRotator(0.0, 90.0, 0.0), FVector(25.0, 0.0, 0.0));
    Pawn->CustomTeleport(Offset, true, false);
    TestTrue(TEXT("Relative teleport rotates the cached facing"), State->OrientationIntent.Equals(-FVector::ForwardVector));
    TestTrue(TEXT("Relative teleport preserves the reset point when disabled"),
        Initial->ContainerPtrToValuePtr<FTransform>(Pawn)->GetLocation().Equals(Target.GetLocation()));

    for (const FName Name : {FName(TEXT("UpdateCachedMoverInputs")), FName(TEXT("ApplyMoverCrouchInput")),
        FName(TEXT("ResolveAimingRotation")), FName(TEXT("UpdateControlYawRate"))})
    {
        const UFunction* Function = Class->FindFunctionByName(Name);
        TestTrue(Name.ToString(), Function && Function->GetOuter() == ASampleMoverCharacter::StaticClass());
    }
    UFunction* Produce = Class->FindFunctionByName(TEXT("ProduceInput"));
    const auto* Command = Produce ? FindFProperty<FStructProperty>(Produce, TEXT("InputCmdResult")) : nullptr;
    const auto* Jump = FindFProperty<FBoolProperty>(Class, TEXT("Jump_JustPressed"));
    const auto* PlayerCrouch = FindFProperty<FBoolProperty>(Player->Struct, TEXT("WantsToCrouch_9_47D0791142F36BB93FFD9EA12F96F2B9"));
    const auto* CommandCrouch = FindFProperty<FBoolProperty>(Custom->Struct, TEXT("WantsToCrouch_28_911DFFDE4A6B03A7346BE8A33869E3D6"));
    if (!Command || Command->Struct != FMoverInputCmdContext::StaticStruct() || !Jump || !PlayerCrouch || !CommandCrouch)
    {
        AddError(TEXT("The authored Mover input interface or intent fields are missing."));
        return false;
    }
    Jump->SetPropertyValue_InContainer(Pawn, true);
    PlayerCrouch->SetPropertyValue_InContainer(PlayerData, true);
    FStructOnScope ProduceParameters(Produce);
    Pawn->ProcessEvent(Produce, ProduceParameters.GetStructMemory());
    const FMoverDataCollection& Collection = Command->ContainerPtrToValuePtr<FMoverInputCmdContext>(
        ProduceParameters.GetStructMemory())->InputCollection;
    const FCharacterDefaultInputs* DefaultCommand = Collection.FindDataByType<FCharacterDefaultInputs>();
    if (TestNotNull(TEXT("The original interface supplies default input"), DefaultCommand))
    {
        TestTrue(TEXT("Jump edge is included in the default block"), DefaultCommand->bIsJumpJustPressed);
        TestEqual(TEXT("The command carries directional intent"), DefaultCommand->GetMoveInputType(), EMoveInputType::DirectionalIntent);
    }
    const auto* CustomCommand = static_cast<const FMoverUserDefinedDataStruct*>(Collection.FindDataByType(Custom->Struct));
    if (TestNotNull(TEXT("The command preserves the imported custom block"), CustomCommand))
    {
        TestTrue(TEXT("Crouch intent is included in the custom block"),
            CommandCrouch->GetPropertyValue_InContainer(CustomCommand->StructInstance.GetMemory()));
        PlayerCrouch->SetPropertyValue_InContainer(PlayerData, false);
        TestTrue(TEXT("Subsequent device input does not modify an issued command"),
            CommandCrouch->GetPropertyValue_InContainer(CustomCommand->StructInstance.GetMemory()));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleMoverDeviceInputTest,
    "GameAnimationSampleCpp.Migration.MoverDeviceInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleMoverDeviceInputTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleMoverCharacter>(nullptr,
        TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
    if (!TestNotNull(TEXT("Saved native Mover pawn"), Class))
    {
        return false;
    }
    const auto* Enabled = FindFProperty<FBoolProperty>(Class, TEXT("bUseNativeMoverDeviceInput"));
    TestTrue(TEXT("Saved pawn enables the native bindings"), Enabled && Enabled->GetPropertyValue_InContainer(Class->GetDefaultObject()));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    ASampleMoverCharacter* Pawn = World->SpawnActor<ASampleMoverCharacter>(Class);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Mover pawn"), Pawn) || !TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }
    Controller->Player = NewObject<ULocalPlayer>(GEngine);
    Controller->Possess(Pawn);
    Pawn->PawnClientRestart();
    auto* Input = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
    if (!TestNotNull(TEXT("Possession creates Enhanced Input"), Input))
    {
        return false;
    }

    auto* PlayerInput = NewObject<UEnhancedPlayerInput>(Controller);
    Controller->PlayerInput = PlayerInput;
    const TArray<UInputComponent*> Stack{Input};
    const auto Apply = [&](const TCHAR* Name, const FInputActionValue& Value)
    {
        const FString Path = FString::Printf(TEXT("/Game/Input/%s.%s"), Name, Name);
        if (const auto* Action = LoadObject<UInputAction>(nullptr, *Path); TestNotNull(Name, Action))
        {
            PlayerInput->InjectInputForAction(Action, Value);
            PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
        }
    };

    Controller->SetControlRotation(FRotator(0.0, 90.0, 0.0));
    Apply(TEXT("IA_Move"), FVector2D(0.0, 0.5));
    TestTrue(TEXT("Device move is normalized in the control yaw frame"),
        Pawn->ResolveMoverDirection().Equals(FVector::RightVector));
    Apply(TEXT("IA_Strafe"), true);
    TestEqual(TEXT("Strafe toggles the authored facing mode"), Pawn->ResolveMoverRotationMode(), uint8{1});
    Apply(TEXT("IA_Aim"), true);
    TestEqual(TEXT("Aim takes priority over strafe"), Pawn->ResolveMoverRotationMode(), uint8{2});
    return true;
}

#endif
