#pragma once

#include "GameFramework/PlayerController.h"
#include "SampleSandboxPlayerController.generated.h"

class UInputAction;

/**
 * Controller for sandbox-only navigation and demonstration commands.
 * The active mapping context and owned pawn supply the authored input and view.
 */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleSandboxPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASampleSandboxPlayerController();

    /**
     * Teleports the possessed pawn along the controller's view ray.
     * A visibility sphere sweep stops at blocking geometry with clearance.
     * The pawn moves with TeleportPhysics and retains its rotation and velocity.
     * Calls without a pawn or world have no effect.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Sandbox")
    void TeleportToTarget();

protected:
    /** Binds the sandbox commands once to the controller's Enhanced Input component. */
    virtual void SetupInputComponent() override;

    /** Applies the reference mobile virtual joystick rule. */
    virtual void Tick(float DeltaSeconds) override;

private:
    /** Reliably asks the authority to choose the next demonstration pawn. */
    UFUNCTION(Server, Reliable)
    void NextPawn();

    /** Asks the authority to advance the visual override, with the reference unreliable delivery. */
    UFUNCTION(Server, Unreliable)
    void NextVisualOverride();

    /** Sweep distance in centimeters, copied from the original controller defaults. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Sandbox")
    double TargetTeleportDistance = 10000.0;

    /** Authored input for moving to the viewed location. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> TeleportAction;

    /** Authored input for switching demonstration pawns. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> NextPawnAction;

    /** Authored input for switching the visual preset. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> NextVisualOverrideAction;
};
