#pragma once

#include "DefaultMovementSet/Modes/FallingMode.h"
#include "MovementModeTransition.h"
#include "SampleMoverModes.generated.h"

/** Adds the sample's facing intent to the stock Mover falling proposal. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverFallingMode : public UFallingMode
{
    GENERATED_BODY()

public:
    /** Runs stock air movement, then limits the turn toward the command's facing and rotation offset. */
    virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
        const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
};

/** Enters Sliding when a crouched pawn crosses the original horizontal speed threshold. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverEnterSlideTransition : public UBaseMovementModeTransition
{
    GENERATED_BODY()

public:
    /** @return Sliding above 380 cm/s with the crouch tag, or no transition. */
    virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};

/** Returns to Walking when a slide slows down or crouch ends. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleMoverExitSlideTransition : public UBaseMovementModeTransition
{
    GENERATED_BODY()

public:
    /** @return Walking at or below 200 cm/s or when standing, or no transition. */
    virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
};
