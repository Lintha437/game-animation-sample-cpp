#pragma once

#include "Components/ActorComponent.h"
#include "SampleAnimationTickComponent.generated.h"

/** Broadcast when a sample animation tick component executes its scheduled tick. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSampleAnimationTickSignature);

/**
 * Executes the animation sandbox's pre-movement and post-animation tick dispatch.
 *
 * Instances retain their scheduled tick groups, prerequisites, and zero-parameter
 * callback dispatch natively, while continuing to honor any legacy Blueprint
 * `Tick` event dispatcher when loaded on unretired component subclasses.
 */
UCLASS(Blueprintable, ClassGroup = (Sample), meta = (BlueprintSpawnableComponent))
class GAMEANIMATIONSAMPLECPP_API USampleAnimationTickComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Enables participation in Unreal's component tick scheduler with the sandbox physics tick group. */
    USampleAnimationTickComponent();

    /** Native zero-parameter multicast delegate invoked on each component tick. */
    UPROPERTY(BlueprintAssignable, Category = "Sample|Tick")
    FSampleAnimationTickSignature OnTick;

    /** Adds a reflected zero-argument callback to the active tick delegate at most once. */
    void BindTick(UObject& Receiver, FName Callback);

    /**
     * Broadcasts the component's tick delegate once per scheduled tick.
     * @param DeltaTime Seconds since this component last ticked.
     * @param TickType World's current tick mode.
     * @param ThisTickFunction Scheduler-owned tick descriptor.
     * @pre The component is registered on the game thread.
     */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    FMulticastScriptDelegate* FindAuthoredTick();
};
