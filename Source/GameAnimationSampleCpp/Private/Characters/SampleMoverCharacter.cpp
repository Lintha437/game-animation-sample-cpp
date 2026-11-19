#include "Characters/SampleMoverCharacter.h"

#include "Characters/SampleMoverCharacterInternal.h"
#include "Characters/SamplePawnSetup.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/SampleAnimationTickComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataDrivenCVars/DataDrivenCVars.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "DrawDebugLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoverDataModelTypes.h"
#include "SmartObjects/SampleSmartObjectInteractionComponent.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Traversal/SampleTraversalClearance.h"
#include "Traversal/SampleTraversalComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"
#include "Utilities/SampleLogChannels.h"
#include "Utilities/SamplePropertyAccess.h"

namespace
{
    using namespace SampleMoverInternal;

    constexpr int32 ClearMoverJumpEdgeAction = 0x4D4F5601;

    enum class EAuthoredMovementSector : uint8
    {
        Forward = 0,
        Backward = 1,
        Left = 2,
        Right = 4,
    };

    struct FMovementDirectionLimits
    {
        double Forward = 0.0;
        double Backward = 0.0;
    };

    FMovementDirectionLimits ResolveDirectionLimits(int64 Rotation, EAuthoredMovementSector Sector)
    {
        const int32 Style = UKismetSystemLibrary::GetConsoleVariableIntValue(
            Rotation == AimRotationMode ? TEXT("DDCvar.AimStyle") : TEXT("DDCvar.StrafeStyle"));
        switch (Style)
        {
        case 0:
            return Sector == EAuthoredMovementSector::Forward || Sector == EAuthoredMovementSector::Backward
                ? FMovementDirectionLimits{60.0, 120.0} : FMovementDirectionLimits{40.0, 140.0};
        case 1:
            return Sector == EAuthoredMovementSector::Backward
                ? FMovementDirectionLimits{120.0, 120.0} : FMovementDirectionLimits{140.0, 140.0};
        case 2:
            return {180.0, 180.0};
        default:
            return {};
        }
    }

    int64 ReadEnum(const FProperty& Property, const void* Value)
    {
        return SamplePropertyAccess::ReadEnum(Property, Value);
    }

    const FMapProperty* FindMovementModeMap(const ASampleMoverCharacter& Pawn)
    {
        const auto* Map = FindFProperty<FMapProperty>(Pawn.GetClass(), TEXT("MovementModeMap"));
        return Map && CastField<FNameProperty>(Map->KeyProp) ? Map : nullptr;
    }

    uint8 ReadMovementModeValue(const ASampleMoverCharacter& Pawn, const FMapProperty& Map, FName Mode)
    {
        FScriptMapHelper Entries(&Map, Map.ContainerPtrToValuePtr<void>(&Pawn));
        const uint8* Value = Entries.FindValueFromHash(&Mode);
        return Value ? static_cast<uint8>(ReadEnum(*Map.ValueProp, Value)) : 0;
    }

    const FStructProperty* FindSnapshot(const ASampleMoverCharacter& Pawn, FName Name)
    {
        return SamplePropertyAccess::FindStructProperty(Pawn, Name);
    }

    const FCharacterDefaultInputs* GetPreSimInputs(const ASampleMoverCharacter& Pawn)
    {
        const FStructProperty* Input = FindSnapshot(Pawn, TEXT("MoverDefaultInputs_PreSim"));
        return Input && Input->Struct == FCharacterDefaultInputs::StaticStruct()
            ? Input->ContainerPtrToValuePtr<FCharacterDefaultInputs>(&Pawn) : nullptr;
    }

    bool ReadIntent(const FStructProperty* Snapshot, const ASampleMoverCharacter& Pawn, FName Name)
    {
        return SamplePropertyAccess::ReadStructBool(Snapshot, Pawn, Name);
    }

    void WriteMoverIntent(ASampleMoverCharacter& Pawn, FName Name, bool bValue)
    {
        SamplePropertyAccess::WriteStructBool(FindSnapshot(Pawn, MoverPlayerInputState), Pawn, Name, bValue);
    }

    bool IsMoverTwinStick(const ASampleMoverCharacter& Pawn)
    {
        return SamplePropertyAccess::ReadBool(Pawn, TEXT("TwinStickMode"));
    }

    const FArrayProperty* FindMoverTargets(const ASampleMoverCharacter& Pawn)
    {
        const auto* Array = FindFProperty<FArrayProperty>(Pawn.GetClass(), TEXT("TargetableActors"));
        const auto* Inner = Array ? CastField<FObjectPropertyBase>(Array->Inner) : nullptr;
        return Inner && Inner->PropertyClass == AActor::StaticClass() ? Array : nullptr;
    }

