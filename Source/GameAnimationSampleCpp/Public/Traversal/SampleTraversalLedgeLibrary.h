#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleTraversalLedgeLibrary.generated.h"

/** World-space poses selected from the obstacle's authored splines. */
struct FSampleTraversalLedges
{
    /** World-space transform on the selected front edge after corner clearance. */
    TOptional<FTransform> Front;

    /** World-space transform on the mapped opposite edge, when both edges are valid. */
    TOptional<FTransform> Back;
};

/** Resolves front and back ledges for the sample's spline-authored traversal blocks. */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleTraversalLedgeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Chooses a facing edge from the actor position, then samples the hit position
     * with clearance from either corner. The block owns the authored splines.
     * @param Block Traversable level block.
     * @param HitLocation World-space trace impact.
     * @param ActorLocation World-space position of the traversing pawn.
     * @return Front and opposite ledge poses, when available.
     */
    static FSampleTraversalLedges QueryLedges(const AActor& Block, FVector HitLocation, FVector ActorLocation);

    /**
     * Adapts the native query to the reference's user-defined traversal result.
     * Only ledge fields are written. This entry point is called by the retained
     * Blueprint signature, so other gameplay and animation fields are preserved.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Traversal",
        meta = (CustomStructureParam = "TraversalTraceResultInOut", DefaultToSelf = "Block"))
    static void UpdateBlockLedges(AActor* Block, FVector HitLocation, FVector ActorLocation,
        UPARAM(ref) int32& TraversalTraceResultInOut);

    /** Unpacks the wildcard result structure from the Blueprint VM. */
    DECLARE_FUNCTION(execUpdateBlockLedges);
};
