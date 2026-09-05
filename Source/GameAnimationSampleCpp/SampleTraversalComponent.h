#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "SampleTraversalComponent.generated.h"

/**
 * Native movement hooks for the reference traversal component.
 * The authored component supplies the traversal result and invokes the hooks
 * at its existing montage and replication boundaries.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleTraversalComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * Changes the owning Character Movement Component to a standard mode.
     * Other owner types or missing movement components are ignored.
     * @param CMCMovementMode Standard movement mode. Custom mode is reset to zero.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void SetMovementMode_CMC(EMovementMode CMCMovementMode);

    /**
     * Resolves the traversal exit mode from the stored authored action type.
     * The reference selects falling for its airborne exit and walking for all other actions.
     * @return Exit movement mode. Missing or unrecognized data defaults to walking.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Traversal")
    EMovementMode GetEndMovementMode_CMC() const;

    /**
     * Sets both CMC correction and authoritative-position flags for the traversal window.
     * The traversal caller owns the timing and invokes this again to restore normal correction.
     * The Mover path leaves CMC settings untouched.
     * @param ClientAuthoritative Whether to accept client traversal movement.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void SetReplicationBehavior(bool ClientAuthoritative);
};