    int64 GetSnapshotRotationMode(const ASampleMoverCharacter& Pawn, FName Snapshot)
    {
        const FStructProperty* Custom = FindSnapshot(Pawn, Snapshot);
        const FProperty* Rotation = Custom ? Custom->Struct->FindPropertyByName(
            TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")) : nullptr;
        return Rotation ? ReadEnum(*Rotation, Rotation->ContainerPtrToValuePtr<void>(
            Custom->ContainerPtrToValuePtr<void>(&Pawn))) : FaceMovement;
    }

    int64 GetPreSimRotationMode(const ASampleMoverCharacter& Pawn)
    {
        return GetSnapshotRotationMode(Pawn, TEXT("MoverCustomInputs_PreSim"));
    }

    EAuthoredMovementSector ReadDirectionSector(const UScriptStruct* Type, const void* Data)
    {
        const auto* Field = Type ? Type->FindPropertyByName(MovementSectorField) : nullptr;
        return Field ? static_cast<EAuthoredMovementSector>(ReadEnum(*Field, Field->ContainerPtrToValuePtr<void>(Data)))
            : EAuthoredMovementSector::Forward;
    }

    void WriteVector(ASampleMoverCharacter& Pawn, FName Name, const FVector& Value)
    {
        SamplePropertyAccess::WriteVector(Pawn, Name, Value);
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
        SamplePropertyAccess::CopyEnum(Target, Destination, Source, SourceContainer);
    }

    void SetEnumByte(const UStruct* Type, void* Data, FName Name, uint8 Value)
    {
        SamplePropertyAccess::WriteEnumByte(Type, Data, Name, Value);
    }

    double ReadNumber(const UStruct* Type, const void* Data, FName Name)
    {
        return SamplePropertyAccess::ReadNumber(Type, Data, Name);
    }

    void WriteNumber(const UStruct* Type, void* Data, FName Name, double Value)
    {
        SamplePropertyAccess::WriteNumber(Type, Data, Name, Value);
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
    const auto* Map = FindMovementModeMap(*this);
    return Mover && Map ? ReadMovementModeValue(*this, *Map, Mover->GetMovementModeName()) : 0;
}

void ASampleMoverCharacter::InstallSandboxInputMapping()
{
    SamplePawnSetup::InstallInputMapping(*this);
}

void ASampleMoverCharacter::ActivateSandboxCamera()
{
    SamplePawnSetup::ActivateCamera(*this, TEXT("Camera"));
}

void ASampleMoverCharacter::InitializeMoverRuntime()
{
    auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (Mover)
    {
        Mover->OnPostFinalize.AddUniqueDynamic(this, &ASampleMoverCharacter::NotifyMoverModeFinalized);
        Mover->OnPreSimulationTick.AddUniqueDynamic(this, &ASampleMoverCharacter::ApplyMoverCrouchInput);
        Mover->OnBasedMovementApplied.AddUniqueDynamic(this, &ASampleMoverCharacter::CaptureBasedMovement);
    }
    if (auto* Mesh = GetRagdollMesh(*this))
    {
        if (Mover)
        {
            Mesh->AddTickPrerequisiteComponent(Mover);
        }
        Mesh->AddTickPrerequisiteActor(this);
    }
    if (const auto* Inputs = FindSnapshot(*this, TEXT("MoverDefaultInputs_PreSim"));
        Inputs && Inputs->Struct == FCharacterDefaultInputs::StaticStruct())
    {
        Inputs->ContainerPtrToValuePtr<FCharacterDefaultInputs>(this)->OrientationIntent = GetActorForwardVector();
    }
    if (const auto* Initial = FindSnapshot(*this, TEXT("InitialTransform"));
        Initial && Initial->Struct == TBaseStructure<FTransform>::Get())
    {
        *Initial->ContainerPtrToValuePtr<FTransform>(this) = GetActorTransform();
    }
}

void ASampleMoverCharacter::UpdateMoverRuntime()
{
    const auto Present = [this](FName Name)
    {
        if (UFunction* Callback = FindFunction(Name); Callback && Callback->NumParms == 0)
        {
            ProcessEvent(Callback, nullptr);
        }
    };
    UpdateCachedMoverInputs();
    UpdateMoverFloor();
    UpdateControlYawRate();
    Present(TEXT("Update_SlidingAudio"));
    UpdateMoverTarget();
    UpdateMoverTwinStick();
    UpdateMoverAnalogDeflection();
    DrawMoverMovementDebug();
}

void ASampleMoverCharacter::DrawMoverMovementDebug()
{
#if ENABLE_DRAW_DEBUG
    if (!IsLocallyControlled() || !UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("DDCvar.DrawCharacterDebugShapes")))
    {
        return;
    }
    const auto* Capsule = GetRagdollCapsule(*this);
    if (!Capsule)
    {
        return;
    }

    constexpr float Thickness = 0.6f;
    const FVector Origin = Capsule->GetComponentLocation() - FVector(0, 0, Capsule->GetUnscaledCapsuleHalfHeight());
    const FRotator CapsuleRotation = Capsule->GetComponentRotation();
    const FDebugDrawer Drawer = UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(this, TEXT("Draw Debug Library"));
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const auto* Mesh = GetRagdollMesh(*this);
    const auto* Snapshot = FindSnapshot(*this, TEXT("MoverCustomInputs_PreSim"));
    const auto* Type = Snapshot ? Snapshot->Struct.Get() : nullptr;
    const void* Data = Snapshot ? Snapshot->ContainerPtrToValuePtr<void>(this) : nullptr;
    const FRotator Target = Mover ? Mover->GetTargetOrientation() : FRotator::ZeroRotator;
    const FRotator Root = Mesh ? Mesh->GetBoneTransform(TEXT("root"), RTS_World).Rotator() : FRotator::ZeroRotator;
    const FRotator RootArrow = UKismetMathLibrary::ComposeRotators(FRotator(0, 90, 0), Root);
    const FDrawDebugLineStyle Black(FLinearColor::Black, Thickness);
    const FDrawDebugLineStyle Magenta(FLinearColor(1, 0, 1), Thickness);
    const FDrawDebugLineStyle Orange(FLinearColor(1, 0.5f, 0), Thickness);
    const FVector IntentLayer = Origin + FVector(0, 0, 1.2);

    UDrawDebugLibrary::DrawDebugCircle(Drawer, Origin, CapsuleRotation, Black, true, 30, 50);
    UDrawDebugLibrary::DrawDebugFlatArrow(Drawer, Origin - RootArrow.Vector() * 5, RootArrow, Black, true, 20, 20);
    UDrawDebugLibrary::DrawDebugCircleArrow(Drawer, Origin, CapsuleRotation, 0, Black, true, 35, 10);
    UDrawDebugLibrary::DrawDebugCircleArrow(Drawer, Origin + FVector(0, 0, 0.6), Target, 0, Magenta, true, 50, 30);

    const double Offset = Type ? ReadNumber(Type, Data, DirectionalOffsetField) : 0.0;
    if (Offset != 0.0)
    {
        UDrawDebugLibrary::DrawDebugCircleArrow(Drawer, Origin, Target, static_cast<float>(Offset),
            FDrawDebugLineStyle(FLinearColor(0.75f, 0, 0.75f), Thickness), true, 50, 30);
        UDrawDebugLibrary::DrawDebugArc(Drawer, Origin, Target, static_cast<float>(Offset), Magenta, true, 60, 50);
    }

    const FVector Intent = Mover ? Mover->GetMovementIntent() : FVector::ZeroVector;
    if (UKismetMathLibrary::NotEqual_VectorVector(Intent, FVector::ZeroVector))
    {
        UDrawDebugLibrary::DrawDebugCircleArrow(Drawer, IntentLayer, Intent.Rotation(), 0, Orange, true, 50, 30);
        const float Angle = static_cast<float>(ReadNumber(GetClass(), this, TEXT("DebugAngle")));
        UDrawDebugLibrary::DrawDebugArc(Drawer, IntentLayer, Target, Angle, Orange, true, 50, 50);
    }

    const int64 Rotation = GetPreSimRotationMode(*this);
    if (Rotation != FaceMovement)
    {
        const EAuthoredMovementSector Sector = ReadDirectionSector(Type, Data);
        const FMovementDirectionLimits Limits = ResolveDirectionLimits(Rotation, Sector);
        const float Forward = static_cast<float>(Limits.Forward);
        const float Backward = static_cast<float>(Limits.Backward);
        const float Ticks[] = {-Forward, Forward, -Backward, Backward};
        UDrawDebugLibrary::DrawDebugCircleTicksArrayView(Drawer, IntentLayer, Target, Ticks, Magenta, true, 80, 5);

        // The backward arc crosses -180 degrees. Its signed width matches the direction of the authored overlay.
        float Start = -Forward;
        float Width = 2 * Forward;
        if (Sector == EAuthoredMovementSector::Backward)
        {
            Start = -Backward;
            Width = 2 * Backward - 360;
        }
        else if (Sector >= EAuthoredMovementSector::Right)
        {
            Start = Forward;
            Width = FMath::Abs(Backward - Forward);
        }
        else if (Sector >= EAuthoredMovementSector::Left)
        {
            Start = -Backward;
            Width = FMath::Abs(Backward - Forward);
        }
        const FRotator ArcRotation(Target.Pitch, Target.Yaw + Start, Target.Roll);
        UDrawDebugLibrary::DrawDebugArc(Drawer, IntentLayer, ArcRotation, Width,
            FDrawDebugLineStyle(FLinearColor::Green, Thickness), true, 80, 50);
    }
#endif
}

void ASampleMoverCharacter::BuildImportedDirectionThresholds(const ASampleMoverCharacter*, int32&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(ASampleMoverCharacter::execBuildImportedDirectionThresholds)
{
    P_GET_OBJECT(ASampleMoverCharacter, Pawn);
    Stack.MostRecentProperty = nullptr;
    Stack.MostRecentPropertyAddress = nullptr;
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const auto* Property = CastField<FStructProperty>(Stack.MostRecentProperty);
    void* Output = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    const auto* Struct = Property ? Cast<UUserDefinedStruct>(Property->Struct.Get()) : nullptr;
    if (Output && Struct && Struct->GetFName() == TEXT("S_MovementDirectionThresholds"))
    {
        const auto* Snapshot = Pawn ? FindSnapshot(*Pawn, TEXT("MoverCustomInputs_PreSim")) : nullptr;
        const auto Sector = Snapshot ? ReadDirectionSector(Snapshot->Struct, Snapshot->ContainerPtrToValuePtr<void>(Pawn))
            : EAuthoredMovementSector::Forward;
        const FMovementDirectionLimits Limits = Pawn ? ResolveDirectionLimits(GetPreSimRotationMode(*Pawn), Sector)
            : FMovementDirectionLimits{};
        WriteNumber(Struct, Output, TEXT("FL_3_747C69604F939E7EFF347D97BBD8A811"), -Limits.Forward);
        WriteNumber(Struct, Output, TEXT("FR_5_790F2C4742CB53A5585DBFAF660F9CF0"), Limits.Forward);
        WriteNumber(Struct, Output, TEXT("BL_7_630ADC5A4B6C3AF9DA182E816EDB311B"), -Limits.Backward);
        WriteNumber(Struct, Output, TEXT("BR_9_67DF35F84900050330F770A709E84CF7"), Limits.Backward);
    }
    P_NATIVE_END;
}

void ASampleMoverCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (bUseNativeCameraStyleKeys && PlayerInputComponent)
    {
        SamplePawnSetup::BindCameraStyleKeys(*this, *PlayerInputComponent);
    }
    auto* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!Input)
    {
        return;
    }
    if (bUseNativeFlightToggle)
    {
        PlayerInputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &ASampleMoverCharacter::ToggleMoverFlight);
#if !UE_BUILD_SHIPPING
        Input->BindDebugKey(FInputChord(EKeys::Z), IE_Pressed, this, &ASampleMoverCharacter::HandleMoverFlightDebugKey, false);
#endif
    }
    const auto Bind = [this, Input](const TSoftObjectPtr<UInputAction>& Action, auto Handler)
    {
        if (const UInputAction* Loaded = Action.LoadSynchronous())
        {
            Input->BindAction(Loaded, ETriggerEvent::Triggered, this, Handler);
        }
    };
    if (bUseNativeMoverInteractionInput)
    {
        Bind(MoverInteractAction, &ASampleMoverCharacter::HandleMoverShove);
        Bind(MoverTakedownAction, &ASampleMoverCharacter::HandleMoverTakedown);
    }
    if (bUseNativeMoverRagdollInput)
    {
        Bind(MoverRagdollAction, &ASampleMoverCharacter::HandleMoverRagdollInput);
    }
    if (bUseNativeMoverJumpInput)
    {
        if (const UInputAction* Jump = MoverJumpAction.LoadSynchronous())
        {
            Input->BindAction(Jump, ETriggerEvent::Started, this, &ASampleMoverCharacter::HandleMoverJumpStarted);
            Input->BindAction(Jump, ETriggerEvent::Triggered, this, &ASampleMoverCharacter::HandleMoverJumpTriggered);
        }
    }
    if (!bUseNativeMoverDeviceInput)
    {
        return;
    }
    for (const auto& ValueAction : {MoverMoveAction, MoverTwinStickAimAction})
    {
        if (const UInputAction* Action = ValueAction.LoadSynchronous())
        {
            Input->BindActionValue(Action);
        }
    }
    Bind(MoverLookAction, &ASampleMoverCharacter::HandleMoverLook);
    Bind(MoverGamepadLookAction, &ASampleMoverCharacter::HandleMoverGamepadLook);
    Bind(MoverCrouchAction, &ASampleMoverCharacter::HandleMoverCrouch);
    Bind(MoverWalkAction, &ASampleMoverCharacter::HandleMoverWalk);
    Bind(MoverSprintAction, &ASampleMoverCharacter::HandleMoverSprint);
    Bind(MoverStrafeAction, &ASampleMoverCharacter::HandleMoverStrafe);
    Bind(MoverAimAction, &ASampleMoverCharacter::HandleMoverAim);
    if (auto* Interaction = FindComponentByClass<USampleSmartObjectInteractionComponent>())
    {
        Interaction->BindPlayerInteractionInput(*Input);
    }
}

