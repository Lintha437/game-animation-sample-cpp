#include "SampleMoverCharacter.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoverDataModelTypes.h"
#include "PhysicsControlBPLibrary.h"
#include "PhysicsControlComponent.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

namespace
{
    constexpr uint8 Walk = 0;
    constexpr uint8 Run = 1;
    constexpr uint8 Sprint = 2;
    constexpr int64 FaceMovement = 0;
    constexpr int64 AimRotationMode = 2;
    const FName RagdollMode(TEXT("Ragdoll"));
    const FName RagdollTransformField(TEXT("RagdollTransform_33_5FE542414D593629259855B867DEA655"));
    const FName DefaultRagdollProfile(TEXT("DefaultPhysicsProfile"));
    const FName KinematicRagdollProfile(TEXT("Kinematic"));
    const FName PhysicalAnimationProfile(TEXT("PhysicalAnimation"));
    const FName CapsuleProfile(TEXT("CharacterCapsule"));
    // Dedicated latent-action keys on the pawn, distinct from Blueprint node IDs.
    constexpr int32 ConfirmRagdollEntryAction = 0x52414701;
    constexpr int32 ReleaseRagdollFacingAction = 0x52414702;
    constexpr int32 RestoreRagdollProfileAction = 0x52414703;

    enum class EAuthoredMovementMode : uint8
    {
        Grounded,
        Airborne,
        Sliding,
        Traversing,
        Ragdoll,
        Flying,
    };

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

