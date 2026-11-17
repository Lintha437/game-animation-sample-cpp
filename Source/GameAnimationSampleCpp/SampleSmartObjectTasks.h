#pragma once

#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "GameFramework/Actor.h"
#include "SmartObjectRuntime.h"
#include "Templates/Atomic.h"
#include "SampleSmartObjectTasks.generated.h"

class AAIController;
class UActorComponent;
class USmartObjectSubsystem;

/** Supplies navigation state to the retained Smart Object montage selection task. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMotionMatchApproachTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

public:
    /** Binds navigation and montage completion to the current StateTree actor on entry. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Smart Object")
    void InitializeApproachTask();

    /**
     * @return Current navigation path length, or straight-line distance when no path exists.
     * A path invalidated while still owned by path following keeps the reference result of -1.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Smart Object")
    double ReadApproachDistance() const;

    /** @return Mover velocity when the actor supplies Mover, otherwise its normal actor velocity. */
    UFUNCTION(BlueprintPure, Category = "Sample|Smart Object")
    FVector ReadApproachVelocity() const;

protected:
    /** Removes callbacks registered for this execution before the StateTree can reuse the task. */
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
    void UnbindApproachCallbacks();

    TWeakObjectPtr<AAIController> ApproachController;
    TWeakObjectPtr<UActorComponent> ApproachAnimation;
};

/**
 * Finds an available slot matching the sample's Gameplay Interaction and user tag requirements.
 * The imported task subclass owns the existing StateTree input and output bindings.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleFindSmartObjectTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Resolves a candidate in the current world and completes with failure if no usable slot is found. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Claims a slot for the bound actor and publishes the claim to downstream tasks. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleClaimSmartObjectTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Claims a free slot, preferring the selected slot and then other slots of the same object. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;

    /** Releases an untransferred claim when its owning state is left. */
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
    friend class USampleReleaseSmartObjectTask;
    friend class USampleUseSmartObjectTask;
    friend class FSampleSmartObjectClaimArbitrationTest;

    static FSmartObjectClaimHandle ArbitrateClaim(USmartObjectSubsystem& Subsystem,
        const FSmartObjectSlotHandle& Slot, const AActor& Actor, AActor* Target);
    bool TrackPendingClaim(USmartObjectSubsystem& Subsystem, const FSmartObjectClaimHandle& Claim);

    /** Finds the native owner of this exact, still pending reservation in the supplied world. */
    static USampleClaimSmartObjectTask* FindPendingClaim(const USmartObjectSubsystem* Subsystem,
        const FSmartObjectClaimHandle& Claim);

    /** Stops tracking the reservation before releasing it or transferring it to the engine task. */
    void ForgetPendingClaim();
    bool ReleasePendingClaim(bool bResetPlayerActivity);
    void ResetFailedPlayerActivity() const;
    void OnPendingClaimRevoked(const FSmartObjectClaimHandle& Claim);

    TWeakObjectPtr<USmartObjectSubsystem> PendingSubsystem;
    FSmartObjectClaimHandle PendingClaim;
    FSmartObjectClaimHandle TransferredClaim;
    FDelegateHandle PendingSlotListener;
    TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> PendingClaimRevoked;
    TWeakObjectPtr<AActor> PendingActor;
    uint64 PendingPlayerRequestId = 0;
};

/** Releases a bound Smart Object claim when the StateTree reaches its cleanup state. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleReleaseSmartObjectTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Releases a pending native claim; cleanup of a completed or transferred claim is idempotent. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Starts the engine's Gameplay Interaction task on an already claimed slot. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleUseSmartObjectTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Transfers the reservation to the engine task before activation, or releases it on dispatch failure. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Samples a reachable patrol point outside the authored Smart Object exclusion distance. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleFindPatrolLocationTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Publishes the patrol target, falling back to the pawn's position when no sample is usable. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;

private:
    /** Actor class whose instances reserve space for Smart Object approaches. */
    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    TSoftClassPtr<AActor> PatrolExclusionClass{FSoftObjectPath(TEXT("/Game/Blueprints/SmartObjects/BP_SmartObject_Base.BP_SmartObject_Base_C"))};
};

/** Gives the AI controller gameplay focus on its bound target. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleFocusInteractionTargetTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Focuses a valid bound target and leaves state completion to the other tasks. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Clears the gameplay focus when the StateTree leaves the approach sequence. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleClearInteractionFocusTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Clears the bound controller's gameplay focus when the cleanup state begins. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Resolves a claimed slot's world transform for movement and alignment consumers. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleFindSlotTransformTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Publishes the slot transform, or identity if the claim no longer resolves to a slot. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Updates the same replicated collision policy used by the authored paired-interaction component. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleSetInteractionCollisionTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Sends the requested ignore state through the character's existing replicated event. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Records a named Smart Object cooldown in the controller's existing world-time map. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleAddInteractionCooldownTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Replaces the named cooldown using the active world's elapsed game time. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Permits the next interaction after its cooldown and removes expired map entries. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleCheckInteractionCooldownCondition : public UStateTreeConditionBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Blocks while the named entry has time remaining and removes the entry when it expires. */
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** Publishes the AI controller associated with the actor bound to the evaluator. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleNpcControllerEvaluator : public UStateTreeEvaluatorBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Resolves the controller once at StateTree startup and retains the existing output binding. */
    virtual void TreeStart(FStateTreeExecutionContext& Context) override;
};

/** Replaces the bound NPC's input snapshot with the StateTree's walk or run request. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleSetNpcInputTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Applies input without completing the containing navigation state. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};
