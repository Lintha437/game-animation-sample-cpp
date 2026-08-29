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
     * Computes the crouched speed for the current movement direction.
     * The subclass owns the forward, strafe and backward speeds and the angle
     * remapping curve. A missing curve uses forward speed, as does orient-to-movement.
     * @return The maximum crouch speed in centimeters per second.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Movement")
    double CalculateMaxCrouchSpeed() const;
};