    int64 GetPreSimRotationMode(const ASampleMoverCharacter& Pawn)
    {
        const FStructProperty* Custom = FindSnapshot(Pawn, TEXT("MoverCustomInputs_PreSim"));
        const FProperty* Rotation = Custom ? Custom->Struct->FindPropertyByName(
            TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")) : nullptr;
        return Rotation ? ReadEnum(*Rotation, Rotation->ContainerPtrToValuePtr<void>(
            Custom->ContainerPtrToValuePtr<void>(&Pawn))) : FaceMovement;
    }

    void WriteVector(ASampleMoverCharacter& Pawn, FName Name, const FVector& Value)
    {
        const auto* Property = FindSnapshot(Pawn, Name);
        if (Property && Property->Struct == TBaseStructure<FVector>::Get())
        {
            *Property->ContainerPtrToValuePtr<FVector>(&Pawn) = Value;
        }
    }

    FVector ReadVector(const ASampleMoverCharacter& Pawn, FName Name)
    {
        const auto* Property = FindSnapshot(Pawn, Name);
        return Property && Property->Struct == TBaseStructure<FVector>::Get()
            ? *Property->ContainerPtrToValuePtr<FVector>(&Pawn) : FVector::ZeroVector;
    }

    const FTransform* FindRagdollTransform(const FStructProperty* Snapshot, const ASampleMoverCharacter& Pawn)
    {
        const auto* Field = Snapshot ? FindFProperty<FStructProperty>(Snapshot->Struct, RagdollTransformField) : nullptr;
        return Field && Field->Struct == TBaseStructure<FTransform>::Get()
            ? Field->ContainerPtrToValuePtr<FTransform>(Snapshot->ContainerPtrToValuePtr<void>(&Pawn)) : nullptr;
    }

    USkeletalMeshComponent* GetRagdollMesh(const ASampleMoverCharacter& Pawn)
    {
        const auto* Mesh = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), TEXT("SkeletalMesh"));
        return Mesh ? Cast<USkeletalMeshComponent>(Mesh->GetObjectPropertyValue_InContainer(&Pawn)) : nullptr;
    }

    UCapsuleComponent* GetRagdollCapsule(const ASampleMoverCharacter& Pawn)
    {
        const auto* Capsule = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), TEXT("Capsule"));
        return Capsule ? Cast<UCapsuleComponent>(Capsule->GetObjectPropertyValue_InContainer(&Pawn)) : nullptr;
    }

    void SetLegCollision(USkeletalMeshComponent& Mesh, const TArray<FName>& Left,
        const TArray<FName>& Right, bool bEnabled)
    {
        if (Left.IsEmpty() || Right.IsEmpty())
        {
            return;
        }

        // Physics Control exposes these batched operations as reflected functions.
        auto* Library = GetMutableDefault<UPhysicsControlBPLibrary>();
        const FName Name = bEnabled
            ? GET_FUNCTION_NAME_CHECKED(UPhysicsControlBPLibrary, EnableCollisionBetweenBodyArrays)
            : GET_FUNCTION_NAME_CHECKED(UPhysicsControlBPLibrary, DisableCollisionBetweenBodyArrays);
        UFunction* Function = Library->FindFunction(Name);
        if (!Function)
        {
            return;
        }

        const auto* First = FindFProperty<FObjectPropertyBase>(Function, TEXT("FirstComponent"));
        const auto* Second = FindFProperty<FObjectPropertyBase>(Function, TEXT("SecondComponent"));
        const auto* FirstBones = FindFProperty<FArrayProperty>(Function, TEXT("FirstBoneNames"));
        const auto* SecondBones = FindFProperty<FArrayProperty>(Function, TEXT("SecondBoneNames"));
        if (!First || !Second || !FirstBones || !SecondBones ||
            !CastField<FNameProperty>(FirstBones->Inner) || !CastField<FNameProperty>(SecondBones->Inner))
        {
            return;
        }

        FStructOnScope Args(Function);
        uint8* Data = Args.GetStructMemory();
        First->SetObjectPropertyValue_InContainer(Data, &Mesh);
        Second->SetObjectPropertyValue_InContainer(Data, &Mesh);
        *FirstBones->ContainerPtrToValuePtr<TArray<FName>>(Data) = Left;
        *SecondBones->ContainerPtrToValuePtr<TArray<FName>>(Data) = Right;
        Library->ProcessEvent(Function, Data);
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
        switch (GetPreSimRotationMode(*this))
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
    Input->OrientationIntent = ResolveOrientationIntent();
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

FVector ASampleMoverCharacter::ResolveOrientationIntent() const
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const FCharacterDefaultInputs* Input = GetPreSimInputs(*this);
    if (!Mover || !Input)
    {
        return FVector::ZeroVector;
    }
    static const FGameplayTag RootMotion = FGameplayTag::RequestGameplayTag(TEXT("Mover.AnimRootMotion"), false);
    if (RootMotion.IsValid() && Mover->HasGameplayTag(RootMotion, false))
    {
        return GetActorForwardVector();
    }
    const int64 RotationMode = GetPreSimRotationMode(*this);
    const double AimYaw = ResolveAimingRotation().Yaw;
    const FVector AimDirection = FRotator(0.0, AimYaw, 0.0).Vector();
    switch (static_cast<EAuthoredMovementMode>(ResolveMovementModeValue()))
    {
    case EAuthoredMovementMode::Grounded:
        if (!Input->GetMoveInput().IsNearlyZero())
        {
            return RotationMode == FaceMovement ? Input->GetMoveInput() : AimDirection;
        }
        if (RotationMode == AimRotationMode)
        {
            return FMath::Abs(FRotator::NormalizeAxis(GetActorRotation().Yaw - AimYaw)) > 60.0
                ? AimDirection : Input->OrientationIntent;
        }
        if (const auto* BaseDelta = FindSnapshot(*this, TEXT("BasedMovementDelta"));
            BaseDelta && BaseDelta->Struct == TBaseStructure<FTransform>::Get())
        {
            return BaseDelta->ContainerPtrToValuePtr<FTransform>(this)->GetRotation().RotateVector(
                Input->OrientationIntent.Rotation().Vector());
        }
        return Input->OrientationIntent.Rotation().Vector();
    case EAuthoredMovementMode::Airborne:
        return RotationMode == FaceMovement ? Input->OrientationIntent : AimDirection;
    case EAuthoredMovementMode::Sliding:
        return RotationMode == FaceMovement ? Mover->GetVelocity().GetSafeNormal2D() : AimDirection;
    case EAuthoredMovementMode::Traversing:
        return GetActorForwardVector();
    case EAuthoredMovementMode::Flying:
        return AimDirection;
    default:
        return FVector::ZeroVector;
    }
}

