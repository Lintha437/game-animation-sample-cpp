#include "Characters/SampleMoverModes.h"

#include "Components/SceneComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintSpringMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

namespace
{
    const FName LocomotionInputType(TEXT("S_MoverCustomInputs"));
    const FName RagdollInputType(TEXT("S_MoverCustomInputs_Ragdoll"));
    const FName RagdollModeTransformField(TEXT("RagdollTransform_33_5FE542414D593629259855B867DEA655"));

    const FInstancedStruct* FindSampleMoverModeInput(const FMoverDataCollection& Command, FName TypeName = LocomotionInputType)
    {
        for (const TSharedPtr<FMoverDataStructBase>& Data : Command.GetDataArray())
        {
            if (!Data || Data->GetScriptStruct() != FMoverUserDefinedDataStruct::StaticStruct())
            {
                continue;
            }
            const auto& Snapshot = static_cast<const FMoverUserDefinedDataStruct&>(*Data).StructInstance;
            const UScriptStruct* Type = Snapshot.GetScriptStruct();
            if (Type && Type->GetFName() == TypeName)
            {
                return &Snapshot;
            }
        }
        return nullptr;
    }

    const FTransform* FindSampleRagdollModeTransform(const FMoverDataCollection& Command)
    {
        const auto* Snapshot = FindSampleMoverModeInput(Command, RagdollInputType);
        const auto* Field = Snapshot ? FindFProperty<FStructProperty>(Snapshot->GetScriptStruct(), RagdollModeTransformField) : nullptr;
        return Field && Field->Struct == TBaseStructure<FTransform>::Get()
            ? Field->ContainerPtrToValuePtr<FTransform>(Snapshot->GetMemory()) : nullptr;
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

    double ReadSampleModeNumber(const UObject& Object, FName Name, double Fallback = 0.0)
    {
        const auto* Field = FindFProperty<FNumericProperty>(Object.GetClass(), Name);
        return Field && Field->IsFloatingPoint()
            ? Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(&Object)) : Fallback;
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

    bool ReadSampleModeFlag(const UObject& Object, FName Name, bool bFallback = false)
    {
        const auto* Field = FindFProperty<FBoolProperty>(Object.GetClass(), Name);
        return Field ? Field->GetPropertyValue_InContainer(&Object) : bFallback;
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

USampleMoverWalkingMode::USampleMoverWalkingMode()
{
    MaxSpeedOverride = -1.0f;
    Acceleration = 1000.0f;
    Deceleration = 1500.0f;
    TurningStrength = 8.0f;
    FacingSmoothingTime = 0.5f;
    SharedSettingsClasses.AddUnique(UStanceSettings::StaticClass());
    Transitions.Add(CreateDefaultSubobject<USampleMoverEnterSlideTransition>(TEXT("EnterSlideTransition")));
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
        bJustLanded = true;
        SetSampleModeFlag(*this, TEXT("JustLanded"), true);
        UKismetSystemLibrary::RetriggerableDelay(this, 0.2f, FLatentActionInfo(0, MoverLandingWindowAction,
            GET_FUNCTION_NAME_STRING_CHECKED(USampleMoverWalkingMode, ClearMoverLandingWindow), this));
    }
}

void USampleMoverWalkingMode::ClearMoverLandingWindow()
{
    bJustLanded = false;
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
    const double TargetRunSpeed = ReadSampleModeNumber(*this, TEXT("RunSpeed"), WalkSettings.RunSpeed);
    const double TargetSprintSpeed = ReadSampleModeNumber(*this, TEXT("SprintSpeed"), WalkSettings.SprintSpeed);
    const float CurrentOffset = static_cast<float>((CurrentFacing.Rotator() - DesiredFacing.Rotator()).GetNormalized().Yaw);
    const FQuat Facing = OffsetSampleMoverFacing(DesiredFacing, CurrentOffset, Input);

    static const FGameplayTag RootMotion = FGameplayTag::RequestGameplayTag(TEXT("Mover.AnimRootMotion"), false);
    FVector Velocity = DesiredVelocity;
    if (Mover && RootMotion.IsValid() && Mover->HasGameplayTag(RootMotion, false))
    {
        Velocity = InOutVelocity;
    }
    else if (ReadSampleModeFlag(*this, TEXT("SlopeRelativeVelocity"), WalkSettings.bSlopeRelativeVelocity))
    {
        Velocity = FQuat::FindBetweenNormals(FVector::UpVector, GetSampleFloorNormal(Mover)).RotateVector(DesiredVelocity);
    }

    const double GroundSpeed = Gait == 2
        ? TargetSprintSpeed
        : (Gait == 1 ? TargetRunSpeed : ReadSampleModeNumber(*this, TEXT("WalkSpeed"), WalkSettings.WalkSpeed));
    MaxSpeedOverride = Mover && Mover->IsCrouching()
        ? ReadSampleModeNumber(*this, TEXT("CrouchSpeed"), WalkSettings.CrouchSpeed) : GroundSpeed;
    const double ActiveAcceleration = Gait == 0
        ? ReadSampleModeNumber(*this, TEXT("WalkAcceleration"), WalkSettings.WalkAcceleration)
        : (Gait == 2 && Speed > TargetRunSpeed
            ? ReadSampleModeNumber(*this, TEXT("SprintAcceleration"), WalkSettings.SprintAcceleration)
            : ReadSampleModeNumber(*this, TEXT("RunAcceleration"), WalkSettings.RunAcceleration));
    Acceleration = ActiveAcceleration;
    const bool bLandingBrake = bJustLanded || ReadSampleModeFlag(*this, TEXT("JustLanded"));
    Deceleration = bIdle
        ? (bLandingBrake ? 20000.0 : ReadSampleModeNumber(*this, TEXT("StoppingDeceleration"), WalkSettings.StoppingDeceleration))
        : ReadSampleModeNumber(*this, TEXT("GaitChangeDeceleration"), WalkSettings.GaitChangeDeceleration);
    TurningStrength = MapSampleModeRange(Speed, TargetRunSpeed, TargetSprintSpeed,
        ReadSampleModeNumber(*this, TEXT("Walk/RunTurnStrength"), WalkSettings.WalkRunTurnStrength),
        ReadSampleModeNumber(*this, TEXT("SprintTurnStrength"), WalkSettings.SprintTurnStrength));
    const double MovingFacingTime = MapSampleModeRange(Speed, TargetRunSpeed, TargetSprintSpeed,
        ReadSampleModeNumber(*this, TEXT("Walk/RunFacingTime"), WalkSettings.WalkRunFacingTime),
        ReadSampleModeNumber(*this, TEXT("SprintFacingTime"), WalkSettings.SprintFacingTime));
    FacingSmoothingTime = bIdle ? ReadSampleModeNumber(*this, TEXT("IdleFacingTime"), WalkSettings.IdleFacingTime)
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

USampleMoverSlidingMode::USampleMoverSlidingMode()
{
    MaxSpeedOverride = -1.0f;
    Acceleration = 2000.0f;
    Deceleration = 200.0f;
    TurningStrength = 1.0f;
    FacingSmoothingTime = 0.15f;
    SharedSettingsClasses.AddUnique(UStanceSettings::StaticClass());
    Transitions.Add(CreateDefaultSubobject<USampleMoverExitSlideTransition>(TEXT("ExitSlideTransition")));
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
    bInitialBoost = true;
    SetSampleModeFlag(*this, TEXT("InitialBoost"), true);
    UKismetSystemLibrary::Delay(this, ReadSampleModeNumber(*this, TEXT("InitialBoostTime"), SlideSettings.InitialBoostTime),
        FLatentActionInfo(0, MoverSlideBoostAction,
            GET_FUNCTION_NAME_STRING_CHECKED(USampleMoverSlidingMode, ClearMoverSlideBoost), this));
}

void USampleMoverSlidingMode::ClearMoverSlideBoost()
{
    bInitialBoost = false;
    SetSampleModeFlag(*this, TEXT("InitialBoost"), false);
}

void USampleMoverSlidingMode::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
    const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
    const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
    const auto* Input = FindSampleMoverModeInput(StartState.InputCmd.InputCollection);
    const FVector FloorNormal = GetSampleFloorNormal(GetMoverComponent());
    const double Slope = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FloorNormal, DesiredVelocity.GetSafeNormal()))) - 90.0;
    const double Shallow = ReadSampleModeNumber(*this, TEXT("ShallowSlopeAngle"), SlideSettings.ShallowSlopeAngle);
    const double Steep = ReadSampleModeNumber(*this, TEXT("SteepSlopeAngle"), SlideSettings.SteepSlopeAngle);
    const bool bBoost = bInitialBoost || ReadSampleModeFlag(*this, TEXT("InitialBoost"));
    const double SlopeSpeed = Slope > -Shallow
        ? ReadSampleModeNumber(*this, TEXT("FlatGroundSpeed"), SlideSettings.FlatGroundSpeed)
        : MapSampleModeRange(Slope, -Shallow, -Steep,
            ReadSampleModeNumber(*this, TEXT("ShallowSlopeSpeed"), SlideSettings.ShallowSlopeSpeed),
            ReadSampleModeNumber(*this, TEXT("SteepSlopeSpeed"), SlideSettings.SteepSlopeSpeed));
    MaxSpeedOverride = bBoost
        ? ReadSampleModeNumber(*this, TEXT("InitialBoostSpeed"), SlideSettings.InitialBoostSpeed) : SlopeSpeed;
    Acceleration = bBoost
        ? ReadSampleModeNumber(*this, TEXT("InitialBoostAcceleration"), SlideSettings.InitialBoostAcceleration)
        : ReadSampleModeNumber(*this, TEXT("AfterBoostAcceleration"), SlideSettings.AfterBoostAcceleration);
    Deceleration = MapSampleModeRange(Slope, Shallow, Steep,
        ReadSampleModeNumber(*this, TEXT("FlatGroundDeceleration"), SlideSettings.FlatGroundDeceleration),
        ReadSampleModeNumber(*this, TEXT("SteepSlopeDeceleration"), SlideSettings.SteepSlopeDeceleration));

    const double CurrentOffset = (CurrentFacing.Rotator() - DesiredFacing.Rotator()).GetNormalized().Yaw;
    const FQuat Facing = OffsetSampleMoverFacing(DesiredFacing, CurrentOffset, Input);
    Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, DesiredVelocity, Facing, CurrentFacing,
        InOutAngularVelocityDegrees, InOutVelocity);
}

