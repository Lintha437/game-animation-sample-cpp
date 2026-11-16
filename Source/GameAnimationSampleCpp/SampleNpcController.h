#pragma once

#include "AIController.h"
#include "TimerManager.h"
#include "SampleNpcController.generated.h"

/** Starts the imported NPC StateTree on the sample's original startup schedule. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleNpcController : public AAIController
{
    GENERATED_BODY()

public:
    /**
     * Implements the native behavior behind AIC_NPC_SmartObject's Debug function.
     *
     * Draws the controlled pawn's control-rotation direction as a 50 cm arrow
     * in the scene and Visual Logger. When no pawn is possessed, the Blueprint
     * pure-node value defaults are preserved by drawing from the origin toward
     * the positive X axis without a Blueprint VM access warning.
     * The debug drawer is created for this controller and consumed immediately.
     * The development-only metadata matches the authored debug draw nodes.
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Debug", meta = (DevelopmentOnly))
    void DebugNative();

protected:
    /** Schedules startup after the sample's local or dedicated-server delay. */
    virtual void BeginPlay() override;

    /** Cancels pending startup when the controller leaves the world. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Starts the existing StateTree AI component once the startup delay expires. */
    void StartNpcStateTree();

    /** Per-controller startup timer, owned until expiry or EndPlay. */
    FTimerHandle StateTreeStartupTimer;
};
