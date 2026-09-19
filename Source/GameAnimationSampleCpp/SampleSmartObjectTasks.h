#pragma once

#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "GameFramework/Actor.h"
#include "SampleSmartObjectTasks.generated.h"

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
    /** Attempts one normal-priority claim and leaves state completion to the interaction task. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Releases a bound Smart Object claim when the StateTree reaches its cleanup state. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleReleaseSmartObjectTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Completes successfully only when the subsystem releases the supplied claim. */
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Starts the engine's Gameplay Interaction task on an already claimed slot. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleUseSmartObjectTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    /** Completes after successful dispatch. The controller's gameplay task owns the ongoing interaction. */
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
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Clears the gameplay focus when the StateTree leaves the approach sequence. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleClearInteractionFocusTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Resolves a claimed slot's world transform for movement and alignment consumers. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleFindSlotTransformTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Updates the same replicated collision policy used by the authored paired-interaction component. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleSetInteractionCollisionTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Records a named Smart Object cooldown in the controller's existing world-time map. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleAddInteractionCooldownTask : public UStateTreeTaskBlueprintBase
{
    GENERATED_BODY()

protected:
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) override;
};

/** Permits the next interaction after its cooldown and removes expired map entries. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleCheckInteractionCooldownCondition : public UStateTreeConditionBlueprintBase
{
    GENERATED_BODY()

protected:
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
