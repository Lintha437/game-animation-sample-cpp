#pragma once

#include "Animation/AnimInstance.h"
#include "SampleLocomotionCharacter.h"
#include "SampleLocomotionAnimInstance.generated.h"

/** Value-only inputs captured on the game thread for animation graph evaluation. */
USTRUCT(BlueprintType)
struct SAMPLELOCOMOTION_API FSampleLocomotionSnapshot
{
    GENERATED_BODY()

    /** Velocity in character space in centimeters per second, including vertical motion. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    FVector LocalVelocity = FVector::ZeroVector;

    /** Horizontal speed in centimeters per second. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float GroundSpeed = 0.0f;

    /** Direction relative to character forward. Positive angles turn toward the right. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    float DirectionDegrees = 0.0f;

    /** CMC acceleration is nonzero, independent of current velocity. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    bool bHasAcceleration = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    bool bIsFalling = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    bool bIsCrouched = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    bool bIsStrafing = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
    ESampleGait RequestedGait = ESampleGait::Run;

    /**
     * Captures movement state without retaining an actor reference.
     * Any Character supplies velocity, stance and falling state. The native subclass
     * also supplies gait and rotation intent. A missing character yields a neutral sample.
     * @pre Call on the game thread.
     */
    static FSampleLocomotionSnapshot Capture(const ACharacter* Character);
};

/**
 * Provides a native input snapshot to an authored Animation Blueprint.
 *
 * The graph owns pose selection, blending, IK and retargeting. Worker-thread graph logic
 * reads the snapshot via Property Access and must not traverse the source pawn.
 * Each game-thread update replaces the entire snapshot, including when the mesh is previewed
 * without a character or its owner changes.
 */
UCLASS(Transient, Blueprintable)
class SAMPLELOCOMOTION_API USampleLocomotionAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    /** Clears transient data when an animation instance is initialized or reused. */
    virtual void NativeInitializeAnimation() override;

    /** Captures the current skeletal mesh owner before graph update. */
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
    /** Capture from the latest game-thread animation update. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
    FSampleLocomotionSnapshot Locomotion;
};
