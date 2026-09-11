#include "SampleMoverCharacter.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoverDataModelTypes.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr uint8 Walk = 0;
    constexpr uint8 Run = 1;
    constexpr uint8 Sprint = 2;

    int64 ReadEnum(const FProperty& Property, const void* Value)
    {
        if (const auto* Enum = CastField<FEnumProperty>(&Property))
        {
            return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
        }
        if (const auto* Byte = CastField<FByteProperty>(&Property))
        {
            return Byte->GetPropertyValue(Value);
        }
        return 0;
    }

    const FStructProperty* FindSnapshot(const ASampleMoverCharacter& Pawn, FName Name)
    {
        return FindFProperty<FStructProperty>(Pawn.GetClass(), Name);
    }

    const FCharacterDefaultInputs* GetPreSimInputs(const ASampleMoverCharacter& Pawn)
    {
        const FStructProperty* Input = FindSnapshot(Pawn, TEXT("MoverDefaultInputs_PreSim"));
        return Input && Input->Struct == FCharacterDefaultInputs::StaticStruct()
            ? Input->ContainerPtrToValuePtr<FCharacterDefaultInputs>(&Pawn) : nullptr;
    }

    bool ReadIntent(const FStructProperty* Snapshot, const ASampleMoverCharacter& Pawn, FName Name)
    {
        const auto* Property = Snapshot ? FindFProperty<FBoolProperty>(Snapshot->Struct, Name) : nullptr;
        return Property && Property->GetPropertyValue_InContainer(Snapshot->ContainerPtrToValuePtr<void>(&Pawn));
    }
}

uint8 ASampleMoverCharacter::ResolveMovementModeValue() const
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const auto* Map = FindFProperty<FMapProperty>(GetClass(), TEXT("MovementModeMap"));
    if (!Mover || !Map || !CastField<FNameProperty>(Map->KeyProp))
    {
        return 0;
    }
    const FName Mode = Mover->GetMovementModeName();
    FScriptMapHelper Entries(Map, Map->ContainerPtrToValuePtr<void>(this));
    const uint8* Value = Entries.FindValueFromHash(&Mode);
    return Value ? static_cast<uint8>(ReadEnum(*Map->ValueProp, Value)) : 0;
}

uint8 ASampleMoverCharacter::ResolveGaitValue() const
{
    const FStructProperty* Player = FindSnapshot(*this, TEXT("PlayerInputState"));
    if (ReadIntent(Player, *this, TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936")))
    {
        const FStructProperty* Custom = FindSnapshot(*this, TEXT("MoverCustomInputs_PreSim"));
        const FProperty* Rotation = Custom ? Custom->Struct->FindPropertyByName(
            TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")) : nullptr;
        const int64 Mode = Rotation ? ReadEnum(*Rotation, Rotation->ContainerPtrToValuePtr<void>(
            Custom->ContainerPtrToValuePtr<void>(this))) : 0;
        switch (Mode)
        {
        case 0:
            return Sprint;
        case 1:
            if (const FCharacterDefaultInputs* Input = GetPreSimInputs(*this))
            {
                const int32 Style = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.StrafeStyle"));
                const double Threshold = Style == 1 || Style == 2 ? -0.1 : 0.5;
                return FVector::DotProduct(Input->GetMoveInput(), Input->OrientationIntent) > Threshold ? Sprint : Run;
            }
            return Run;
        case 2:
            return Run;
        default:
            return Walk;
        }
    }
    if (ReadIntent(Player, *this, TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3")))
    {
        return Walk;
    }
    const int32 AnalogStyle = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.AnalogInputStyle"));
    if (AnalogStyle == 0)
    {
        return Run;
    }
    const auto* Amount = FindFProperty<FNumericProperty>(GetClass(), TEXT("SmoothedAnalogInputAmount"));
    const double Deflection = Amount && Amount->IsFloatingPoint()
        ? Amount->GetFloatingPointPropertyValue(Amount->ContainerPtrToValuePtr<void>(this)) : 0.0;
    return AnalogStyle == 1 && Deflection > 0.7 ? Run : Walk;
}

double ASampleMoverCharacter::Get_Speed() const
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    return Mover ? Mover->GetVelocity().Size2D() : 0.0;
}

void ASampleMoverCharacter::CustomTeleport(FTransform Transform, bool Offset, bool UpdateInitialTransform)
{
    if (Offset)
    {
        AddActorWorldTransform(Transform);
    }
    else
    {
        TeleportTo(Transform.GetLocation(), Transform.Rotator());
    }
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        Mover->QueueNextMode(TEXT("Falling"));
    }
    const FStructProperty* Inputs = FindSnapshot(*this, TEXT("MoverDefaultInputs_PreSim"));
    if (Inputs && Inputs->Struct == FCharacterDefaultInputs::StaticStruct())
    {
        auto* State = Inputs->ContainerPtrToValuePtr<FCharacterDefaultInputs>(this);
        const FVector Direction = State->OrientationIntent.Rotation().Vector();
        State->OrientationIntent = Offset ? Transform.GetRotation().RotateVector(Direction)
            : Transform.GetRotation().GetForwardVector();
    }
    if (UpdateInitialTransform)
    {
        const FStructProperty* Initial = FindSnapshot(*this, TEXT("InitialTransform"));
        if (Initial && Initial->Struct == TBaseStructure<FTransform>::Get())
        {
            *Initial->ContainerPtrToValuePtr<FTransform>(this) = GetActorTransform();
        }
    }
}
