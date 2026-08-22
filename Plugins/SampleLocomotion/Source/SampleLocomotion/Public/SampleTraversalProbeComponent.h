#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "SampleTraversalProbeComponent.generated.h"

class UCapsuleComponent;

/** World-space geometry for a chooser or a caller-owned traversal action. */
USTRUCT(BlueprintType)
struct SAMPLELOCOMOTION_API FSampleTraversalOpportunity
{
    GENERATED_BODY()

    /** Contact point on the facing surface. */
    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    FVector FrontContact = FVector::ZeroVector;

    /** Surface normal pointing toward the approaching pawn. */
    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    FVector FrontNormal = FVector::ZeroVector;

    /** Top surface sampled forward of the facing surface. */
    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    FVector TopContact = FVector::ZeroVector;

    /** Standing capsule center over the sampled top surface. */
    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    FVector CandidateCapsuleCenter = FVector::ZeroVector;

    /** Vertical rise from the current scaled capsule base, in centimeters. */
    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    float Height = 0.0f;
};

/**
 * Probes static or moving geometry with the current capsule dimensions.
 *
 * This component performs synchronous collision queries on the game thread. It
 * retains no hit objects and never changes collision or movement state. The caller
 * owns montage selection, motion-warp targets, movement mode and cancellation.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SAMPLELOCOMOTION_API USampleTraversalProbeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * Tests for a facing obstruction, walkable top, and free space for the current capsule.
     * @param Capsule Capsule belonging to this component's owner.
     * @param WorldDirection Desired traversal heading. The vertical component is ignored.
     * @param Opportunity Cleared on failure. Successful results are valid for this query only.
     * @return Whether all three queries pass. Re-query before acting on moving geometry.
     */
    UFUNCTION(BlueprintCallable, Category = "Traversal")
    bool FindOpportunity(const UCapsuleComponent* Capsule, FVector WorldDirection,
        FSampleTraversalOpportunity& Opportunity) const;

private:
    /** Channel shared by the facing, top and destination queries. */
    UPROPERTY(EditDefaultsOnly, Category = "Traversal")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** Additional horizontal reach beyond the capsule radius, in centimeters. */
    UPROPERTY(EditDefaultsOnly, Category = "Traversal", meta = (ClampMin = "1"))
    float Reach = 85.0f;

    /** Minimum obstacle height measured from the capsule's base, in centimeters. */
    UPROPERTY(EditDefaultsOnly, Category = "Traversal", meta = (ClampMin = "1"))
    float MinimumHeight = 45.0f;

    /** Maximum obstacle height measured from the capsule's base, in centimeters. */
    UPROPERTY(EditDefaultsOnly, Category = "Traversal", meta = (ClampMin = "1"))
    float MaximumHeight = 170.0f;

    /** Minimum accepted dot product between the sampled top normal and world up. */
    UPROPERTY(EditDefaultsOnly, Category = "Traversal", meta = (ClampMin = "0", ClampMax = "1"))
    float MinimumTopNormalZ = 0.75f;
};