FVector2D ASampleMoverCharacter::ReadMoverAction(const TSoftObjectPtr<UInputAction>& Action) const
{
    const auto* Input = Cast<UEnhancedInputComponent>(InputComponent);
    const UInputAction* Loaded = Action.Get();
    return Input && Loaded ? Input->GetBoundActionValue(Loaded).Get<FVector2D>() : FVector2D::ZeroVector;
}

void ASampleMoverCharacter::ToggleMoverFlight()
{
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        const bool bFlying = ResolveMovementModeValue() == static_cast<uint8>(EAuthoredMovementMode::Flying);
        UKismetSystemLibrary::PrintString(this, bFlying ? TEXT("Flying mode OFF") : TEXT("Flying mode ON"));
        Mover->QueueNextMode(bFlying ? TEXT("Falling") : TEXT("Flying"));
    }
}

void ASampleMoverCharacter::HandleMoverFlightDebugKey(FKey Key, FInputActionValue Value)
{
    ToggleMoverFlight();
}

FVector ASampleMoverCharacter::ResolveMoverDirection()
{
    if (Cast<AAIController>(GetController()))
    {
        FVector Intent = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        if (auto* NavMover = FindComponentByClass<UNavMoverComponent>())
        {
            NavMover->ConsumeNavMovementData(Intent, Velocity);
        }
        return Velocity.GetSafeNormal();
    }

    const FVector2D Device = ReadMoverAction(MoverMoveAction);
    const FRotator View = GetControlRotation();
    const bool bFlying = ResolveMovementModeValue() == static_cast<uint8>(EAuthoredMovementMode::Flying);
    const FRotator Basis = bFlying ? View : FRotator(0.0, View.Yaw, 0.0);
    const FVector Direction = FVector(Device.Y, Device.X, 0.0).GetClampedToMaxSize(1.0);
    return Basis.RotateVector(Direction).GetSafeNormal();
}

