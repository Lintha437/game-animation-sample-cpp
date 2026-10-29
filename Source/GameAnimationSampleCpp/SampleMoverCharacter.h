#pragma once

#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "SampleMoverCharacter.generated.h"

class UInputAction;
class USkeletalMeshComponent;
struct FKey;
struct FInputActionValue;

/** Native state queries and gameplay movement for the reference Mover pawn. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleMoverCharacter : public APawn
{
    GENERATED_BODY()

public:
    /** Installs the sandbox input context for the current local controller. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Possession")
    void InstallSandboxInputMapping();

    /** Selects the authored Gameplay Camera or legacy camera during possession. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Possession")
    void ActivateSandboxCamera();

    /** Binds authored component lifecycles, seeds facing, and captures the reset transform. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void InitializeMoverRuntime();

    /** Updates the gameplay snapshots and invokes presentation in the authored tick order. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void UpdateMoverRuntime();

    /** Clears crouch intent after the slide audio fades if sprint is requested. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void ClearSlideCrouchForSprint();

    /** Clears the stored crouch request and uncrouches if the finalized airborne transition began crouched. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    void ClearAirborneCrouch();

    /** Initializes Mover first, then installs the post-animation and Physics Control prerequisites. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void InitializeRagdollRuntime();

    /** Updates inherited Mover state, then ragdoll physics, obstacle checks, and interaction candidates. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdateRagdollRuntime();

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

    /**
     * Prepares the facing sector and sampled heading for the authored rotation-offset Chooser.
     * @param Angle Signed angle from orientation intent to desired movement, in degrees.
     * @param Sector Value in the imported movement-direction enum.
     * @return True if the authored Chooser should run. False yields the forward sector with zero offset.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover")
    bool PrepareMoverDirectionalOffset(double& Angle, uint8& Sector);

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

    /**
     * Predicts the ragdoll's next contact from the trace socket and spine velocity.
     * The authored animation graph reads the time and socket-local yaw and pitch.
     * With a clear path the snapshot describes the final prediction sample.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdatePredictedRagdollImpact();

    /** Applies the configured gravity multiplier until the ragdoll reaches its downward speed limit. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdateRagdollGravity();

    /** Updates the ragdoll physics inputs in the original order and schedules eligible NPC recovery. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdateRagdollBehavior();

    /** Combines move input and slope force, then applies spine torque while physics is active. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void UpdateRagdollRollForce();

    /** Queues one three-second wait before checking whether the ragdoll has settled. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void ScheduleRagdollRecoveryCheck();

    /** Toggles both movement and ragdoll gravity on pawns with the sample's ragdoll settings. */
    void ApplySandboxZeroGravity(bool bEnabled);

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

    /** Passes the current candidate array and request type to authored paired-animation selection. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void RequestCharacterInteraction(FName InteractionType);

    /**
     * Rotates a Mover pawn under the first player's view, or spawns a target if the trace hit no pawn.
     * The sandbox retains three spawn attempts and retires the oldest target when a fourth is recorded.
     * @param DemoCharacterClass Authored target class configured on the ragdoll demo asset.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void SpawnOrRotateRagdollDemoCharacter(TSubclassOf<ASampleMoverCharacter> DemoCharacterClass);

    /**
     * Queries the evaluated demo camera child on the game thread.
     * @param Origin World-space camera position, or zero if the child is absent.
     * @param Direction Camera forward unit vector, or zero if the child is absent.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Mover|Interaction")
    void ResolveRagdollDemoAim(FVector& Origin, FVector& Direction) const;

    /** Draws the one-frame debug reticle while the authored aim intent is active. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void DrawRagdollDemoCrosshair() const;

    /** Traces the demo camera ray, requests the authored injury state, then applies impulse at the hit bone. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void ApplyRagdollDemoHit();

    /** Tracks the capsule ignores and mesh tick dependency owned by this paired action. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void BeginPairedInteractionCollision(UPrimitiveComponent* PartnerCapsule);

    /** Releases only the overrides installed by pairs involving this pawn. Safe to call repeatedly. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Interaction")
    void EndPairedInteractionCollision();

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

    /** Applies the default-profile change only for the authored physics console variable. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void HandleRagdollProfileVariable(FString CVarName);

    /** Evaluates the contact thresholds and forwards qualifying impacts to the authored Foley event. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Mover|Ragdoll")
    void HandleRagdollContact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        FVector NormalImpulse, const FHitResult& Hit);

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
    /** Releases active pairs before the pawn or its components tear down. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    /** Binds sampled direction values and the migrated player events on local possession. */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    /** Enable after the authored camera-style key group is removed. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Possession")
    bool bUseNativeCameraStyleKeys = false;

    /** Enable after removing the authored flight toggle bindings. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    bool bUseNativeFlightToggle = false;

    /** Queues the opposite flight state through the Mover simulation. */
    void ToggleMoverFlight();
    void HandleMoverFlightDebugKey(FKey Key, FInputActionValue Value);

    struct FPairedInteraction
    {
        TWeakObjectPtr<ASampleMoverCharacter> Initiator;
        TWeakObjectPtr<ASampleMoverCharacter> Partner;
        TWeakObjectPtr<UPrimitiveComponent> InitiatorCapsule;
        TWeakObjectPtr<UPrimitiveComponent> PartnerCapsule;
        TWeakObjectPtr<USkeletalMeshComponent> InitiatorMesh;
        TWeakObjectPtr<USkeletalMeshComponent> PartnerMesh;
        bool bAddedInitiatorIgnore = false;
        bool bAddedPartnerIgnore = false;
        bool bAddedTickPrerequisite = false;
    };

    TArray<TSharedPtr<FPairedInteraction>> PairedInteractions;

    /** Local sandbox targets in spawn order. Their world owns the actors. */
    TArray<TWeakObjectPtr<ASampleMoverCharacter>> RagdollDemoCharacters;
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

    /** Enable on the ragdoll pawn after removing its authored interaction key events. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    bool bUseNativeMoverInteractionInput = false;

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverInteractAction{FSoftObjectPath(TEXT("/Game/Input/IA_Interact.IA_Interact"))};

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverTakedownAction{FSoftObjectPath(TEXT("/Game/Input/IA_Takedown.IA_Takedown"))};

    /** Enable on the ragdoll pawn after the original trigger action has been removed. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    bool bUseNativeMoverRagdollInput = false;

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Mover|Input")
    TSoftObjectPtr<UInputAction> MoverRagdollAction{FSoftObjectPath(TEXT("/Game/Input/IA_TriggerRagdoll.IA_TriggerRagdoll"))};

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
    void HandleMoverShove();
    void HandleMoverTakedown();
    void HandleMoverRagdollInput();
    void HandleMoverInteractionInput(FName InteractionType, FName AimingDemoFunction);

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

    UFUNCTION()
    void CompleteRagdollRecoveryCheck();
};
