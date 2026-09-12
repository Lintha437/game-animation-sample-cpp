#include "SampleMoverCharacter.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoverDataModelTypes.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

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

    template <typename ValueType>
    ValueType ReadStructResult(ASampleMoverCharacter& Pawn, FName Name, ValueType Fallback)
    {
        UFunction* Function = Pawn.FindFunction(Name);
        const auto* Result = Function ? CastField<FStructProperty>(Function->GetReturnProperty()) : nullptr;
        if (!Result || Result->Struct != TBaseStructure<ValueType>::Get())
        {
            return Fallback;
        }
        FStructOnScope Args(Function);
        Pawn.ProcessEvent(Function, Args.GetStructMemory());
        return *Result->ContainerPtrToValuePtr<ValueType>(Args.GetStructMemory());
    }

    void CopyEnum(const FProperty* Target, void* Destination, const FProperty* Source, const void* SourceContainer)
    {
        if (!Target || !Source || !Target->SameType(Source))
        {
            return;
        }
        Target->CopyCompleteValue(Target->ContainerPtrToValuePtr<void>(Destination),
            Source->ContainerPtrToValuePtr<void>(SourceContainer));
    }

    void SetEnumByte(const UScriptStruct* Type, void* Data, FName Name, uint8 Value)
    {
        if (const auto* Byte = FindFProperty<FByteProperty>(Type, Name))
        {
            Byte->SetPropertyValue_InContainer(Data, Value);
        }
        else if (const auto* Enum = FindFProperty<FEnumProperty>(Type, Name))
        {
            Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(Data),
                static_cast<uint64>(Value));
        }
    }

    double ReadNumber(const UStruct* Type, const void* Data, FName Name)
    {
        const auto* Number = FindFProperty<FNumericProperty>(Type, Name);
        return Number && Number->IsFloatingPoint()
            ? Number->GetFloatingPointPropertyValue(Number->ContainerPtrToValuePtr<void>(Data)) : 0.0;
    }

    void WriteNumber(const UStruct* Type, void* Data, FName Name, double Value)
    {
        const auto* Number = FindFProperty<FNumericProperty>(Type, Name);
        if (Number && Number->IsFloatingPoint())
        {
            Number->SetFloatingPointPropertyValue(Number->ContainerPtrToValuePtr<void>(Data), Value);
        }
    }

    const uint8* FindCustomInput(const FMoverDataCollection& Collection, const UScriptStruct* Type)
    {
        if (!Type || !Type->IsA<UUserDefinedStruct>())
        {
            return nullptr;
        }
        const FMoverDataStructBase* Data = Collection.FindDataByType(Type);
        return Data ? static_cast<const FMoverUserDefinedDataStruct*>(Data)->StructInstance.GetMemory() : nullptr;
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
    const double Deflection = ReadNumber(GetClass(), this, TEXT("SmoothedAnalogInputAmount"));
    return AnalogStyle == 1 && Deflection > 0.7 ? Run : Walk;
}