void ASampleMoverCharacter::UpdateMoverFloor()
{
    FFloorCheckResult Floor;
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        bool bWalkable = false;
        UFloorQueryUtils::TryFindFloor(Mover, bWalkable, Floor);
    }
    const auto* HasFloor = FindFProperty<FBoolProperty>(GetClass(), TEXT("HasFloor"));
    if (HasFloor)
    {
        HasFloor->SetPropertyValue_InContainer(this, Floor.bBlockingHit);
    }
    const auto* Mesh = FindComponentByClass<USkeletalMeshComponent>();
    WriteVector(*this, TEXT("FloorLocation"),
        Floor.bBlockingHit ? Floor.HitResult.ImpactPoint : (Mesh ? Mesh->GetComponentLocation() : FVector::ZeroVector));
    WriteVector(*this, TEXT("FloorNormal"), Floor.bBlockingHit ? Floor.HitResult.ImpactNormal : FVector::UpVector);
}

void ASampleMoverCharacter::CaptureBasedMovement(const FTransform& TransformDelta, const FMoverTimeStep& TimeStep)
{
    if (const auto* Delta = FindSnapshot(*this, TEXT("BasedMovementDelta"));
        Delta && Delta->Struct == TBaseStructure<FTransform>::Get())
    {
        *Delta->ContainerPtrToValuePtr<FTransform>(this) = TransformDelta;
    }
}

void ASampleMoverCharacter::NotifyMoverModeFinalized(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const auto* Cached = FindFProperty<FNameProperty>(GetClass(), TEXT("CachedMovementMode"));
    if (!Mover || !Cached)
    {
        return;
    }
    const FName Previous = Cached->GetPropertyValue_InContainer(this);
    const FName Next = Mover->GetMovementModeName();
    if (Previous == Next)
    {
        return;
    }
    if (UFunction* Callback = FindFunction(TEXT("On_MovementModeChanged_PostFinalize")))
    {
        const auto* PreviousParam = FindFProperty<FNameProperty>(Callback, TEXT("PreviousMovementModeName"));
        const auto* NewParam = FindFProperty<FNameProperty>(Callback, TEXT("NewMovementModeName"));
        if (PreviousParam && NewParam)
        {
            FStructOnScope Args(Callback);
            PreviousParam->SetPropertyValue_InContainer(Args.GetStructMemory(), Previous);
            NewParam->SetPropertyValue_InContainer(Args.GetStructMemory(), Next);
            ProcessEvent(Callback, Args.GetStructMemory());
        }
    }
    Cached->SetPropertyValue_InContainer(this, Next);
}

FMoverInputCmdContext ASampleMoverCharacter::BuildRagdollInputCommand(int32 SimTimeMs)
{
    FMoverInputCmdContext Command = BuildMoverInputCommand(SimTimeMs);
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const auto* Snapshot = FindSnapshot(*this, TEXT("RagdollInputs_PreSim"));
    if (!Mover || Mover->GetNextMovementModeName() != RagdollMode ||
        !Snapshot || !Snapshot->Struct->IsA<UUserDefinedStruct>())
    {
        return Command;
    }

    const auto* Transform = FindFProperty<FStructProperty>(Snapshot->Struct, RagdollTransformField);
    if (!Transform || Transform->Struct != TBaseStructure<FTransform>::Get())
    {
        return Command;
    }
    void* Data = Snapshot->ContainerPtrToValuePtr<void>(this);
    *Transform->ContainerPtrToValuePtr<FTransform>(Data) = ResolveRagdollTransform();

    auto* Ragdoll = static_cast<FMoverUserDefinedDataStruct*>(Command.InputCollection.FindOrAddDataByType(Snapshot->Struct));
    Ragdoll->InitializeAs(Snapshot->Struct, static_cast<const uint8*>(Data));
    if (auto* Defaults = Command.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>())
    {
        Defaults->OrientationIntent = ResolveRagdollFacing();
    }
    return Command;
}

