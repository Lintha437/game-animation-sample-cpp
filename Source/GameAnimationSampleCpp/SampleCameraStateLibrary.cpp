#include "SampleCameraStateLibrary.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SampleCmcCharacter.h"
#include "SampleMoverCharacter.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/UnrealType.h"

namespace
{
    void WriteCameraState(const UScriptStruct& Struct, void* Data, uint8 Mode, uint8 Gait, bool bCrouching)
    {
        if (const auto* Field = FindFProperty<FByteProperty>(&Struct, TEXT("CameraMode_6_A59B5561435DC3553D7444B49C47015A")))
        {
            Field->SetPropertyValue_InContainer(Data, Mode);
        }
        if (const auto* Field = FindFProperty<FByteProperty>(&Struct, TEXT("Gait_23_479D639548BC1B9BF402EEAB5E71E3C3")))
        {
            Field->SetPropertyValue_InContainer(Data, Gait);
        }
        if (const auto* Field = FindFProperty<FByteProperty>(&Struct, TEXT("Stance_10_65A83BF64BE8EDFA2FA9FC817B1ECD53")))
        {
            Field->SetPropertyValue_InContainer(Data, bCrouching ? 1 : 0);
        }
    }

    void SampleCameraState(const UUserDefinedStruct& Struct, void* Data, const APawn* Pawn)
    {
        // CMC leaves movement mode at its authored default. Mover does the same for camera style.
        Struct.CopyScriptStruct(Data, Struct.GetDefaultInstance());
        if (const auto* Cmc = Cast<ASampleCmcCharacter>(Pawn))
        {
            const auto* Movement = Cmc->GetCharacterMovement();
            WriteCameraState(Struct, Data, Cmc->GetAuthoredCameraModeValue(), Cmc->GetAuthoredGaitValue(),
                Movement && Movement->IsCrouching());
            if (const auto* Field = FindFProperty<FByteProperty>(&Struct, TEXT("CameraStyle_9_3AB8E4E240527073591DBC8A5CC26A59")))
            {
                Field->SetPropertyValue_InContainer(Data, Cmc->GetAuthoredCameraStyleValue());
            }
        }
        else if (const auto* Mover = Cast<ASampleMoverCharacter>(Pawn))
        {
            const auto* Field = FindFProperty<FObjectPropertyBase>(Mover->GetClass(), TEXT("CharacterMover"));
            const auto* Movement = Field ? Cast<UCharacterMoverComponent>(Field->GetObjectPropertyValue_InContainer(Mover)) : nullptr;
            if (!IsValid(Movement))
            {
                return;
            }
            WriteCameraState(Struct, Data, Mover->GetAuthoredCameraModeValue(), Mover->GetAuthoredGaitValue(),
                Movement->IsCrouching());
            if (const auto* Mode = FindFProperty<FByteProperty>(&Struct, TEXT("MovementMode_29_9179014C47EBE85BE880F78CB6E2E69D")))
            {
                Mode->SetPropertyValue_InContainer(Data, Mover->ResolveMovementModeValue());
            }
        }
    }
}

void USampleCameraStateLibrary::BuildImportedCameraProperties(const APawn*, int32&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(USampleCameraStateLibrary::execBuildImportedCameraProperties)
{
    P_GET_OBJECT(APawn, Pawn);
    Stack.MostRecentProperty = nullptr;
    Stack.MostRecentPropertyAddress = nullptr;
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const auto* Property = CastField<FStructProperty>(Stack.MostRecentProperty);
    const auto* Struct = Property ? Cast<UUserDefinedStruct>(Property->Struct.Get()) : nullptr;
    void* Data = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    if (Data && Struct && Struct->GetFName() == TEXT("S_CharacterPropertiesForCamera"))
    {
        SampleCameraState(*Struct, Data, Pawn);
    }
    P_NATIVE_END;
}