uint8 ASampleMoverCharacter::ResolveMoverRotationMode() const
{
    const auto* State = FindSnapshot(*this, MoverPlayerInputState);
    const bool bAiming = ReadIntent(State, *this, MoverAimIntent);
    const auto* Target = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("TargetedActor"));
    if (Target && IsValid(Target->GetObjectPropertyValue_InContainer(this)))
    {
        return bAiming ? AimRotationMode : StrafeRotationMode;
    }
    if (IsMoverTwinStick(*this))
    {
        return ReadMoverAction(MoverTwinStickAimAction).IsNearlyZero()
            ? FaceMovement : (bAiming ? AimRotationMode : StrafeRotationMode);
    }
    if (bAiming)
    {
        return AimRotationMode;
    }
    return ReadIntent(State, *this, MoverStrafeIntent) ? StrafeRotationMode : FaceMovement;
}

void ASampleMoverCharacter::AddMoverTarget(AActor* TargetToAdd)
{
    if (const auto* Property = FindMoverTargets(*this))
    {
        FScriptArrayHelper Targets(Property, Property->ContainerPtrToValuePtr<void>(this));
        const auto* Item = CastFieldChecked<FObjectPropertyBase>(Property->Inner);
        Item->SetObjectPropertyValue(Targets.GetRawPtr(Targets.AddValue()), TargetToAdd);
    }
}

