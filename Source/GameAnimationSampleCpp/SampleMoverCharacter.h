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
};
