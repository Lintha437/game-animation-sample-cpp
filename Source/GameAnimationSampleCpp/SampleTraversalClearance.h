#pragma once

#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SampleTraversalLedgeLibrary.h"

/** Capsule swept towards the obstacle in world space. */
struct FSampleTraversalApproach
{
    FVector Direction = FVector::ZeroVector; ///< Authored direction, including any vertical component.
    FVector StartOffset = FVector::ZeroVector; ///< Relative to the pawn's current location.
    FVector EndOffset = FVector::ZeroVector; ///< Added after the forward displacement.
    double Distance = 0.0; ///< Forward displacement in centimeters.
    float Radius = 0.0f; ///< Radius of the detection capsule in centimeters.
    float HalfHeight = 0.0f; ///< Half height of the detection capsule in centimeters.
};

/** Geometry supplied to the authored traversal chooser. */
struct FSampleTraversalClearance
{
    TWeakObjectPtr<UPrimitiveComponent> Obstacle; ///< Component hit by the approach sweep.
    FSampleTraversalLedges Ledges; ///< Front and opposite transforms supplied by the obstacle.
    bool bCanReachFront = false; ///< Whether the pawn capsule can sweep to the front edge.
    bool bCanCrossTop = false; ///< Whether the sweep from front to back was clear.
    double Height = 0.0; ///< Absolute front ledge height relative to the pawn's feet.
    double Depth = 0.0; ///< Horizontal distance to the back ledge or blocking top hit.
    TOptional<FVector> BackFloor; ///< Impact point from the sweep below the far edge.
    double BackHeight = 0.0; ///< Absolute vertical distance from the far edge to the floor.
};

/** Performs the sample's traversal sweeps prior to animation selection. */
class GAMEANIMATIONSAMPLECPP_API FSampleTraversalClearanceQuery
{
public:
    /**
     * Samples the first valid traversable blocker, approach clearance, top and far floor.
     * The pawn and its components are ignored in all four sweeps. The pawn's authored
     * capsule dimensions drive clearance, and the approach settings drive detection.
     * @param Pawn Traversing actor and world context.
     * @param Approach Authored approach sweep.
     * @param Radius Pawn capsule radius in centimeters.
     * @param HalfHeight Pawn capsule half height in centimeters.
     * @param DebugDraw Trace visualization selected by the caller.
     * @param DebugLevel 1 draws ledges, 2 adds detection and 3 adds clearance sweeps.
     * @param DebugDuration Duration in seconds for timed debug drawing.
     * @return Geometry and eligibility from completed sweeps. The caller owns montage selection.
     */
    static FSampleTraversalClearance Query(AActor& Pawn, const FSampleTraversalApproach& Approach,
        float Radius, float HalfHeight, EDrawDebugTrace::Type DebugDraw, int32 DebugLevel, float DebugDuration);
};
