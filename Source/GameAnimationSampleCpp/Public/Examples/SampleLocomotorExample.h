#pragma once

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "SampleLocomotorExample.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Routes the isolated Locomotor example's authored movement and look actions. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLocomotorController : public APlayerController
{
    GENERATED_BODY()

public:
    ASampleLocomotorController();

protected:
    /** Binds the three authored actions to their Triggered handlers on the local input component. */
    virtual void SetupInputComponent() override;

    /** Axis2D action supplying normalized controller-relative movement. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> MoveAction;

    /** Axis2D mouse delta action sampled with the example's fixed sensitivity. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> MouseLookAction;

    /** Axis2D stick rate action scaled by the current frame time. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputAction> GamepadLookAction;

private:
    void MoveFromAction(const FInputActionValue& Value);
    void LookFromMouse(const FInputActionValue& Value);
    void LookFromGamepad(const FInputActionValue& Value);
};

/** Installs the isolated example's input context after local possession. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLocomotorWalker : public ACharacter
{
    GENERATED_BODY()

public:
    ASampleLocomotorWalker();

    /** Installs mappings immediately for local authority, such as standalone and listen-server play. */
    virtual void PossessedBy(AController* NewController) override;

    /** Installs the same mappings when the owning player's pawn restarts. */
    virtual void PawnClientRestart() override;

protected:
    /** Context added to the owning local player once per input subsystem. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Input")
    TSoftObjectPtr<UInputMappingContext> InputMapping;

private:
    void InstallLocalMappings();
};