void ASampleMoverCharacter::RemoveMoverTarget(AActor* TargetToRemove)
{
    if (const auto* Property = FindMoverTargets(*this))
    {
        FScriptArrayHelper Targets(Property, Property->ContainerPtrToValuePtr<void>(this));
        const auto* Item = CastFieldChecked<FObjectPropertyBase>(Property->Inner);
        for (int32 Index = Targets.Num() - 1; Index >= 0; --Index)
        {
            if (Item->GetObjectPropertyValue(Targets.GetRawPtr(Index)) == TargetToRemove)
            {
                Targets.RemoveValues(Index, 1);
            }
        }
    }
}

void ASampleMoverCharacter::UpdateMoverTarget()
{
    const auto* Current = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("TargetedActor"));
    const auto* Property = FindMoverTargets(*this);
    if (!Current || !Property)
    {
        return;
    }

    FScriptArrayHelper Targets(Property, Property->ContainerPtrToValuePtr<void>(this));
    const auto* Item = CastFieldChecked<FObjectPropertyBase>(Property->Inner);
    const FVector Origin = GetActorLocation();
    AActor* Nearest = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        auto* Candidate = Cast<AActor>(Item->GetObjectPropertyValue(Targets.GetRawPtr(Index)));
        if (!IsValid(Candidate))
        {
            continue;
        }
        const double Distance = FVector::DistSquared(Origin, Candidate->GetActorLocation());
        if (Distance < BestDistance)
        {
            Nearest = Candidate;
            BestDistance = Distance;
        }
    }
    Current->SetObjectPropertyValue_InContainer(this, Nearest);
    if (Nearest)
    {
        UDrawDebugLibrary::DrawDebugCone(UDrawDebugLibrary::MakeDebugDrawer(this),
            Nearest->GetActorLocation() + FVector(0.0, 0.0, 150.0), FRotator(0.0, 0.0, 180.0),
            FDrawDebugLineStyle(FLinearColor::Red, 1.0f), true, 20.0f, 10.0f, 4);
    }
}

void ASampleMoverCharacter::UpdateMoverTwinStick()
{
    const auto* Mode = FindFProperty<FBoolProperty>(GetClass(), TEXT("TwinStickMode"));
    if (!Mode)
    {
        return;
    }
    const bool bTwinStick = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.ControlStyle")) == 1;
    Mode->SetPropertyValue_InContainer(this, bTwinStick);
    AController* OwnerController = GetController();
    if (!bTwinStick || !IsValid(OwnerController))
    {
        return;
    }

    OwnerController->SetControlRotation(FRotator::ZeroRotator);
    const auto* Aim = FindSnapshot(*this, TEXT("TwinStickAimRotation"));
    if (Aim && Aim->Struct == TBaseStructure<FRotator>::Get())
    {
        const FVector2D Direction = ReadMoverAction(MoverTwinStickAimAction);
        *Aim->ContainerPtrToValuePtr<FRotator>(this) = Direction.IsNearlyZero(0.1)
            ? GetActorRotation()
            : FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, -Direction.X)), 0.0);
    }
}

void ASampleMoverCharacter::UpdateMoverAnalogDeflection()
{
    const double Current = ReadNumber(GetClass(), this, TEXT("SmoothedAnalogInputAmount"));
    const double Target = ReadMoverAction(MoverMoveAction).Size();
    const double Delta = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0;
    WriteNumber(GetClass(), this, TEXT("SmoothedAnalogInputAmount"),
        FMath::FInterpConstantTo(Current, Target, Delta, Target > Current ? 100.0 : 2.0));
}

void ASampleMoverCharacter::ApplyNpcWalkIntent(bool bWalk)
{
    WriteMoverIntent(*this, MoverWalkIntent, bWalk);
    WriteMoverIntent(*this, MoverSprintIntent, false);
    WriteMoverIntent(*this, MoverStrafeIntent, false);
    WriteMoverIntent(*this, MoverAimIntent, false);
    WriteMoverIntent(*this, MoverCrouchIntent, false);
}

bool ASampleMoverCharacter::CanStartMoverTraversal() const
{
    const auto Mode = static_cast<EAuthoredMovementMode>(ResolveMovementModeValue());
    const auto* Traversal = FindComponentByClass<USampleTraversalComponent>();
    if (!Traversal || Traversal->IsTraversalActionActive() ||
        (Mode != EAuthoredMovementMode::Grounded && Mode != EAuthoredMovementMode::Airborne))
    {
        return false;
    }
    const auto* Mesh = GetRagdollMesh(*this);
    const auto* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    return !Anim || !Anim->IsSlotActive(TEXT("DefaultSlot"));
}

uint8 ASampleMoverCharacter::GetAuthoredGaitValue() const
{
    const auto* Snapshot = FindSnapshot(*this, TEXT("MoverCustomInputs_PostSim"));
    const auto* Gait = Snapshot ? Snapshot->Struct->FindPropertyByName(MoverGaitField) : nullptr;
    return Gait ? static_cast<uint8>(ReadEnum(*Gait, Gait->ContainerPtrToValuePtr<void>(
        Snapshot->ContainerPtrToValuePtr<void>(this)))) : MoverGaitWalk;
}

