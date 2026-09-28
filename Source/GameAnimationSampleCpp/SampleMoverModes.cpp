#include "SampleMoverModes.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

namespace
{
    const FInstancedStruct* FindSampleMoverModeInput(const FMoverDataCollection& Command)
    {
        for (const TSharedPtr<FMoverDataStructBase>& Data : Command.GetDataArray())
        {
            if (!Data || Data->GetScriptStruct() != FMoverUserDefinedDataStruct::StaticStruct())
            {
                continue;
            }
            const auto& Snapshot = static_cast<const FMoverUserDefinedDataStruct&>(*Data).StructInstance;
            const UScriptStruct* Type = Snapshot.GetScriptStruct();
            if (Type && Type->GetFName() == TEXT("S_MoverCustomInputs"))
            {
                return &Snapshot;
            }
        }
        return nullptr;
    }

    constexpr int32 MoverLandingWindowAction = 0x4D4F5701;
    constexpr int32 MoverSlideBoostAction = 0x4D4F5702;
    const FName RotationOffsetField(TEXT("RotationOffset_14_9438753848B6A37A4F76569318195C60"));

    double ReadSampleModeNumber(const UStruct* Type, const void* Data, FName Name)
    {
        const auto* Field = Type ? FindFProperty<FNumericProperty>(Type, Name) : nullptr;
        return Field && Field->IsFloatingPoint()
            ? Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(Data)) : 0.0;
    }

    double ReadSampleModeNumber(const UObject& Object, FName Name)
    {
        return ReadSampleModeNumber(Object.GetClass(), &Object, Name);
    }

    double ReadSampleModeNumber(const FInstancedStruct* Input, FName Name)
    {
        return Input ? ReadSampleModeNumber(Input->GetScriptStruct(), Input->GetMemory(), Name) : 0.0;
    }

    int64 ReadSampleModeGait(const FInstancedStruct* Input)
    {
        if (!Input)
        {
            return 0;
        }
        const auto* Field = Input->GetScriptStruct()->FindPropertyByName(TEXT("Gait_5_13BB29FB446593C2412547A541C4C00B"));
        if (const auto* Byte = CastField<FByteProperty>(Field))
        {
            return Byte->GetPropertyValue_InContainer(Input->GetMemory());
        }
        if (const auto* Enum = CastField<FEnumProperty>(Field))
        {
            return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(Input->GetMemory()));
        }
        return 0;
    }

    bool ReadSampleModeFlag(const UObject& Object, FName Name)
    {
        const auto* Field = FindFProperty<FBoolProperty>(Object.GetClass(), Name);
        return Field && Field->GetPropertyValue_InContainer(&Object);
    }

    void SetSampleModeFlag(UObject& Object, FName Name, bool bValue)
    {
        if (const auto* Field = FindFProperty<FBoolProperty>(Object.GetClass(), Name))
        {
            Field->SetPropertyValue_InContainer(&Object, bValue);
        }
    }

    FVector GetSampleFloorNormal(const UMoverComponent* Mover)
    {
        FHitResult Hit;
        if (Mover)
        {
            Mover->TryGetFloorCheckHitResult(Hit);
        }
        return Hit.Normal;
    }

    FQuat OffsetSampleMoverFacing(const FQuat& Facing, double CurrentOffset, const FInstancedStruct* Input)
    {
        const double Offset = FMath::Clamp(ReadSampleModeNumber(Input, RotationOffsetField),
            CurrentOffset - 179.0, CurrentOffset + 179.0);
        return FRotator(0.0, Offset, 0.0).Quaternion() * Facing;
    }

    double MapSampleModeRange(double Value, double InputMin, double InputMax, double OutputMin, double OutputMax)
    {
        return FMath::GetMappedRangeValueClamped(FVector2D(InputMin, InputMax), FVector2D(OutputMin, OutputMax), Value);
    }
}

void USampleMoverFallingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext,
    const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
    Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
    const FMoverDataCollection& Command = StartState.InputCmd.InputCollection;
    const auto* Input = Command.FindDataByType<FCharacterDefaultInputs>();
    const auto* State = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    FRotator Facing = Input ? Input->OrientationIntent.Rotation() : FRotator::ZeroRotator;
    Facing.Yaw += ReadSampleModeNumber(FindSampleMoverModeInput(Command), RotationOffsetField);
    const FRotator Current = State ? State->GetOrientation_BaseSpace() : FRotator::ZeroRotator;
    const UWorld* World = GetWorld();
    const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
    OutProposedMove.AngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(Current, Facing, DeltaSeconds, 300.0f);
}

