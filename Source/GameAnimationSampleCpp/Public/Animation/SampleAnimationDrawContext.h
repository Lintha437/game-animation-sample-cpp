#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleAnimationDrawContext.generated.h"

class UAnimInstance;

/**
 * Shared world-context resolution for Animation Blueprint debug drawing.
 *
 * The caller retains ownership of execution gating. In particular, the existing
 * IsLocallyControlled branch remains in each diagnostic graph.
 */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleAnimationDrawContext final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Resolves PlayerController index zero using the animation instance as the
     * world context, matching the original Blueprint Self context.
     *
     * Returns FRotator::ZeroRotator when the animation instance, its owning
     * skeletal mesh component, or PlayerController zero is invalid. The owning
     * actor is deliberately not required to pass an additional validity check.
     *
     * This function does not perform local-control filtering. Callers must
     * preserve their existing IsLocallyControlled execution gate.
     *
     * @param AnimInstance Animation instance whose world supplies PlayerController zero.
     * @return PlayerController zero's control rotation, or the zero rotator on invalid context.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Animation|Diagnostics")
    static FRotator ResolveDrawRotation(const UAnimInstance* AnimInstance);
};
