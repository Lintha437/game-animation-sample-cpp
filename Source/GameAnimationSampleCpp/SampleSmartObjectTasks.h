#pragma once

#include "Blueprint/StateTreeTaskBlueprintBase.h"
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