FTransitionEvalResult USampleMoverEnterSlideTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
    const UMoverComponent* Mover = Params.MovingComps.MoverComponent.Get();
    static const FGameplayTag Crouching = FGameplayTag::RequestGameplayTag(TEXT("Mover.Stance.IsCrouching"), false);
    return Mover && Crouching.IsValid() && Mover->HasGameplayTag(Crouching, false) &&
        Params.ProposedMove.LinearVelocity.SizeSquared2D() > FMath::Square(380.0)
        ? FTransitionEvalResult(TEXT("Sliding")) : FTransitionEvalResult::NoTransition;
}

FTransitionEvalResult USampleMoverExitSlideTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
    const auto* Mover = Cast<UCharacterMoverComponent>(Params.MovingComps.MoverComponent.Get());
    return !Mover || !Mover->IsCrouching() || Params.ProposedMove.LinearVelocity.SizeSquared2D() <= FMath::Square(200.0)
        ? FTransitionEvalResult(TEXT("Walking")) : FTransitionEvalResult::NoTransition;
}

void USampleMoverWalkingMode::Activate(const FMoverEventContext& Context, FName PrevModeName,
    const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState,
    FMoverAuxStateContext* OutAuxState)
{
    Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);
    if (!bSupportsAsync)
    {
        BeginMoverLandingWindow();
    }
}

void USampleMoverWalkingMode::Activate_External(const FMoverEventContext& Context)
{
    Super::Activate_External(Context);
    if (bSupportsAsync)
    {
        BeginMoverLandingWindow();
    }
}

void USampleMoverWalkingMode::BeginMoverLandingWindow()
{
    if (const auto* Mover = GetMoverComponent(); Mover && Mover->GetMovementModeName() == TEXT("Falling"))
    {
        SetSampleModeFlag(*this, TEXT("JustLanded"), true);
        UKismetSystemLibrary::RetriggerableDelay(this, 0.2f, FLatentActionInfo(0, MoverLandingWindowAction,
            GET_FUNCTION_NAME_STRING_CHECKED(USampleMoverWalkingMode, ClearMoverLandingWindow), this));
    }
}

void USampleMoverWalkingMode::ClearMoverLandingWindow()
{
    SetSampleModeFlag(*this, TEXT("JustLanded"), false);
}

void USampleMoverWalkingMode::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
    const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
    const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
    const auto* Mover = GetMoverComponent<UCharacterMoverComponent>();
    const FMoverDataCollection& Command = StartState.InputCmd.InputCollection;
    const auto* DefaultInput = Command.FindDataByType<FCharacterDefaultInputs>();
    const auto* Input = FindSampleMoverModeInput(Command);
    const int64 Gait = ReadSampleModeGait(Input);
    const bool bIdle = !DefaultInput || DefaultInput->GetMoveInput().IsNearlyZero();
    const double Speed = InOutVelocity.Size2D();
    const double RunSpeed = ReadSampleModeNumber(*this, TEXT("RunSpeed"));
    const double SprintSpeed = ReadSampleModeNumber(*this, TEXT("SprintSpeed"));
    const float CurrentOffset = static_cast<float>((CurrentFacing.Rotator() - DesiredFacing.Rotator()).GetNormalized().Yaw);
    const FQuat Facing = OffsetSampleMoverFacing(DesiredFacing, CurrentOffset, Input);

    static const FGameplayTag RootMotion = FGameplayTag::RequestGameplayTag(TEXT("Mover.AnimRootMotion"), false);
    FVector Velocity = DesiredVelocity;
    if (Mover && RootMotion.IsValid() && Mover->HasGameplayTag(RootMotion, false))
    {
        Velocity = InOutVelocity;
    }
    else if (ReadSampleModeFlag(*this, TEXT("SlopeRelativeVelocity")))
    {
        Velocity = FQuat::FindBetweenNormals(FVector::UpVector, GetSampleFloorNormal(Mover)).RotateVector(DesiredVelocity);
    }

    const double GroundSpeed = Gait == 2 ? SprintSpeed : (Gait == 1 ? RunSpeed : ReadSampleModeNumber(*this, TEXT("WalkSpeed")));
    MaxSpeedOverride = Mover && Mover->IsCrouching() ? ReadSampleModeNumber(*this, TEXT("CrouchSpeed")) : GroundSpeed;
    const FName AccelerationName = Gait == 0 ? FName(TEXT("WalkAcceleration"))
        : (Gait == 2 && Speed > RunSpeed ? FName(TEXT("SprintAcceleration")) : FName(TEXT("RunAcceleration")));
    Acceleration = ReadSampleModeNumber(*this, AccelerationName);
    Deceleration = bIdle
        ? (ReadSampleModeFlag(*this, TEXT("JustLanded")) ? 20000.0 : ReadSampleModeNumber(*this, TEXT("StoppingDeceleration")))
        : ReadSampleModeNumber(*this, TEXT("GaitChangeDeceleration"));
    TurningStrength = MapSampleModeRange(Speed, RunSpeed, SprintSpeed,
        ReadSampleModeNumber(*this, TEXT("Walk/RunTurnStrength")), ReadSampleModeNumber(*this, TEXT("SprintTurnStrength")));
    const double MovingFacingTime = MapSampleModeRange(Speed, RunSpeed, SprintSpeed,
        ReadSampleModeNumber(*this, TEXT("Walk/RunFacingTime")), ReadSampleModeNumber(*this, TEXT("SprintFacingTime")));
    FacingSmoothingTime = bIdle ? ReadSampleModeNumber(*this, TEXT("IdleFacingTime"))
        : MapSampleModeRange(FMath::Abs(CurrentOffset), 90.0, 135.0, MovingFacingTime, 0.2);

    Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, Velocity, Facing, CurrentFacing,
        InOutAngularVelocityDegrees, InOutVelocity);
    const double ControlRate = ReadSampleModeNumber(Input, TEXT("ControlRotationRate_29_A6F8894147153250CDA5C889CD9DB171"));
    if (CurrentOffset >= 135.0f)
    {
        InOutAngularVelocityDegrees.Z = FMath::Clamp(InOutAngularVelocityDegrees.Z, -1000.0, ControlRate);
    }
    else if (CurrentOffset <= -135.0f)
    {
        InOutAngularVelocityDegrees.Z = FMath::Clamp(InOutAngularVelocityDegrees.Z, ControlRate, 1000.0);
    }
}

