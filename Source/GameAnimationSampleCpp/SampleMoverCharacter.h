#pragma once

#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "SampleMoverCharacter.generated.h"

/** Native state queries and gameplay movement for the reference Mover pawn. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleMoverCharacter : public APawn
{
    GENERATED_BODY()

public:
    /**
     * Converts the current Mover mode name using the pawn's authored state map.
     * @return Value in the imported movement-mode enum. A missing entry returns zero.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover")
    uint8 ResolveMovementModeValue() const;

    /**
     * Resolves gait from the input snapshot immediately before simulation.
     * Sprint takes precedence over the walk toggle, aim caps gait at run, and
     * strafe uses the authored direction test. Dual-gait analog input uses the
     * smoothed deflection and a strict 0.7 threshold.
     * @return Value in the imported gait enum.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover")
    uint8 ResolveGaitValue() const;

    /** @return Horizontal speed from the Mover simulation, in centimeters per second. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover")
    double Get_Speed() const;

    /**
     * Applies the sandbox teleport and preserves the next simulation orientation.
     * @param Transform World target or world-space offset, selected by Offset.
     * @param Offset Applies a relative move if true. An absolute move tests the destination.
     * @param UpdateInitialTransform Updates the sandbox reset transform after movement.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void CustomTeleport(FTransform Transform, bool Offset, bool UpdateInitialTransform);

    /**
     * Assembles the original default and user-defined blocks for Mover.
     * Evaluates the retained device and animation-facing queries on the game thread
     * in the same order as the reference input producer.
     * @param SimTimeMs Time supplied by the Mover input producer interface.
     * @return Command containing copies of the completed input blocks.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    FMoverInputCmdContext BuildMoverInputCommand(int32 SimTimeMs);

    /** Copies the latest simulated blocks to the snapshots consumed by animation and camera. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void UpdateCachedMoverInputs();

    /**
     * Applies the crouch request from the command about to be simulated.
     * @param TimeStep Simulation step that will consume the command.
     * @param InputCmd Command produced by the local or remote controller.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void ApplyMoverCrouchInput(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);

    /** @return Target yaw, twin-stick aim, or control rotation in that precedence order. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover")
    FRotator ResolveAimingRotation() const;

    /** Caches the normalized control yaw rate in degrees per world second. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void UpdateControlYawRate();

    /**
     * Resolves the gameplay facing for the next simulation step.
     * Preserves stationary aim hysteresis, moving-base rotation, and root-motion priority.
     * @return Desired facing in world space.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover")
    FVector ResolveOrientationIntent() const;

    /** Updates the blocking floor, contact point, and surface normal from Mover. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void UpdateMoverFloor();

    /**
     * Records the moving base's transform delta for stationary facing.
     * @param TransformDelta Change applied by Mover for this step.
     * @param TimeStep Simulation step supplying the change.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void CaptureBasedMovement(const FTransform& TransformDelta, const FMoverTimeStep& TimeStep);

    /**
     * Notifies the authored presentation callback once per finalized mode change.
     * @param SyncState Finalized simulation state.
     * @param AuxState Finalized auxiliary state.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void NotifyMoverModeFinalized(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState);

    /**
     * Adds a ragdoll transform snapshot and facing to the regular movement command.
     * The extra block is present only when Ragdoll is the next simulation mode.
     * @param SimTimeMs Time supplied by the Mover input producer interface.
     * @return A command that owns copies of both the standard and ragdoll input blocks.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    FMoverInputCmdContext BuildRagdollInputCommand(int32 SimTimeMs);

    /** @return The pelvis position and upright capsule heading inferred from the simulated spine, in world space. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Ragdoll")
    FTransform ResolveRagdollTransform() const;

    /** @return World-space capsule facing from the frozen heading, roll velocity, or latest ragdoll transform. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Ragdoll")
    FVector ResolveRagdollFacing() const;

    /** Records center-of-mass velocity, spine velocity and spine speed for downstream physics and animation. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdateRagdollVelocities();

    /** Applies the configured gravity multiplier until the ragdoll reaches its downward speed limit. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdateRagdollGravity();

    /** @return Whether this grounded pawn can start an interaction given its aim intent and active montage slot. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Ragdoll")
    bool CanStartRagdollCharacterInteraction() const;

    /** @return Whether another character may select this grounded pawn as a target. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Ragdoll")
    bool CanReceiveRagdollCharacterInteraction() const;
};
