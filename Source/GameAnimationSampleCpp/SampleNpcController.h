#pragma once

#include "AIController.h"
#include "TimerManager.h"
#include "SampleNpcController.generated.h"

/** Starts the imported NPC StateTree on the sample's original startup schedule. */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleNpcController : public AAIController
{
    GENERATED_BODY()

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