void USampleMoverSlidingMode::Activate(const FMoverEventContext& Context, FName PrevModeName,
    const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState,
    FMoverAuxStateContext* OutAuxState)
{
    Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);
    if (!bSupportsAsync)
    {
        BeginMoverSlideBoost();
    }
}

void USampleMoverSlidingMode::Activate_External(const FMoverEventContext& Context)
{
    Super::Activate_External(Context);
    if (bSupportsAsync)
    {
        BeginMoverSlideBoost();
    }
}

void USampleMoverSlidingMode::BeginMoverSlideBoost()
{
    SetSampleModeFlag(*this, TEXT("InitialBoost"), true);
    UKismetSystemLibrary::Delay(this, ReadSampleModeNumber(*this, TEXT("InitialBoostTime")),
        FLatentActionInfo(0, MoverSlideBoostAction,
            GET_FUNCTION_NAME_STRING_CHECKED(USampleMoverSlidingMode, ClearMoverSlideBoost), this));
}

void USampleMoverSlidingMode::ClearMoverSlideBoost()
{
    SetSampleModeFlag(*this, TEXT("InitialBoost"), false);
}

void USampleMoverSlidingMode::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
    const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
    const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
    const auto* Input = FindSampleMoverModeInput(StartState.InputCmd.InputCollection);
    const FVector FloorNormal = GetSampleFloorNormal(GetMoverComponent());
    const double Slope = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FloorNormal, DesiredVelocity.GetSafeNormal()))) - 90.0;
    const double Shallow = ReadSampleModeNumber(*this, TEXT("ShallowSlopeAngle"));
    const double Steep = ReadSampleModeNumber(*this, TEXT("SteepSlopeAngle"));
    const bool bBoost = ReadSampleModeFlag(*this, TEXT("InitialBoost"));
    const double SlopeSpeed = Slope > -Shallow ? ReadSampleModeNumber(*this, TEXT("FlatGroundSpeed"))
        : MapSampleModeRange(Slope, -Shallow, -Steep, ReadSampleModeNumber(*this, TEXT("ShallowSlopeSpeed")),
            ReadSampleModeNumber(*this, TEXT("SteepSlopeSpeed")));
    MaxSpeedOverride = bBoost ? ReadSampleModeNumber(*this, TEXT("InitialBoostSpeed")) : SlopeSpeed;
    Acceleration = ReadSampleModeNumber(*this, bBoost ? FName(TEXT("InitialBoostAcceleration")) : FName(TEXT("AfterBoostAcceleration")));
    Deceleration = MapSampleModeRange(Slope, Shallow, Steep, ReadSampleModeNumber(*this, TEXT("FlatGroundDeceleration")),
        ReadSampleModeNumber(*this, TEXT("SteepSlopeDeceleration")));

    const double CurrentOffset = (CurrentFacing.Rotator() - DesiredFacing.Rotator()).GetNormalized().Yaw;
    const FQuat Facing = OffsetSampleMoverFacing(DesiredFacing, CurrentOffset, Input);
    Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, DesiredVelocity, Facing, CurrentFacing,
        InOutAngularVelocityDegrees, InOutVelocity);
}