uint8 ASampleMoverCharacter::GetAuthoredCameraModeValue() const
{
    constexpr uint8 TwinStickCamera = 3;
    if (IsMoverTwinStick(*this))
    {
        return TwinStickCamera;
    }
    const int64 Rotation = GetSnapshotRotationMode(*this, TEXT("MoverCustomInputs_PostSim"));
    return Rotation >= FaceMovement && Rotation <= AimRotationMode ? static_cast<uint8>(Rotation) : FaceMovement;
}

bool ASampleMoverCharacter::TryBuildTraversalApproach(FSampleTraversalApproach& Approach) const
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const auto Mode = static_cast<EAuthoredMovementMode>(ResolveMovementModeValue());
    if (!Mover || (Mode != EAuthoredMovementMode::Grounded && Mode != EAuthoredMovementMode::Airborne &&
        Mode != EAuthoredMovementMode::Sliding && Mode != EAuthoredMovementMode::Traversing))
    {
        return false;
    }

    Approach = {};
    Approach.Radius = 30.0f;
    if (Mode == EAuthoredMovementMode::Airborne)
    {
        const auto* Snapshot = FindSnapshot(*this, TEXT("MoverDefaultInputs_PostSim"));
        const FVector Move = Snapshot && Snapshot->Struct == FCharacterDefaultInputs::StaticStruct()
            ? Snapshot->ContainerPtrToValuePtr<FCharacterDefaultInputs>(this)->GetMoveInput() : FVector::ZeroVector;
        Approach.Direction = Move.IsNearlyZero() ? GetActorForwardVector() : Move.GetSafeNormal();
        Approach.Distance = 75.0;
        Approach.HalfHeight = 86.0f;
        Approach.EndOffset.Z = 50.0;
    }
    else
    {
        const FVector Velocity = Mover->GetVelocity();
        Approach.Direction = Velocity.SizeSquared2D() > FMath::Square(50.0)
            ? Velocity.GetSafeNormal() : Mover->GetTargetOrientation().Vector();
        const double LocalSpeed = GetActorQuat().UnrotateVector(Velocity).X;
        Approach.Distance = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 375.0), FVector2D(75.0, 300.0), LocalSpeed);
        Approach.HalfHeight = 60.0f;
    }

    return true;
}

bool ASampleMoverCharacter::TryMoverTraversal(bool bDrawForDuration)
{
    auto* Traversal = FindComponentByClass<USampleTraversalComponent>();
    FSampleTraversalApproach Approach;
    return Traversal && TryBuildTraversalApproach(Approach) && Traversal->TryAuthoredTraversal(Approach,
        bDrawForDuration ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::ForOneFrame);
}

void ASampleMoverCharacter::HandleMoverJumpStarted()
{
    const auto Mode = static_cast<EAuthoredMovementMode>(ResolveMovementModeValue());
    if (Mode == EAuthoredMovementMode::Ragdoll && FindSnapshot(*this, TEXT("RagdollInputs_PreSim")))
    {
        const auto* HasFloor = FindFProperty<FBoolProperty>(GetClass(), TEXT("HasFloor"));
        if (HasFloor && HasFloor->GetPropertyValue_InContainer(this))
        {
            RequestRagdollRecovery();
        }
        return;
    }
    if (Mode == EAuthoredMovementMode::Sliding)
    {
        WriteMoverIntent(*this, MoverCrouchIntent, false);
    }
    if (!CanStartMoverTraversal() || TryMoverTraversal(true))
    {
        return;
    }
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (Mover && Mover->IsCrouching())
    {
        WriteMoverIntent(*this, MoverCrouchIntent, false);
        return;
    }
    if (const auto* Edge = FindFProperty<FBoolProperty>(GetClass(), MoverJumpEdge); Edge && GetWorld())
    {
        Edge->SetPropertyValue_InContainer(this, true);
        UKismetSystemLibrary::DelayUntilNextFrame(this, FLatentActionInfo(
            0, ClearMoverJumpEdgeAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, ClearMoverJumpEdge), this));
    }
}

void ASampleMoverCharacter::HandleMoverJumpTriggered()
{
    if (CanStartMoverTraversal())
    {
        TryMoverTraversal(false);
    }
}

void ASampleMoverCharacter::ClearMoverJumpEdge()
{
    if (const auto* Edge = FindFProperty<FBoolProperty>(GetClass(), MoverJumpEdge))
    {
        Edge->SetPropertyValue_InContainer(this, false);
    }
}

void ASampleMoverCharacter::HandleMoverLook(const FInputActionValue& Value)
{
    const FVector2D Delta = Value.Get<FVector2D>();
    AddControllerYawInput(Delta.X);
    AddControllerPitchInput(Delta.Y);
}

void ASampleMoverCharacter::HandleMoverGamepadLook(const FInputActionValue& Value)
{
    if (!IsMoverTwinStick(*this) && GetWorld())
    {
        const FVector2D Delta = Value.Get<FVector2D>() * GetWorld()->GetDeltaSeconds();
        AddControllerYawInput(Delta.X);
        AddControllerPitchInput(Delta.Y);
    }
}

void ASampleMoverCharacter::HandleMoverCrouch()
{
    const auto Mode = static_cast<EAuthoredMovementMode>(ResolveMovementModeValue());
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (Mover && (Mode == EAuthoredMovementMode::Grounded || Mode == EAuthoredMovementMode::Sliding))
    {
        WriteMoverIntent(*this, MoverCrouchIntent, !Mover->IsCrouching());
    }
}