FTransform ASampleMoverCharacter::ResolveRagdollTransform() const
{
    const USkeletalMeshComponent* Mesh = GetRagdollMesh(*this);
    if (!Mesh)
    {
        return GetActorTransform();
    }

    const FVector Pelvis = Mesh->GetBoneTransform(TEXT("pelvis"), RTS_World).GetLocation();
    const FTransform Spine = Mesh->GetBoneTransform(TEXT("spine_05"), RTS_World);
    FVector Heading = Spine.GetLocation() - Pelvis;
    if (Spine.GetUnitAxis(EAxis::Y).Z > 0.0)
    {
        Heading = -Heading;
    }
    return FTransform(FRotationMatrix::MakeFromZX(FVector::UpVector, Heading).ToQuat(), Pelvis);
}

FVector ASampleMoverCharacter::ResolveRagdollFacing() const
{
    const auto* Frozen = FindFProperty<FBoolProperty>(GetClass(), TEXT("Ragdoll_FreezeCapsuleRotation"));
    if (Frozen && Frozen->GetPropertyValue_InContainer(this))
    {
        return GetActorForwardVector();
    }
    if (const auto* Rolling = FindFProperty<FBoolProperty>(GetClass(), TEXT("Ragdoll_PlayRollingGetups"));
        Rolling && Rolling->GetPropertyValue_InContainer(this))
    {
        return ReadVector(*this, TEXT("Ragdoll_COM_Velocity")).GetSafeNormal2D();
    }
    const auto* Transform = FindRagdollTransform(FindSnapshot(*this, TEXT("RagdollInputs_PreSim")), *this);
    return Transform ? Transform->GetUnitAxis(EAxis::X) : GetActorForwardVector();
}

void ASampleMoverCharacter::UpdateRagdollVelocities()
{
    const USkeletalMeshComponent* Mesh = GetRagdollMesh(*this);
    if (!Mesh)
    {
        return;
    }
    const FVector Center = Mesh->GetSkeletalCenterOfMass();
    const double DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0;
    const FVector PreviousCenter = ReadVector(*this, TEXT("Ragdoll_COM_LastFrame"));
    WriteVector(*this, TEXT("Ragdoll_COM_Velocity"),
        DeltaSeconds > SMALL_NUMBER ? (Center - PreviousCenter) / DeltaSeconds : FVector::ZeroVector);
    WriteVector(*this, TEXT("Ragdoll_COM_LastFrame"), Center);
    const FVector SpineVelocity = Mesh->GetPhysicsLinearVelocity(TEXT("spine_05"));
    WriteVector(*this, TEXT("Ragdoll_SpineVelocity"), SpineVelocity);
    WriteNumber(GetClass(), this, TEXT("Ragdoll_Speed"), SpineVelocity.Size());
}

void ASampleMoverCharacter::UpdateRagdollGravity()
{
    if (auto* Physics = FindComponentByClass<UPhysicsControlComponent>())
    {
        constexpr double GravityCutoffSpeed = -2500.0;
        const double Multiplier = ReadVector(*this, TEXT("Ragdoll_COM_Velocity")).Z <= GravityCutoffSpeed
            ? 0.0 : ReadNumber(GetClass(), this, TEXT("Ragdoll_GravityMultiplier"));
        Physics->SetBodyModifierGravityMultiplier(TEXT("All"), static_cast<float>(Multiplier), true, true);
    }
}

