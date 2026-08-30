#pragma once

#include "GameFramework/Character.h"
#include "SampleCmcCharacter.generated.h"

/** Native parent for the reference CMC character and its authored movement settings. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleCmcCharacter : public ACharacter
{
    GENERATED_BODY()

public:
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

private:
    /** Reads serialized tuning owned by the subclass. The class also owns the curve. */
    double CalculateDirectionalSpeed(FName SpeedsName, double Fallback, bool bUseStrafeCurve) const;
};