void ASampleMoverCharacter::HandleMoverWalk()
{
    const auto* State = FindSnapshot(*this, MoverPlayerInputState);
    if (!ReadIntent(State, *this, MoverSprintIntent))
    {
        WriteMoverIntent(*this, MoverWalkIntent, !ReadIntent(State, *this, MoverWalkIntent));
    }
}

void ASampleMoverCharacter::HandleMoverSprint(const FInputActionValue& Value)
{
    const bool bSprint = Value.Get<bool>();
    WriteMoverIntent(*this, MoverSprintIntent, bSprint);
    WriteMoverIntent(*this, MoverWalkIntent, false);
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (bSprint && Mover && Mover->IsCrouching())
    {
        WriteMoverIntent(*this, MoverCrouchIntent, false);
    }
}

void ASampleMoverCharacter::HandleMoverStrafe()
{
    const auto* State = FindSnapshot(*this, MoverPlayerInputState);
    WriteMoverIntent(*this, MoverStrafeIntent, !ReadIntent(State, *this, MoverStrafeIntent));
}

void ASampleMoverCharacter::HandleMoverAim(const FInputActionValue& Value)
{
    WriteMoverIntent(*this, MoverAimIntent, Value.Get<bool>());
}

uint8 ASampleMoverCharacter::ResolveGaitValue() const
{
    const FStructProperty* Player = FindSnapshot(*this, MoverPlayerInputState);
    if (ReadIntent(Player, *this, MoverSprintIntent))
    {
        switch (GetPreSimRotationMode(*this))
        {
        case 0:
            return MoverGaitSprint;
        case 1:
            if (const FCharacterDefaultInputs* Input = GetPreSimInputs(*this))
            {
                const int32 Style = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.StrafeStyle"));
                const double Threshold = Style == 1 || Style == 2 ? -0.1 : 0.5;
                return FVector::DotProduct(Input->GetMoveInput(), Input->OrientationIntent) > Threshold ? MoverGaitSprint : MoverGaitRun;
            }
            return MoverGaitRun;
        case 2:
            return MoverGaitRun;
        default:
            return MoverGaitWalk;
        }
    }
    if (ReadIntent(Player, *this, TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3")))
    {
        return MoverGaitWalk;
    }
    const int32 AnalogStyle = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.AnalogInputStyle"));
    if (AnalogStyle == 0)
    {
        return MoverGaitRun;
    }
    const double Deflection = ReadNumber(GetClass(), this, TEXT("SmoothedAnalogInputAmount"));
    return AnalogStyle == 1 && Deflection > 0.7 ? MoverGaitRun : MoverGaitWalk;
}

bool ASampleMoverCharacter::PrepareMoverDirectionalOffset(double& Angle, uint8& Sector)
{
    Angle = 0.0;
    Sector = static_cast<uint8>(EAuthoredMovementSector::Forward);
    const auto* Defaults = GetPreSimInputs(*this);
    const auto* Snapshot = FindSnapshot(*this, TEXT("MoverCustomInputs_PreSim"));
    const int64 Rotation = GetPreSimRotationMode(*this);
    FVector Direction = FVector::ZeroVector;
    if (Defaults && Snapshot && Rotation != FaceMovement)
    {
        switch (static_cast<EAuthoredMovementMode>(ResolveMovementModeValue()))
        {
        case EAuthoredMovementMode::Grounded:
            Direction = Defaults->GetMoveInput();
            break;
        case EAuthoredMovementMode::Airborne:
        case EAuthoredMovementMode::Sliding:
            if (const auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
            {
                Direction = Mover->GetVelocity().GetSafeNormal2D(0.0001);
            }
            break;
        default:
            break;
        }
    }
    if (Direction.IsZero())
    {
        WriteNumber(GetClass(), this, TEXT("DebugAngle"), 0.0);
        return false;
    }

    const UScriptStruct* Type = Snapshot->Struct;
    const void* Data = Snapshot->ContainerPtrToValuePtr<void>(this);
    const auto Previous = ReadDirectionSector(Type, Data);
    const double IntentYaw = Defaults->OrientationIntent.Rotation().Yaw;
    Angle = FRotator::NormalizeAxis(Direction.Rotation().Yaw - IntentYaw);
    if (Previous == EAuthoredMovementSector::Forward)
    {
        const double Offset = ReadNumber(Type, Data, DirectionalOffsetField);
        const double Side = Offset == 0.0 ? FRotator::NormalizeAxis(GetActorRotation().Yaw - IntentYaw) : Offset;
        if (Side > 0.0 && Angle >= -180.0 && Angle <= -170.0)
        {
            Angle = 179.0;
        }
        else if (Side <= 0.0 && Angle >= 170.0 && Angle <= 180.0)
        {
            Angle = -179.0;
        }
    }
    WriteNumber(GetClass(), this, TEXT("DebugAngle"), Angle);

    const FMovementDirectionLimits Limits = ResolveDirectionLimits(Rotation, Previous);
    const auto* Gait = Type->FindPropertyByName(MoverGaitField);
    if ((Gait && ReadEnum(*Gait, Gait->ContainerPtrToValuePtr<void>(Data)) == MoverGaitSprint) ||
        FMath::Abs(Angle) <= Limits.Forward)
    {
        return true;
    }
    const auto Selected = FMath::Abs(Angle) > Limits.Backward ? EAuthoredMovementSector::Backward
        : (Angle < 0.0 ? EAuthoredMovementSector::Left : EAuthoredMovementSector::Right);
    Sector = static_cast<uint8>(Selected);
    return true;
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
    const auto* Jump = FindFProperty<FBoolProperty>(GetClass(), MoverJumpEdge);
    Input->bIsJumpJustPressed = Jump && Jump->GetPropertyValue_InContainer(this);
    if (UFunction* Rotation = FindFunction(TEXT("Get_RotationMode")))
    {
        FStructOnScope Args(Rotation);
        ProcessEvent(Rotation, Args.GetStructMemory());
        CopyEnum(CustomType->FindPropertyByName(TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")),
            CustomData, Rotation->GetReturnProperty(), Args.GetStructMemory());
    }
    Input->OrientationIntent = ResolveOrientationIntent();
    SetEnumByte(CustomType, CustomData, MoverGaitField, ResolveGaitValue());
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
        CopyEnum(CustomType->FindPropertyByName(MovementSectorField),
            CustomData, Direction->FindPropertyByName(TEXT("MovementDirection")), Args.GetStructMemory());
        WriteNumber(CustomType, CustomData, DirectionalOffsetField,
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

bool ASampleMoverCharacter::TryHandleMoverModeTransition(FName PreviousMovementModeName, FName NewMovementModeName)
{
    const auto* Map = FindMovementModeMap(*this);
    if (!Map || (!CastField<FEnumProperty>(Map->ValueProp) && !CastField<FByteProperty>(Map->ValueProp)))
    {
        return false;
    }

    const auto Previous = static_cast<EAuthoredMovementMode>(ReadMovementModeValue(*this, *Map, PreviousMovementModeName));
    const auto Next = static_cast<EAuthoredMovementMode>(ReadMovementModeValue(*this, *Map, NewMovementModeName));
    const bool bPlayLanding = Next == EAuthoredMovementMode::Grounded && Previous == EAuthoredMovementMode::Airborne;
    const bool bEnterAirborne = Next == EAuthoredMovementMode::Airborne;
    const bool bCheckJump = bEnterAirborne && Previous == EAuthoredMovementMode::Grounded;
    const bool bStartSlide = Next == EAuthoredMovementMode::Sliding;
    const bool bEndSlide = Previous == EAuthoredMovementMode::Sliding;
    const auto* PreSim = bCheckJump ? GetPreSimInputs(*this) : nullptr;
    if (bCheckJump && !PreSim)
    {
        return false;
    }
    const bool bPlayJump = bCheckJump && PreSim->bIsJumpJustPressed;

    const auto* MoverField = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("CharacterMover"));
    const auto* Mover = MoverField ? Cast<UCharacterMoverComponent>(MoverField->GetObjectPropertyValue_InContainer(this)) : nullptr;
    const auto* SlideAudio = bEndSlide ? FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("SlidingAudioComponent")) : nullptr;
    const UClass* Class = GetClass();
    // Validate every required hook before playing sound or changing crouch intent, so the caller can use its original graph on failure.
    if (((bPlayLanding || bPlayJump) && !IsValid(Mover)) ||
        (bPlayLanding && !Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(ASampleMoverCharacter, PlayMoverLandingFoley))) ||
        (bPlayJump && !Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(ASampleMoverCharacter, PlayMoverJumpFoley))) ||
        (bStartSlide && !Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(ASampleMoverCharacter, StartMoverSlideFoley))) ||
        (bEndSlide && (!SlideAudio || !SlideAudio->PropertyClass->IsChildOf(UAudioComponent::StaticClass()) ||
            !Class->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(ASampleMoverCharacter, FadeMoverSlideFoley)))))
    {
        return false;
    }

    if (bPlayLanding)
    {
        PlayMoverLandingFoley(Mover->GetVelocity().Z);
    }
    if (bEnterAirborne)
    {
        if (bPlayJump)
        {
            PlayMoverJumpFoley(Mover->GetVelocity().Size2D());
        }
        ClearAirborneCrouch();
    }
    if (bStartSlide)
    {
        StartMoverSlideFoley();
    }

    // The authored validated getter runs after the new-mode branch, including any new slide component assignment.
    if (bEndSlide)
    {
        if (auto* Audio = Cast<UAudioComponent>(SlideAudio->GetObjectPropertyValue_InContainer(this)); IsValid(Audio))
        {
            FadeMoverSlideFoley(Audio);
            ClearSlideCrouchForSprint();
        }
    }
    return true;
}