FMoverInputCmdContext ASampleMoverCharacter::BuildMoverInputCommand(int32 SimTimeMs)
{
    FMoverInputCmdContext Command;
    const auto* Defaults = FindSnapshot(*this, TEXT("MoverDefaultInputs_PreSim"));
    const auto* Custom = FindSnapshot(*this, TEXT("MoverCustomInputs_PreSim"));
    if (!Defaults || Defaults->Struct != FCharacterDefaultInputs::StaticStruct() ||
        !Custom || !Custom->Struct->IsA<UUserDefinedStruct>())
    {
        return Command;
    }
    auto* Input = Defaults->ContainerPtrToValuePtr<FCharacterDefaultInputs>(this);
    UScriptStruct* CustomType = Custom->Struct;
    void* CustomData = Custom->ContainerPtrToValuePtr<void>(this);
    Input->SetMoveInput(EMoveInputType::DirectionalIntent,
        ReadStructResult(*this, TEXT("Get_MoveInput"), FVector::ZeroVector));
    Input->ControlRotation = ResolveAimingRotation();
    const auto* Jump = FindFProperty<FBoolProperty>(GetClass(), TEXT("Jump_JustPressed"));
    Input->bIsJumpJustPressed = Jump && Jump->GetPropertyValue_InContainer(this);
    if (UFunction* Rotation = FindFunction(TEXT("Get_RotationMode")))
    {
        FStructOnScope Args(Rotation);
        ProcessEvent(Rotation, Args.GetStructMemory());
        CopyEnum(CustomType->FindPropertyByName(TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")),
            CustomData, Rotation->GetReturnProperty(), Args.GetStructMemory());
    }
    Input->OrientationIntent = ReadStructResult(*this, TEXT("Get_OrientationIntent"), FVector::ZeroVector);
    SetEnumByte(CustomType, CustomData, TEXT("Gait_5_13BB29FB446593C2412547A541C4C00B"), ResolveGaitValue());
    WriteNumber(CustomType, CustomData, TEXT("ControlRotationRate_29_A6F8894147153250CDA5C889CD9DB171"),
        ReadNumber(GetClass(), this, TEXT("ControlRotationRate")));
    const auto* Crouch = FindFProperty<FBoolProperty>(CustomType, TEXT("WantsToCrouch_28_911DFFDE4A6B03A7346BE8A33869E3D6"));
    if (Crouch)
    {
        Crouch->SetPropertyValue_InContainer(CustomData, ReadIntent(FindSnapshot(*this, TEXT("PlayerInputState")),
            *this, TEXT("WantsToCrouch_9_47D0791142F36BB93FFD9EA12F96F2B9")));
    }
    if (UFunction* Direction = FindFunction(TEXT("Get_MovementDirectionAndOffset")))
    {
        FStructOnScope Args(Direction);
        ProcessEvent(Direction, Args.GetStructMemory());
        CopyEnum(CustomType->FindPropertyByName(TEXT("MovementDirection_2_19B243824010A37C0D54A7A28A5C6EFA")),
            CustomData, Direction->FindPropertyByName(TEXT("MovementDirection")), Args.GetStructMemory());
        WriteNumber(CustomType, CustomData, TEXT("RotationOffset_14_9438753848B6A37A4F76569318195C60"),
            ReadNumber(Direction, Args.GetStructMemory(), TEXT("RotationOffset")));
    }

    Command.InputCollection.AddDataByCopy(Input);
    auto* Wrapped = static_cast<FMoverUserDefinedDataStruct*>(Command.InputCollection.FindOrAddDataByType(CustomType));
    Wrapped->InitializeAs(CustomType, static_cast<const uint8*>(CustomData));
    return Command;
}

void ASampleMoverCharacter::UpdateCachedMoverInputs()
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (!Mover)
    {
        return;
    }
    const FMoverDataCollection& Collection = Mover->GetLastInputCmd().InputCollection;
    if (const FCharacterDefaultInputs* Input = Collection.FindDataByType<FCharacterDefaultInputs>())
    {
        const auto* Defaults = FindSnapshot(*this, TEXT("MoverDefaultInputs_PostSim"));
        if (Defaults && Defaults->Struct == FCharacterDefaultInputs::StaticStruct())
        {
            *Defaults->ContainerPtrToValuePtr<FCharacterDefaultInputs>(this) = *Input;
        }
    }
    if (const auto* Custom = FindSnapshot(*this, TEXT("MoverCustomInputs_PostSim")))
    {
        if (const uint8* Data = FindCustomInput(Collection, Custom->Struct))
        {
            Custom->Struct->CopyScriptStruct(Custom->ContainerPtrToValuePtr<void>(this), Data);
        }
    }
}

void ASampleMoverCharacter::ApplyMoverCrouchInput(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        const auto* Custom = FindSnapshot(*this, TEXT("MoverCustomInputs_PreSim"));
        const void* Data = Custom ? FindCustomInput(InputCmd.InputCollection, Custom->Struct) : nullptr;
        const auto* Crouch = Data ? FindFProperty<FBoolProperty>(Custom->Struct,
            TEXT("WantsToCrouch_28_911DFFDE4A6B03A7346BE8A33869E3D6")) : nullptr;
        if (Crouch && Crouch->GetPropertyValue_InContainer(Data))
        {
            Mover->Crouch();
        }
        else
        {
            Mover->UnCrouch();
        }
    }
}

FRotator ASampleMoverCharacter::ResolveAimingRotation() const
{
    const auto* Target = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("TargetedActor"));
    const auto* Actor = Target ? Cast<AActor>(Target->GetObjectPropertyValue_InContainer(this)) : nullptr;
    if (IsValid(Actor))
    {
        return FRotator(0.0, (Actor->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0);
    }
    const auto* TwinStick = FindFProperty<FBoolProperty>(GetClass(), TEXT("TwinStickMode"));
    if (TwinStick && TwinStick->GetPropertyValue_InContainer(this))
    {
        const auto* Aim = FindSnapshot(*this, TEXT("TwinStickAimRotation"));
        if (Aim && Aim->Struct == TBaseStructure<FRotator>::Get())
        {
            return *Aim->ContainerPtrToValuePtr<FRotator>(this);
        }
    }
    return GetControlRotation();
}

void ASampleMoverCharacter::UpdateControlYawRate()
{
    const auto* Last = FindSnapshot(*this, TEXT("LastControlRotation"));
    if (!Last || Last->Struct != TBaseStructure<FRotator>::Get())
    {
        return;
    }
    auto* Previous = Last->ContainerPtrToValuePtr<FRotator>(this);
    const FRotator Current = GetControlRotation();
    const double Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0;
    WriteNumber(GetClass(), this, TEXT("ControlRotationRate"),
        Delta > 0.0 ? FRotator::NormalizeAxis(Current.Yaw - Previous->Yaw) / Delta : 0.0);
    *Previous = Current;
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
