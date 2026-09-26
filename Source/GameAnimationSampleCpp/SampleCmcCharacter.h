#pragma once

#include "GameFramework/Character.h"
#include "SampleCmcCharacter.generated.h"

class UInputAction;
struct FInputActionValue;

/**
 * Input intent carried to the authority.
 * The imported pawn retains the replicated state read by animation and camera graphs.
 */
USTRUCT()
struct FSampleCmcInputIntent
{
    GENERATED_BODY()

    /** Sprint input is held or latched by its authored action. */
    UPROPERTY()
    bool bWantsToSprint = false;

    /** The player selected walk gait. Sprint clears this selection. */
    UPROPERTY()
    bool bWantsToWalk = false;

    /** The player selected camera-relative facing. */
    UPROPERTY()
    bool bWantsToStrafe = false;

    /** The aim action's current value. */
    UPROPERTY()
    bool bWantsToAim = false;

    /** Preserved intent from the authored character interface. */
    UPROPERTY()
    bool bWantsToCrouch = false;
};

/** Native parent for the reference CMC character and its authored movement settings. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleCmcCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASampleCmcCharacter();

    /**
     * Selects gait and applies movement settings immediately before CMC simulation.
     * Polls Enhanced Input's raw movement actions and updates the serialized gait
     * consumed by the reference Animation Blueprint. An unpossessed pawn has zero
     * action magnitude and continues to use its existing movement and input state.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Movement")
    void UpdateMovementBeforeSimulation();

    /**
     * Applies the authored stick mode to movement input before it reaches CMC.
     * Fixed-speed modes normalize nonzero input. Variable modes preserve its magnitude.
     * @param Input Raw two-axis movement input from the current action.
     * @return Input in the same coordinate frame, with magnitude appropriate to the mode.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    FVector2D GetMovementInputScaleValue(FVector2D Input) const;

    /**
     * Checks sprint intent and the direction allowed by the current rotation mode.
     * Locally controlled pawns use queued input. Remote pawns use CMC acceleration.
     * A strafing direction exactly 50 degrees from actor yaw is outside the sprint cone.
     * @return Whether the current input state permits sprinting.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    bool CanSprint() const;

    /**
     * Applies aim and strafe intent to CMC before it simulates movement.
     * Ground movement turns immediately. Falling limits yaw to 200 degrees per second.
     * The reference tick dispatcher owns the call timing and the input writer owns intent.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Movement")
    void UpdateRotation_PreCMC();

    /** @return Acceleration in cm/s², reduced as planar sprint velocity increases. */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    double CalculateMaxAcceleration() const;

    /** @return Ground friction, reduced with planar sprint velocity. */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    double CalculateGroundFriction() const;

    /**
     * Evaluates the authored standing speeds for the active gait.
     * Controller-oriented movement uses the strafe curve and current planar direction.
     * A missing speed field falls back to the component's standing speed.
     * @return Maximum speed in centimeters per second.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    double CalculateMaxSpeed() const;

    /**
     * Computes the crouched speed for the current movement direction.
     * The subclass owns the forward, strafe and backward speeds and the angle
     * remapping curve. A missing curve uses forward speed, as does orient-to-movement.
     * @return The maximum crouch speed in centimeters per second.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    double CalculateMaxCrouchSpeed() const;

    /** @return Whether the character's default montage slot permits a jump or traversal request. */
    UFUNCTION(BlueprintPure, Category = "Sample|Input")
    bool CanStartCmcJump() const;

    /** Disables character movement and hands the skeletal bodies below the pelvis to physics. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Movement|Ragdoll")
    void BeginCmcRagdoll();

    /** Restores the authored capsule and skeletal collision policy and resumes CMC in Falling. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Movement|Ragdoll")
    void EndCmcRagdoll();

    /** Replaces the navigation task's input snapshot with the requested walk or run intent. */
    void ApplyNpcWalkIntent(bool bWalk);

protected:
    /** Binds raw action values and the installed native handlers on local possession. */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** Ends the grounded crouch when CMC leaves a ledge. */
    virtual void OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal,
        const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta) override;

