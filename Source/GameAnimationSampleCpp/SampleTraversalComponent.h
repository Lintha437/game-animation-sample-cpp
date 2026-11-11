#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "SampleTraversalComponent.generated.h"

class UPrimitiveComponent;
class UMoverComponent;
class APawn;
struct FSampleTraversalApproach;

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
     * Writes the native approach into the existing imported traversal input struct.
     * Unsupported pawn classes or modes return the authored struct defaults.
     * @param Pawn Character whose movement state supplies the sweep.
     * @param Inputs Typed Blueprint output read by existing traversal callers.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Traversal", meta = (CustomStructureParam = "Inputs"))
    static void BuildImportedTraversalInputs(const APawn* Pawn, int32& Inputs);

    /** Resolves the wildcard output before dispatching to the pawn's native approach builder. */
    DECLARE_FUNCTION(execBuildImportedTraversalInputs);

    /**
     * Caches the owner's authored traversal interface and resolves the Mover playback component.
     * Called at the reference component's BeginPlay. The owner and its components remain externally owned.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void InitializeTraversalRuntime();

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

    /**
     * Updates the traversal activity field exposed by the imported component.
     * The original montage callbacks own when the activity starts and ends.
     * @param bActive Whether a traversal action is in progress.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void SetTraversalActionActive(bool bActive);

    /** @return Whether an authored traversal action currently owns movement and collision. */
    bool IsTraversalActionActive() const;

    /**
     * Sends a native approach to the imported chooser and playback graph.
     * @param Approach World-space sweep direction, offsets, and capsule dimensions.
     * @param DebugType Visualization mode for this input attempt.
     * @return True once geometry and montage selection both succeed.
     */
    bool TryAuthoredTraversal(const FSampleTraversalApproach& Approach, EDrawDebugTrace::Type DebugType);

    /**
     * Suspends collision with the struck component for this traversal.
     * Replacing an active target releases its changes first. Repeating the same
     * request leaves the originally captured snapshot intact.
     * @param Capsule Component swept by the owner's movement system.
     * @param Obstacle Specific component to ignore, if present.
     * @param Mesh Optional Mover visual mesh whose WorldDynamic response is suspended.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void BeginTraversalCollision(UPrimitiveComponent* Capsule, UPrimitiveComponent* Obstacle,
        UPrimitiveComponent* Mesh = nullptr);

    /**
     * Releases the ignore entry and mesh response acquired at the action start.
     * Safe to call more than once, including after the target has been destroyed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void EndTraversalCollision();

    /** Enters Flying and suspends CMC correction during the current montage. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void BeginCmcTraversalMode();

    /**
     * Applies the authored exit mode locally and restores normal correction after a 0.2-second grace period.
     * The authority for an autonomous proxy waits for the owner's first non-Flying move, with a bounded
     * fallback to the authored mode. Starting another traversal cancels any pending restoration.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void FinishCmcTraversalMode();

    /**
     * Queues the authored Traversing mode and records the current simulation step.
     * @param Mover Movement component used by the retained authored montage action.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void BeginMoverTraversalMode(UMoverComponent* Mover);

    /**
     * Returns an active traversal to Walking. If interrupted before the queued
     * start takes effect, cancels it and preserves the current mode.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Traversal")
    void FinishMoverTraversalMode();

    /**
     * Receives the reference multiplayer traversal result on authority and proxies.
     * The locally controlled pawn has already started the chosen action. Other
     * copies record the payload before invoking the authored playback event.
     * @param TraversalResult Original typed result serialized by the sample RPC.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Traversal",
        meta = (CustomStructureParam = "TraversalResult"))
    void ApplyTraversalReplay(UPARAM(ref) int32& TraversalResult);

    /** Reads the reflected network payload directly from the Blueprint VM. */
    DECLARE_FUNCTION(execApplyTraversalReplay);

    /**
     * Stores an accepted traversal result, starts authored local playback and submits the original server RPC.
     * @param TraversalResult Typed result of the authored traversal Chooser.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Traversal",
        meta = (CustomStructureParam = "TraversalResult"))
    void PublishTraversalAction(UPARAM(ref) int32& TraversalResult);

    /** Reads the authored traversal result struct directly from the Blueprint VM. */
    DECLARE_FUNCTION(execPublishTraversalAction);

    /** @return Whether this copy needs traversal replay from the multicast. */
    bool ShouldReceiveTraversalReplay() const;

protected:
    /** Restores outstanding physical changes if the owner leaves play during traversal. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CompleteCmcCorrection(bool bWaitForClientExit, double Deadline);
    void RestoreCmcCorrection();

    TWeakObjectPtr<UPrimitiveComponent> TraversalCapsule;
    TWeakObjectPtr<UPrimitiveComponent> TraversalObstacle;
    TWeakObjectPtr<UPrimitiveComponent> TraversalMesh;
    ECollisionResponse SavedMeshResponse = ECR_Block;
    bool bAddedMoveIgnore = false;
    FTimerHandle CmcCorrectionTimer;
    bool bCmcCorrectionSuspended = false;
    TWeakObjectPtr<UMoverComponent> TraversalMover;
    int32 MoverStartFrame = INDEX_NONE;
    double MoverStartTimeMs = -1.0;
};
