#include "../SampleCmcCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
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

#endif