bool ASampleMoverCharacter::CanStartRagdollCharacterInteraction() const
{
    const USkeletalMeshComponent* Mesh = GetRagdollMesh(*this);
    const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    return CanReceiveRagdollCharacterInteraction() && (!Anim || !Anim->IsSlotActive(TEXT("DefaultSlot"))) &&
        !ReadIntent(FindSnapshot(*this, TEXT("PlayerInputState")), *this,
            TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8"));
}

bool ASampleMoverCharacter::CanReceiveRagdollCharacterInteraction() const
{
    return ResolveMovementModeValue() == static_cast<uint8>(EAuthoredMovementMode::Grounded);
}

void ASampleMoverCharacter::BeginRagdollPhysics()
{
    auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    USkeletalMeshComponent* Mesh = GetRagdollMesh(*this);
    if (!Mover || !Mesh || !GetWorld())
    {
        return;
    }

    // Impulses sent by the caller must see simulated bodies before Physics Control ticks.
    Mesh->SetSimulatePhysics(true);
    ApplyRagdollProfile(RagdollMode);
    Mover->QueueNextMode(RagdollMode);
    UKismetSystemLibrary::DelayUntilNextFrame(this, FLatentActionInfo(
        0, ConfirmRagdollEntryAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, ConfirmRagdollEntry), this));

    if (const auto* Frozen = FindFProperty<FBoolProperty>(GetClass(), TEXT("Ragdoll_FreezeCapsuleRotation")))
    {
        Frozen->SetPropertyValue_InContainer(this, true);
    }
    UKismetSystemLibrary::RetriggerableDelay(this, 0.25f, FLatentActionInfo(
        0, ReleaseRagdollFacingAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, ReleaseRagdollFacing), this));
}

void ASampleMoverCharacter::RequestRagdollRecovery()
{
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        Mover->QueueNextMode(TEXT("Walking"));
    }
}

void ASampleMoverCharacter::ApplyRagdollCapsuleCollision()
{
    if (UCapsuleComponent* Capsule = GetRagdollCapsule(*this))
    {
        Capsule->SetCollisionProfileName(CapsuleProfile);
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    }
}

void ASampleMoverCharacter::RestoreRagdollCapsuleCollision()
{
    if (UCapsuleComponent* Capsule = GetRagdollCapsule(*this))
    {
        Capsule->SetCollisionProfileName(CapsuleProfile);
    }
}

void ASampleMoverCharacter::FinishRagdollPhysicsRecovery()
{
    if (auto* Physics = FindComponentByClass<UPhysicsControlComponent>())
    {
        FPhysicsControlData Recovery;
        Recovery.LinearTargetVelocityMultiplier = 0.f;
        Recovery.AngularTargetVelocityMultiplier = 0.f;
        Physics->SetControlDatasInSet(TEXT("ParentSpace"), Recovery);
    }
    if (GetWorld())
    {
        UKismetSystemLibrary::DelayUntilNextFrame(this, FLatentActionInfo(
            0, RestoreRagdollProfileAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, RestoreRagdollDefaultProfile), this));
    }
}

void ASampleMoverCharacter::RefreshRagdollDefaultProfile()
{
    const auto* Default = FindFProperty<FNameProperty>(GetClass(), DefaultRagdollProfile);
    if (!Default)
    {
        return;
    }
    const int32 Choice = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCVar.CharacterPhysics.DefaultProfile"));
    const FName Profile = Choice == 1 ? PhysicalAnimationProfile : KinematicRagdollProfile;
    Default->SetPropertyValue_InContainer(this, Profile);
    if (ResolveMovementModeValue() != static_cast<uint8>(EAuthoredMovementMode::Ragdoll))
    {
        ApplyRagdollProfile(Profile);
    }
}