private:
    /**
     * Enables native event bindings after the corresponding Blueprint events are removed.
     * The migration sets this on the reference pawn's class defaults.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    bool bUseNativeInputHandlers = false;

    /** Enables native intent bindings after the matching Blueprint events are removed. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    bool bUseNativeInputIntentHandlers = false;

    /** Enable after the imported jump and traversal action has been removed. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    bool bUseNativeCmcJumpInput = false;

    /** @return Walking deceleration in cm/s², with lighter braking while input remains queued. */
    double CalculateBrakingDeceleration() const;

    /** Authored camera-relative move action, sampled before CMC consumes queued input. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> CameraRelativeMoveAction;

    /** Authored world-relative move action, sampled independently of camera-relative move. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> WorldRelativeMoveAction;

    /** Mouse-style look deltas, in degrees per input update. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> LookAction;

    /** Gamepad turn rates, in degrees per second before frame scaling. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> GamepadLookAction;

    /** Authored toggle action for grounded crouching. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> CrouchAction;

    /** Authored walk toggle action. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> WalkAction;

    /** Authored sprint state action. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> SprintAction;

    /** Authored strafe toggle action. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> StrafeAction;

    /** Authored aim state action. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> AimAction;

    /** Authored jump action used for the initial press and held traversal retry. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> CmcJumpAction{FSoftObjectPath(TEXT("/Game/Input/IA_Jump.IA_Jump"))};

    /** Attempts traversal first and requests the CMC jump if the approach is rejected. */
    void HandleCmcJumpStarted();

    /** Retries traversal while jump input remains triggered. */
    void HandleCmcJumpTriggered();

    /** @return Whether the authored traversal component accepted the native CMC approach. */
    bool TryCmcTraversal(bool bDrawForDuration);

    /** Mirrors the current ragdoll state to the original Blueprint property. */
    void SetCmcRagdollFlag(bool bActive);

    /** Queues camera-relative movement using the authored stick mode. */
    void HandleCameraRelativeMove(const FInputActionValue& Value);

    /** Queues normalized movement on the world's horizontal axes. */
    void HandleWorldRelativeMove(const FInputActionValue& Value);

    /** Applies a look delta directly to the local controller. */
    void HandleLook(const FInputActionValue& Value);

    /** Converts gamepad turn rate to an angular delta for the frame. */
    void HandleGamepadLook(const FInputActionValue& Value);

    /** Toggles crouching while grounded or in another non-falling movement mode. */
    void HandleCrouch();

    /** Toggles walking if sprint is inactive. */
    void HandleWalkIntent();

    /** Applies sprint intent and clears the walk toggle. */
    void HandleSprintIntent(const FInputActionValue& Value);

    /** Toggles controller-facing movement. */
    void HandleStrafeIntent();

    /** Applies aim intent without affecting the walk or sprint state. */
    void HandleAimIntent(const FInputActionValue& Value);

    /** Reads the shared asset-owned state. No separate copy persists on the pawn. */
    FSampleCmcInputIntent ReadInputIntentSnapshot() const;

    /** Applies all intent fields to the compatible asset state and marks replication dirty. */
    bool WriteInputIntentSnapshot(const FSampleCmcInputIntent& Intent);

    /** Applies intent locally and sends a complete snapshot from the owning client. */
    void CommitInputIntent(const FSampleCmcInputIntent& Intent);

    /** Updates the authoritative state from the owning player's pawn channel. */
    UFUNCTION(Server, Unreliable)
    void ServerCommitInputIntent(FSampleCmcInputIntent Intent);

    /** Sends an angular delta to the owning local player controller. */
    void AddLookInput(const FVector2D& Delta);

    /** @return The reference gait index, determined from intent and raw input amplitude. */
    int64 SelectGait() const;

    /** Reads serialized tuning owned by the subclass. The class also owns the curve. */
    double CalculateDirectionalSpeed(FName SpeedsName, double Fallback, bool bUseStrafeCurve) const;
};
