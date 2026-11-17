#if WITH_DEV_AUTOMATION_TESTS

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MoverDataModelTypes.h"
#include "Camera/SampleCameraStateLibrary.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCameraStateTest,
    "GameAnimationSampleCpp.Migration.CameraCharacterState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCameraStateTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    const auto Spawn = [World](const TCHAR* ClassPath)
    {
        UClass* Class = LoadClass<APawn>(nullptr, ClassPath);
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return Class ? World->SpawnActor<APawn>(Class, FTransform::Identity, SpawnParams) : nullptr;
    };
    const auto Verify = [this](APawn& Pawn, uint8 Style, uint8 Mode, uint8 Gait, uint8 Stance, FName MovementMode)
    {
        UFunction* Query = Pawn.FindFunction(TEXT("Get_PropertiesForCamera"));
        const auto* Return = Query ? CastField<FStructProperty>(Query->GetReturnProperty()) : nullptr;
        if (!TestTrue(TEXT("Pawn camera interface returns its authored struct"), Return &&
            Return->Struct->GetFName() == TEXT("S_CharacterPropertiesForCamera")))
        {
            return;
        }
        FStructOnScope Args(Query);
        Pawn.ProcessEvent(Query, Args.GetStructMemory());
        const void* Data = Return->ContainerPtrToValuePtr<void>(Args.GetStructMemory());
        const auto Check = [this, &Pawn, Return, Data](FName Name, uint8 Expected)
        {
            const auto* Field = FindFProperty<FByteProperty>(Return->Struct, Name);
            if (TestNotNull(Name.ToString(), Field))
            {
                TestEqual(Pawn.GetName() + TEXT(" ") + Name.ToString(),
                    static_cast<int32>(Field->GetPropertyValue_InContainer(Data)), static_cast<int32>(Expected));
            }
        };
        Check(TEXT("CameraStyle_9_3AB8E4E240527073591DBC8A5CC26A59"), Style);
        Check(TEXT("CameraMode_6_A59B5561435DC3553D7444B49C47015A"), Mode);
        Check(TEXT("Gait_23_479D639548BC1B9BF402EEAB5E71E3C3"), Gait);
        Check(TEXT("Stance_10_65A83BF64BE8EDFA2FA9FC817B1ECD53"), Stance);
        const auto* Field = FindFProperty<FByteProperty>(Return->Struct,
            TEXT("MovementMode_29_9179014C47EBE85BE880F78CB6E2E69D"));
        const UEnum* Enum = Field ? Field->GetIntPropertyEnum() : nullptr;
        if (TestNotNull(TEXT("Camera movement mode enum"), Enum))
        {
            TestEqual(Pawn.GetName() + TEXT(" camera movement mode"),
                Enum->GetNameStringByValue(Field->GetPropertyValue_InContainer(Data)), MovementMode.ToString());
        }
    };

    auto* Cmc = Cast<ACharacter>(Spawn(TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C")));
    const auto* CmcGait = Cmc ? FindFProperty<FByteProperty>(Cmc->GetClass(), TEXT("Gait")) : nullptr;
    const auto* Style = Cmc ? FindFProperty<FByteProperty>(Cmc->GetClass(), TEXT("CameraStyle")) : nullptr;
    const auto* Input = Cmc ? FindFProperty<FStructProperty>(Cmc->GetClass(), TEXT("CharacterInputState")) : nullptr;
    const auto* Aim = Input ? FindFProperty<FBoolProperty>(Input->Struct,
        TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8")) : nullptr;
    const auto* Strafe = Input ? FindFProperty<FBoolProperty>(Input->Struct,
        TEXT("WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9")) : nullptr;
    if (!TestTrue(TEXT("CMC camera inputs"), Cmc && CmcGait && Style && Aim && Strafe))
    {
        return false;
    }
    void* InputData = Input->ContainerPtrToValuePtr<void>(Cmc);
    for (uint8 Mask = 0; Mask < 4; ++Mask)
    {
        const bool bAim = (Mask & 1) != 0;
        const bool bStrafe = (Mask & 2) != 0;
        Aim->SetPropertyValue_InContainer(InputData, bAim);
        Strafe->SetPropertyValue_InContainer(InputData, bStrafe);
        CmcGait->SetPropertyValue_InContainer(Cmc, Mask % 3);
        Style->SetPropertyValue_InContainer(Cmc, (Mask + 1) % 3);
        Cmc->bIsCrouched = bStrafe;
        Cmc->GetCharacterMovement()->MovementMode = bAim ? MOVE_Falling : MOVE_Walking;
        Verify(*Cmc, (Mask + 1) % 3, bAim ? 2 : bStrafe ? 1 : 0, Mask % 3, bStrafe ? 1 : 0,
            TEXT("NewEnumerator4"));
    }

    APawn* Mover = Spawn(TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
    auto* CharacterMover = Mover ? Mover->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
    const auto* Pre = Mover ? FindFProperty<FStructProperty>(Mover->GetClass(), TEXT("MoverCustomInputs_PreSim")) : nullptr;
    const auto* Post = Mover ? FindFProperty<FStructProperty>(Mover->GetClass(), TEXT("MoverCustomInputs_PostSim")) : nullptr;
    const auto* TwinStick = Mover ? FindFProperty<FBoolProperty>(Mover->GetClass(), TEXT("TwinStickMode")) : nullptr;
    const auto* Gait = Post ? FindFProperty<FByteProperty>(Post->Struct,
        TEXT("Gait_5_13BB29FB446593C2412547A541C4C00B")) : nullptr;
    const auto* Rotation = Post ? FindFProperty<FByteProperty>(Post->Struct,
        TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")) : nullptr;
    const auto* Component = Mover ? FindFProperty<FObjectPropertyBase>(Mover->GetClass(), TEXT("CharacterMover")) : nullptr;
    if (!TestTrue(TEXT("Mover camera inputs"), CharacterMover && Pre && Post && Pre->SameType(Post) &&
        TwinStick && Gait && Rotation && Component))
    {
        return false;
    }
    for (const bool bFalling : {false, true})
    {
        FMoverSyncState State = CharacterMover->GetSyncState();
        State.MovementMode = bFalling ? TEXT("Falling") : TEXT("Walking");
        State.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
        CharacterMover->OnSimulationStateInitialized(State, FMoverAuxStateContext());
        for (uint8 Value = 0; Value < 3; ++Value)
        {
            void* PreData = Pre->ContainerPtrToValuePtr<void>(Mover);
            void* PostData = Post->ContainerPtrToValuePtr<void>(Mover);
            Gait->SetPropertyValue_InContainer(PreData, (Value + 1) % 3);
            Rotation->SetPropertyValue_InContainer(PreData, (Value + 2) % 3);
            Gait->SetPropertyValue_InContainer(PostData, Value);
            Rotation->SetPropertyValue_InContainer(PostData, Value);
            for (const bool bTwinStick : {false, true})
            {
                TwinStick->SetPropertyValue_InContainer(Mover, bTwinStick);
                Verify(*Mover, 2, bTwinStick ? 3 : Value, Value, 0,
                    bFalling ? TEXT("NewEnumerator5") : TEXT("NewEnumerator4"));
            }
        }
    }
    UObject* MovementReference = Component->GetObjectPropertyValue_InContainer(Mover);
    Component->SetObjectPropertyValue_InContainer(Mover, nullptr);
    Verify(*Mover, 2, 0, 0, 0, TEXT("NewEnumerator4"));
    Component->SetObjectPropertyValue_InContainer(Mover, MovementReference);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCameraSelectionPolicyTest,
    "GameAnimationSampleCpp.Migration.CameraSelectionPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCameraSelectionPolicyTest::RunTest(const FString& Parameters)
{
    UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr,
        TEXT("/Game/Blueprints/Data/S_CharacterPropertiesForCamera.S_CharacterPropertiesForCamera"));
    if (!TestNotNull(TEXT("Imported camera properties struct"), Struct))
    {
        return false;
    }

    const FName FieldNames[] = {
        TEXT("CameraStyle_9_3AB8E4E240527073591DBC8A5CC26A59"),
        TEXT("CameraMode_6_A59B5561435DC3553D7444B49C47015A"),
        TEXT("Gait_23_479D639548BC1B9BF402EEAB5E71E3C3"),
        TEXT("Stance_10_65A83BF64BE8EDFA2FA9FC817B1ECD53"),
        TEXT("MovementMode_29_9179014C47EBE85BE880F78CB6E2E69D")
    };
    const uint8 InitialValues[] = {2, 3, 2, 1, 5};
    const FByteProperty* Fields[UE_ARRAY_COUNT(FieldNames)] = {};
    FStructOnScope Source(Struct);
    FStructOnScope Selection(Struct);
    void* SourceData = Source.GetStructMemory();
    void* SelectionData = Selection.GetStructMemory();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(FieldNames); ++Index)
    {
        Fields[Index] = FindFProperty<FByteProperty>(Struct, FieldNames[Index]);
        if (!TestNotNull(FieldNames[Index].ToString(), Fields[Index]))
        {
            return false;
        }
        Fields[Index]->SetPropertyValue_InContainer(SourceData, InitialValues[Index]);
    }
    const UEnum* StyleEnum = Fields[0]->GetIntPropertyEnum();
    if (!TestNotNull(TEXT("Imported camera style enum"), StyleEnum))
    {
        return false;
    }

    const int32 Styles[] = {MIN_int32, -257, -256, -1, 0, 1, 2, 3, 4, 5, 255, 256, 257, 259, MAX_int32};
    for (const int32 Style : Styles)
    {
        const FString Context = FString::Printf(TEXT("Camera style %d: "), Style);
        const uint8 Byte = static_cast<uint8>(Style);
        const uint8 ExpectedStyle = StyleEnum->IsValidEnumValueOrBitfield(Byte) ? Byte :
            static_cast<uint8>(StyleEnum->GetMaxEnumValue());
        TestTrue(Context + TEXT("prepares selection"),
            USampleCameraStateLibrary::BuildCameraSelection(*Struct, SourceData, SelectionData, Style));
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(FieldNames); ++Index)
        {
            TestEqual(Context + TEXT("cached state ") + FieldNames[Index].ToString(),
                Fields[Index]->GetPropertyValue_InContainer(SourceData), InitialValues[Index]);
            TestEqual(Context + TEXT("selection state ") + FieldNames[Index].ToString(),
                Fields[Index]->GetPropertyValue_InContainer(SelectionData), Index == 0 ? ExpectedStyle : InitialValues[Index]);
        }
        TestEqual(Context + TEXT("crouch eligibility uses original integer"),
            USampleCameraStateLibrary::IsCrouchRigEligible(Style, 1), Style != 3);
        TestFalse(Context + TEXT("standing excludes crouch rig"),
            USampleCameraStateLibrary::IsCrouchRigEligible(Style, 0));
        TestFalse(Context + TEXT("unknown stance excludes crouch rig"),
            USampleCameraStateLibrary::IsCrouchRigEligible(Style, 255));
    }

    TestFalse(TEXT("Missing source storage is rejected"),
        USampleCameraStateLibrary::BuildCameraSelection(*Struct, nullptr, SelectionData, 0));
    TestFalse(TEXT("Missing output storage is rejected"),
        USampleCameraStateLibrary::BuildCameraSelection(*Struct, SourceData, nullptr, 0));
    TestTrue(TEXT("Aliased initialized storage is supported"),
        USampleCameraStateLibrary::BuildCameraSelection(*Struct, SourceData, SourceData, 1));
    const uint8 ExpectedAliasStyle = StyleEnum->IsValidEnumValueOrBitfield(1) ? 1 :
        static_cast<uint8>(StyleEnum->GetMaxEnumValue());
    TestEqual(TEXT("Aliased storage receives selected style"),
        Fields[0]->GetPropertyValue_InContainer(SourceData), ExpectedAliasStyle);
    for (int32 Index = 1; Index < UE_ARRAY_COUNT(FieldNames); ++Index)
    {
        TestEqual(TEXT("Aliased non-style field ") + FieldNames[Index].ToString(),
            Fields[Index]->GetPropertyValue_InContainer(SourceData), InitialValues[Index]);
    }
    return true;
}

#endif
