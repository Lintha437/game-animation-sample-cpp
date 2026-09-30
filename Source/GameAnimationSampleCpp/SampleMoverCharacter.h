#pragma once

#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "SampleMoverCharacter.generated.h"

class UInputAction;
struct FInputActionValue;

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

    /**
     * Samples navigation intent for AI and camera-relative device input for players.
     * AI navigation data are consumed at the command boundary. Flying also uses camera pitch.
     * @return Normalized world-space movement intent.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Input")
    FVector ResolveMoverDirection();

    /** @return Authored facing mode from the current target, aim, strafe and twin-stick state. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Input")
    uint8 ResolveMoverRotationMode() const;

    /** Re-evaluates the nearest registered target and updates the sample's target marker. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Input")
    void UpdateMoverTarget();

    /** Registers an actor with the original target candidate list. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Input")
    void AddMoverTarget(AActor* TargetToAdd);

    /** Removes every matching entry from the original target candidate list. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Input")
    void RemoveMoverTarget(AActor* TargetToRemove);

    /** Updates the twin-stick control mode and heading from the current aim action value. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Input")
    void UpdateMoverTwinStick();

    /** Updates the analog deflection with the authored rise and decay speeds. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Input")
    void UpdateMoverAnalogDeflection();

    /** Replaces the navigation task's input snapshot with the requested walk or run intent. */
    void ApplyNpcWalkIntent(bool bWalk);

    /** @return Whether movement and the active montage slot allow a new traversal request. */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Input")
    bool CanStartMoverTraversal() const;

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

    /**
     * Rebuilds the imported interaction candidates in the initiator's forward arc.
     * The list is empty when the initiator cannot act. Each accepted pawn is grounded,
     * within 50 cm vertically, and reachable with the sample capsule profile.
     * The imported chooser and pose search consume this list for paired animation.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void UpdateCharacterInteractionCandidates();

    /** Starts physics immediately, queues the Ragdoll mode, and freezes capsule facing briefly. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void BeginRagdollPhysics();

    /** Queues the grounded mode. The finalized transition owns collision and profile recovery. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void RequestRagdollRecovery();

    /** Applies the ragdoll capsule profile and suppresses pawn-to-pawn collision while down. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void ApplyRagdollCapsuleCollision();

    /** Restores the normal capsule profile before the recovery pose is captured. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void RestoreRagdollCapsuleCollision();

    /** Suppresses motor velocity for the recovery frame and schedules restoration of the default profile. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void FinishRagdollPhysicsRecovery();

    /** Updates the default profile from the sample CVar. Applies it immediately outside ragdoll. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void RefreshRagdollDefaultProfile();

    /**
     * Applies the named Physics Control recipe and the corresponding leg collision policy.
     * @param PhysicsProfile Authored profile name. Ragdoll enables collisions between the leg groups.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void ApplyRagdollProfile(FName PhysicsProfile);

    /** Checks the neck-to-feet obstacle volume and starts the authored ragdoll reaction on a hit. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void TraceRagdollObstacles();

protected:
    /** Binds sampled direction values and the migrated player events on local possession. */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    /** Enable after removing the equivalent Blueprint action events from the local asset. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    bool bUseNativeMoverDeviceInput = false;

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverMoveAction{FSoftObjectPath(TEXT("/Game/Input/IA_Move.IA_Move"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverTwinStickAimAction{FSoftObjectPath(TEXT("/Game/Input/IA_TwinStick_AimDirection.IA_TwinStick_AimDirection"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverLookAction{FSoftObjectPath(TEXT("/Game/Input/IA_Look.IA_Look"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverGamepadLookAction{FSoftObjectPath(TEXT("/Game/Input/IA_Look_Gamepad.IA_Look_Gamepad"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverCrouchAction{FSoftObjectPath(TEXT("/Game/Input/IA_Crouch.IA_Crouch"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverWalkAction{FSoftObjectPath(TEXT("/Game/Input/IA_Walk.IA_Walk"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverSprintAction{FSoftObjectPath(TEXT("/Game/Input/IA_Sprint.IA_Sprint"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverStrafeAction{FSoftObjectPath(TEXT("/Game/Input/IA_Strafe.IA_Strafe"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverAimAction{FSoftObjectPath(TEXT("/Game/Input/IA_Aim.IA_Aim"))};

    /** Enable after the primary and ragdoll pawn jump events are removed. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    bool bUseNativeMoverJumpInput = false;

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverJumpAction{FSoftObjectPath(TEXT("/Game/Input/IA_Jump.IA_Jump"))};

    void HandleMoverLook(const FInputActionValue& Value);
    void HandleMoverGamepadLook(const FInputActionValue& Value);
    void HandleMoverCrouch();
    void HandleMoverWalk();
    void HandleMoverSprint(const FInputActionValue& Value);
    void HandleMoverStrafe();
    void HandleMoverAim(const FInputActionValue& Value);
    FVector2D ReadMoverAction(const TSoftObjectPtr<UInputAction>& Action) const;

    /** Handles press-time traversal, jump, stance exit, or ragdoll recovery. */
    void HandleMoverJumpStarted();

    /** Retries traversal while the action remains triggered. */
    void HandleMoverJumpTriggered();

    /** @return True when the authored traversal call accepted the approach and selected a montage. */
    bool TryMoverTraversal(bool bDrawForDuration);

    /** Clears the one-frame jump edge after it is available to Mover's input producer. */
    UFUNCTION()
    void ClearMoverJumpEdge();

    /** Physics body names on the sample skeleton. Adjust both groups when using another rig. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Ragdoll")
    TArray<FName> LeftLegPhysicsBodies{TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l")};

    /** Physics body names paired with the left leg group when switching collision policy. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Ragdoll")
    TArray<FName> RightLegPhysicsBodies{TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")};

    UFUNCTION()
    void ConfirmRagdollEntry();

    UFUNCTION()
    void ReleaseRagdollFacing();

    UFUNCTION()
    void RestoreRagdollDefaultProfile();
};