void USampleMoverFlyingMode::GenerateFlyingMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
    const FVector& DesiredVelocity, const FQuat& DesiredFacing, const FQuat& CurrentFacing,
    FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
    InOutVelocity = UBlueprintSpringMathLibrary::DampVector(InOutVelocity, DesiredVelocity, DeltaSeconds, 0.1f);
    const FQuat Facing = UBlueprintSpringMathLibrary::DampQuat(CurrentFacing, DesiredFacing, DeltaSeconds);
    InOutAngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(
        CurrentFacing.Rotator(), Facing.Rotator(), DeltaSeconds);
}

void USampleMoverRagdollMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
    OutputState.MovementEndState.RemainingMs = 0.0f;
    USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
    if (!UpdatedComponent)
    {
        return;
    }

    const auto& Command = Params.StartState.InputCmd.InputCollection;
    const auto* Pelvis = FindSampleRagdollModeTransform(Command);
    const auto* Previous = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
    FVector Velocity = FVector::ZeroVector;
    FVector AngularVelocity = FVector::ZeroVector;

    if (Pelvis && Previous && DeltaSeconds > 0.0f)
    {
        constexpr double TraceDepth = 88.0;
        constexpr float TraceRadius = 30.0f;
        constexpr double CapsuleHeightFromGround = 86.0 + 1.9;
        FVector Location = Pelvis->GetLocation();
        const int32 Debug = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCVar.CharacterPhysics.DrawDebug"));
        FHitResult Ground;
        if (UKismetSystemLibrary::SphereTraceSingle(this, Location, Location - FVector(0.0, 0.0, TraceDepth),
            TraceRadius, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, {},
            Debug == 1 ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None, Ground, true))
        {
            Location.Z += CapsuleHeightFromGround - FMath::Abs(Ground.ImpactPoint.Z - Ground.TraceStart.Z);
        }

        const auto* Inputs = Command.FindDataByType<FCharacterDefaultInputs>();
        const FRotator Facing = Inputs ? Inputs->OrientationIntent.Rotation() : FRotator::ZeroRotator;
        const FVector Delta = Location - Previous->GetLocation_WorldSpace();
        FHitResult MoveHit;
        UMovementUtils::TrySafeMoveAndSlideUpdatedComponentNoMovementRecord(
            Params.MovingComps, Delta, Facing.Quaternion(), false, MoveHit, ETeleportType::None);
        Velocity = Delta / DeltaSeconds;
        AngularVelocity = UMovementUtils::ComputeAngularVelocityDegrees(
            Previous->GetOrientation_WorldSpace(), Facing, DeltaSeconds);
    }

    auto& State = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
    State = FMoverDefaultSyncState();
    State.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentRotation(),
        Velocity, AngularVelocity);
}
