#pragma once

#include "DefaultMovementSet/Modes/FallingMode.h"
#include "DefaultMovementSet/Modes/SimpleFlyingMode.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
#include "MovementModeTransition.h"
#include "SampleMoverModes.generated.h"

/** Adds the sample's facing intent to the stock Mover falling proposal. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverFallingMode : public UFallingMode
{
    GENERATED_BODY()

public:
    /** Runs stock air movement, then limits the turn toward the command's facing and rotation offset. */
    virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
        const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
};

/** Enters Sliding when a crouched pawn crosses the original horizontal speed threshold. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverEnterSlideTransition : public UBaseMovementModeTransition
{
    GENERATED_BODY()

public:
    /** @return Sliding above 380 cm/s with the crouch tag, or no transition. */
    virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};

/** Returns to Walking when a slide slows down or crouch ends. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverExitSlideTransition : public UBaseMovementModeTransition
{
    GENERATED_BODY()

public:
    /** @return Walking at or below 200 cm/s or when standing, or no transition. */
    virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};

/** Tuning parameters authored for the sample's grounded walking, running, sprinting, and crouch gaits. */
USTRUCT(BlueprintType)
struct GAMEANIMATIONSAMPLECPP_API FSampleMoverWalkingSettings
{
    GENERATED_BODY()

    /** Target movement speed when walking. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double WalkSpeed = 165.0;

    /** Target movement speed when running. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double RunSpeed = 375.0;

    /** Target movement speed when sprinting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SprintSpeed = 585.0;

    /** Target movement speed while crouching. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double CrouchSpeed = 165.0;

    /** Acceleration applied while walking. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double WalkAcceleration = 500.0;

    /** Acceleration applied while running. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double RunAcceleration = 800.0;

    /** Acceleration applied above run speed while sprinting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SprintAcceleration = 300.0;

    /** Deceleration applied when input stops outside the landing brake window. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double StoppingDeceleration = 1000.0;

    /** Deceleration applied while transitioning between gaits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double GaitChangeDeceleration = 300.0;

    /** Velocity turning strength at walk and run speeds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double WalkRunTurnStrength = 8.0;

    /** Velocity turning strength at sprint speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SprintTurnStrength = 4.0;

    /** Facing smoothing time at walk and run speeds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double WalkRunFacingTime = 0.4;

    /** Facing smoothing time at sprint speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SprintFacingTime = 0.8;

    /** Facing smoothing time while stationary. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double IdleFacingTime = 0.2;

    /** Rotates desired walking velocity to align with the current floor normal. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    bool bSlopeRelativeVelocity = true;
};

/** Applies the sample gait, crouch, braking, and facing policy to stock smooth walking. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverWalkingMode : public USmoothWalkingMode
{
    GENERATED_BODY()

public:
    /** Initializes the sample walking defaults, stance settings, and slide entry transition. */
    USampleMoverWalkingMode();

    /** Sample-authored gait, acceleration, braking, and facing parameters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    FSampleMoverWalkingSettings WalkSettings;

    /** Transient landing brake window state, active briefly after touching down from Falling. */
    UPROPERTY(Transient, BlueprintReadWrite, Category = "Sample|Mover")
    bool bJustLanded = false;

    /** Retains the authored landing window when coming from Falling. */
    virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext,
        const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

    /** Retains the landing callback's game-thread boundary for an asynchronous mode. */
    virtual void Activate_External(const FMoverEventContext& Context) override;

    /** Updates the mode settings for the current command, then runs the engine walking solver. */
    virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
        const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
        const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;

private:
    void BeginMoverLandingWindow();

    UFUNCTION()
    void ClearMoverLandingWindow();
};

/** Slope-dependent speed, boost, and braking parameters for sliding movement. */
USTRUCT(BlueprintType)
struct GAMEANIMATIONSAMPLECPP_API FSampleMoverSlidingSettings
{
    GENERATED_BODY()

    /** Minimum slope angle in degrees before downhill acceleration applies. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double ShallowSlopeAngle = 10.0;

    /** Slope angle in degrees at which maximum downhill slide speed and deceleration apply. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SteepSlopeAngle = 40.0;

    /** Duration in seconds of the initial slide entry speed boost. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double InitialBoostTime = 0.2;

    /** Target slide speed on flat ground after the initial boost ends. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double FlatGroundSpeed = 100.0;

    /** Target slide speed at the shallow slope angle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double ShallowSlopeSpeed = 500.0;

    /** Target slide speed at the steep slope angle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SteepSlopeSpeed = 800.0;

    /** Target slide speed while the entry boost is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double InitialBoostSpeed = 800.0;

    /** Acceleration applied while the entry boost is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double InitialBoostAcceleration = 2000.0;

    /** Acceleration applied after the entry boost expires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double AfterBoostAcceleration = 300.0;

    /** Deceleration applied on flat ground. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double FlatGroundDeceleration = 500.0;

    /** Deceleration applied on steep slopes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    double SteepSlopeDeceleration = 2000.0;
};

/** Uses the sample slope tuning and entry boost with the stock smooth walking solver. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverSlidingMode : public USmoothWalkingMode
{
    GENERATED_BODY()

public:
    /** Initializes the sample sliding defaults, stance settings, and slide exit transition. */
    USampleMoverSlidingMode();

    /** Sample-authored slope speed, entry boost, and braking parameters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample|Mover")
    FSampleMoverSlidingSettings SlideSettings;

    /** Transient slide entry boost flag, cleared after InitialBoostTime elapses. */
    UPROPERTY(Transient, BlueprintReadWrite, Category = "Sample|Mover")
    bool bInitialBoost = false;

    /** Starts the initial boost and schedules its expiry. */
    virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext,
        const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

    /** Retains the boost callback's game-thread boundary for an asynchronous mode. */
    virtual void Activate_External(const FMoverEventContext& Context) override;

    /** Updates slide speed, acceleration and braking from the authored boost and slope settings. */
    virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
        const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
        const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;

private:
    void BeginMoverSlideBoost();

    UFUNCTION()
    void ClearMoverSlideBoost();
};

/** Smooths the sample's flight inputs while the stock mode owns movement and collision. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverFlyingMode : public USimpleFlyingMode
{
    GENERATED_BODY()

public:
    /** Dampens linear velocity and derives angular velocity from the damped facing. */
    virtual void GenerateFlyingMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
        const FVector& DesiredVelocity, const FQuat& DesiredFacing, const FQuat& CurrentFacing,
        FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;
};

/** Follows the physics-driven pelvis using the ragdoll transform carried by each Mover command. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverRagdollMode : public UBaseMovementMode
{
    GENERATED_BODY()

public:
    /**
     * Uses a downward sphere trace to lift the capsule off nearby ground and publishes its
     * resulting world transform without a movement base. Consumes the full step. If the
     * command has no ragdoll snapshot, the capsule holds position until the next command.
     */
    virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
};
