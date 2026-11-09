#pragma once

#include "Components/ActorComponent.h"
#include "SampleSmartObjectInteractionComponent.generated.h"

class UStateTree;
class UEnhancedInputComponent;

/** Applies capsule collision and correction policy for the sample's authored paired interactions. */
UCLASS(Abstract, Blueprintable, ClassGroup = (Gameplay))
class GAMEANIMATIONSAMPLECPP_API USampleSmartObjectInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * Applies a movement ignore entry to each of the owner's capsules.
     * @param bShouldIgnore True during the paired interaction, false during cleanup.
     * @param OtherActor Actor whose components should be ignored during capsule movement.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    void UpdateInteractionCollision(bool bShouldIgnore, AActor* OtherActor);

    /**
     * Applies the sample's temporary CMC correction override on peers that render the interaction.
     * @param IgnoreCorrections True during the authored interaction window.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    void UpdateInteractionMovementCorrection(bool IgnoreCorrections);

    /** Caches the owning pawn's skeletal mesh and the active movement component for the authored consumers. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    void CacheInteractionComponents();

    /** Installs the local player's authored interaction and move actions on the current input component. */
    void BindPlayerInteractionInput(UEnhancedInputComponent& Input);

    /**
     * Searches the player's forward interaction box and submits the nearest eligible free slot.
     * The StateTree continues to own the choice and playback of the entry animation.
     * @return True if playback may open its cancellation gate.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    bool TryStartPlayerInteraction();

    /** Clears the player's active interaction after the cancellation delegate is dispatched. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    void ClearPlayerInteractionState();

    /**
     * Measures the owner against an authored interaction destination.
     * @param Destination The target slot's world transform.
     * @param Distance World-space distance in centimeters.
     * @param Angle Destination-local bearing in degrees, including the destination's rotation and scale.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Interaction")
    void GetNpcApproachMetrics(const FTransform& Destination, double& Distance, double& Angle) const;

protected:
    /** Opens the authored cancellation gate after a native slot request has been accepted. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Sample|Interaction")
    void OpenPlayerInteractionGate();

    /** Lets the authored graph cancel its current loop and clear gameplay state in that order. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Sample|Interaction")
    void RequestPlayerInteractionCancellation();

private:
    void HandlePlayerInteractionInput();

    /** Enables native dispatch after the imported component's two input nodes have been replaced. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Interaction")
    bool bUseNativePlayerInteractionInput = false;

    /** StateTree receiving the existing player interaction event. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Interaction")
    TSoftObjectPtr<UStateTree> PlayerInteractionTree{
        FSoftObjectPath(TEXT("/Game/Blueprints/AI/StateTree/ST_Player_SandboxCharacter_SmartObject.ST_Player_SandboxCharacter_SmartObject"))};

    /** Authored payload type expected by the StateTree's bindings. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Interaction")
    TSoftObjectPtr<UScriptStruct> PlayerInteractionPayload{
        FSoftObjectPath(TEXT("/Game/Blueprints/AI/StateTree/S_SmartObjectPayload.S_SmartObjectPayload"))};
};