void ASampleMoverCharacter::ClearSlideCrouchForSprint()
{
    if (ReadIntent(FindSnapshot(*this, MoverPlayerInputState), *this, MoverSprintIntent))
    {
        WriteMoverIntent(*this, MoverCrouchIntent, false);
    }
}

void ASampleMoverCharacter::ClearAirborneCrouch()
{
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>(); Mover && Mover->IsCrouching())
    {
        WriteMoverIntent(*this, MoverCrouchIntent, false);
        Mover->UnCrouch();
    }
}

void ASampleMoverCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EndPairedInteractionCollision();
    if (auto* Variables = GEngine ? GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>() : nullptr)
    {
        Variables->OnDataDrivenCVarDelegate.RemoveDynamic(this, &ASampleMoverCharacter::HandleRagdollProfileVariable);
    }
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        Mover->OnPostFinalize.RemoveDynamic(this, &ASampleMoverCharacter::NotifyMoverModeFinalized);
        Mover->OnPreSimulationTick.RemoveDynamic(this, &ASampleMoverCharacter::ApplyMoverCrouchInput);
        Mover->OnBasedMovementApplied.RemoveDynamic(this, &ASampleMoverCharacter::CaptureBasedMovement);
    }
    Super::EndPlay(EndPlayReason);
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
