#pragma once

#include "Components/ActorComponent.h"
#include "SampleAnimationTickComponent.generated.h"

/**
 * Executes the animation sandbox's pre-movement and post-animation tick dispatch.
 *
 * The existing component assets retain their separate tick groups, instance overrides,
 * prerequisites and zero-parameter Tick delegates. Native code owns the broadcast.
 * The inherited delegates keep serialized bindings valid while subscribers migrate.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleAnimationTickComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Enables participation in Unreal's component tick scheduler. */
    USampleAnimationTickComponent();

    /** Adds a reflected zero-argument callback to the existing asset delegate at most once. */
    void BindTick(UObject& Receiver, FName Callback);

    /**
     * Broadcasts the component's retained Tick delegate once per scheduled tick.
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
