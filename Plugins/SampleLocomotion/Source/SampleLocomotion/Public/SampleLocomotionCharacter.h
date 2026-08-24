#pragma once

/** @file SampleLocomotionCharacter.h */

#include "GameFramework/Character.h"
#include "SampleLocomotionCharacter.generated.h"

class UInputAction;
struct FInputActionValue;

/** Speed requested by the player. Crouching uses the movement component's own speed. */
UENUM(BlueprintType)
enum class ESampleGait : uint8
{
    Walk,   /**< Low standing speed. */
    Run,    /**< Default standing speed. */
    Sprint  /**< High standing speed requested until released. */
};

/**
 * Capsule-driven character for a local player or an AI issuing native movement requests.
 *
 * The character drives Unreal's Character Movement Component and owns no input mapping
 * contexts. The host PlayerController or Local Player Subsystem owns those contexts.
 * Gait and strafe requests currently have local semantics. A predicted multiplayer
 * implementation must carry them through saved moves before using them competitively.
 */
UCLASS()
class SAMPLELOCOMOTION_API ASampleLocomotionCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    /** Configures movement defaults while leaving mesh, animation class and camera to the host. */
    ASampleLocomotionCharacter();

    /** Applies authored default speeds and rotation behavior before gameplay begins. */
    virtual void PostInitializeComponents() override;

    /**
     * Queues analog movement relative to the supplied yaw.
     * @param Axis X moves right and Y moves forward. Magnitudes above one are clamped.
     * @param ReferenceYaw Yaw in degrees. Use zero for world space and control yaw for camera space.
     * @note Call on the game thread while the pawn is possessed. Invalid or zero axes are ignored.
     */
    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void MoveRelativeToYaw(FVector2D Axis, float ReferenceYaw);

    /**
     * Sets the requested standing gait and updates the CMC speed cap immediately.
     * While crouched, the CMC continues to use MaxWalkSpeedCrouched.
     * @param Gait Requested gait. Unknown enum values leave the current gait unchanged.
     */
    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void SetRequestedGait(ESampleGait Gait);

    /** Returns the requested standing gait, even while falling or crouching. */
    UFUNCTION(BlueprintPure, Category = "Locomotion")
    ESampleGait GetRequestedGait() const { return RequestedGait; }

    /**
     * Uses controller yaw while strafing and acceleration-driven yaw otherwise.
     * @param bEnabled Whether to follow controller yaw.
     */
    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void SetStrafing(bool bEnabled);

    /** Returns the current rotation intent. */
    UFUNCTION(BlueprintPure, Category = "Locomotion")
    bool IsStrafing() const { return bStrafing; }

protected:
    /**
     * Registers callbacks for any non-null action assigned on the character defaults.
     * @param PlayerInputComponent Pawn-owned input component. Enhanced Input is required.
     */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void InputMove(const FInputActionValue& Value);
    void InputWorldMove(const FInputActionValue& Value);
    void InputLook(const FInputActionValue& Value);
    void StartCrouch();
    void StopCrouch();
    void ToggleWalk();
    void StartSprint();
    void StopSprint();
    void StartStrafe();
    void StopStrafe();

    /** Centimeters per second at full input while standing. */
    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Speed", meta = (ClampMin = "0"))
    float WalkSpeed = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Speed", meta = (ClampMin = "0"))
    float RunSpeed = 500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Speed", meta = (ClampMin = "0"))
    float SprintSpeed = 700.0f;

    /** Optional Axis2D action in camera space. */
    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> CameraMoveAction;

    /** Optional Axis2D action with fixed world axes. Bind separately from CameraMoveAction. */
    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> WorldMoveAction;

    /** Optional Axis2D look action. Scale axes with modifiers in the mapping context. */
    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> WalkAction;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Input")
    TObjectPtr<UInputAction> StrafeAction;

    ESampleGait RequestedGait = ESampleGait::Run;
    ESampleGait StandingGait = ESampleGait::Run;
    bool bStrafing = false;
};