void ASampleMoverCharacter::ApplyRagdollProfile(FName PhysicsProfile)
{
    if (const auto* Applied = FindFProperty<FNameProperty>(GetClass(), TEXT("AppliedPhysicsProfile")))
    {
        Applied->SetPropertyValue_InContainer(this, PhysicsProfile);
    }
    if (auto* Physics = FindComponentByClass<UPhysicsControlComponent>())
    {
        Physics->InvokeControlProfile(PhysicsProfile);
    }
    if (USkeletalMeshComponent* Mesh = GetRagdollMesh(*this))
    {
        if (PhysicsProfile == RagdollMode)
        {
            SetLegCollision(*Mesh, LeftLegPhysicsBodies, RightLegPhysicsBodies, true);
            Mesh->SetConstraintProfileForAll(NAME_None, false);
        }
        else if (PhysicsProfile == KinematicRagdollProfile || PhysicsProfile == PhysicalAnimationProfile)
        {
            SetLegCollision(*Mesh, LeftLegPhysicsBodies, RightLegPhysicsBodies, false);
            Mesh->SetConstraintProfileForAll(TEXT("Free"), false);
        }
    }
}

void ASampleMoverCharacter::TraceRagdollObstacles()
{
    const USkeletalMeshComponent* Mesh = GetRagdollMesh(*this);
    if (!Mesh || !GetWorld())
    {
        return;
    }
    const FVector Neck = Mesh->GetSocketLocation(TEXT("neck_01"));
    const FVector Feet = (Mesh->GetSocketLocation(TEXT("foot_l")) + Mesh->GetSocketLocation(TEXT("foot_r"))) * 0.5;
    const auto* Debug = FindFProperty<FIntProperty>(GetClass(), TEXT("DebugState"));
    const auto Draw = Debug && Debug->GetPropertyValue_InContainer(this) == 1
        ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;
    TArray<FHitResult> Hits;
    if (!UKismetSystemLibrary::SphereTraceMultiForObjects(this, Neck, Feet, 15.f,
        {EObjectTypeQuery::ObjectTypeQuery7}, false, {}, Draw, Hits, true))
    {
        return;
    }

    UFunction* Trigger = FindFunction(TEXT("TriggerRagdoll"));
    const auto* Stop = Trigger ? FindFProperty<FBoolProperty>(Trigger, TEXT("StopActiveMontages")) : nullptr;
    const auto* Blend = Trigger ? FindFProperty<FStructProperty>(Trigger, TEXT("MontageBlendOut")) : nullptr;
    const auto* DefaultBlend = FindSnapshot(*this, TEXT("DefaultRagdollMontageBlendOut"));
    if (Stop && Blend && DefaultBlend && Blend->SameType(DefaultBlend))
    {
        FStructOnScope Args(Trigger);
        uint8* Data = Args.GetStructMemory();
        Stop->SetPropertyValue_InContainer(Data, true);
        Blend->CopyCompleteValue(Blend->ContainerPtrToValuePtr<void>(Data), DefaultBlend->ContainerPtrToValuePtr<void>(this));
        ProcessEvent(Trigger, Data);
    }
}

void ASampleMoverCharacter::ConfirmRagdollEntry()
{
    if (ResolveMovementModeValue() != static_cast<uint8>(EAuthoredMovementMode::Ragdoll))
    {
        if (const auto* Default = FindFProperty<FNameProperty>(GetClass(), DefaultRagdollProfile))
        {
            ApplyRagdollProfile(Default->GetPropertyValue_InContainer(this));
        }
        ReleaseRagdollFacing();
    }
}

void ASampleMoverCharacter::ReleaseRagdollFacing()
{
    if (const auto* Frozen = FindFProperty<FBoolProperty>(GetClass(), TEXT("Ragdoll_FreezeCapsuleRotation")))
    {
        Frozen->SetPropertyValue_InContainer(this, false);
    }
}

void ASampleMoverCharacter::RestoreRagdollDefaultProfile()
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (Mover && Mover->GetNextMovementModeName() != RagdollMode)
    {
        if (const auto* Default = FindFProperty<FNameProperty>(GetClass(), DefaultRagdollProfile))
        {
            ApplyRagdollProfile(Default->GetPropertyValue_InContainer(this));
        }
    }
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
