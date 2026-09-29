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

/** Applies the sample gait, crouch, braking, and facing policy to stock smooth walking. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverWalkingMode : public USmoothWalkingMode
{
    GENERATED_BODY()

public:
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

/** Uses the sample slope tuning and entry boost with the stock smooth walking solver. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverSlidingMode : public USmoothWalkingMode
{
    GENERATED_BODY()

public:
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
