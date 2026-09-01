#pragma once

#include "GameFramework/Character.h"
#include "SampleCmcCharacter.generated.h"

class UInputAction;
struct FInputActionValue;

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

protected:
    /** Binds raw action values and the installed native handlers on local possession. */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    /**
     * Enables native event bindings after the corresponding Blueprint events are removed.
     * The migration sets this on the reference pawn's class defaults.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    bool bUseNativeInputHandlers = false;

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

    /** Sends an angular delta to the owning local player controller. */
    void AddLookInput(const FVector2D& Delta);

    /** @return The reference gait index, determined from intent and raw input amplitude. */
    int64 SelectGait() const;

    /** Reads serialized tuning owned by the subclass. The class also owns the curve. */
    double CalculateDirectionalSpeed(FName SpeedsName, double Fallback, bool bUseStrafeCurve) const;
};
