#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
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

    /**
     * Evaluates native geometry and writes its fields to the authored traversal result.
     * The component's cached character properties must be set before invoking this
     * adapter. Montage fields and other caller-owned result fields are retained.
     * @param Inputs Authored traversal probe dimensions and displacement.
     * @param DebugType Trace visualization for the selected debug level.
     * @param TraversalCheckResult Existing typed result consumed by the chooser.
     * @return True if the obstacle has a reachable front ledge.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Traversal",
        meta = (CustomStructureParam = "Inputs,TraversalCheckResult"))
    bool EvaluateTraversalClearance(UPARAM(ref) int32& Inputs, TEnumAsByte<EDrawDebugTrace::Type> DebugType,
        UPARAM(ref) int32& TraversalCheckResult);

    /** Reads the imported Blueprint structures from the VM and forwards them to the native query. */
    DECLARE_FUNCTION(execEvaluateTraversalClearance);
};
