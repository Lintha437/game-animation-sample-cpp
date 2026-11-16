#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleLookAtLevelRuntime.generated.h"

/**
 * Forwards the Look At POI level's authored Control Rig diagnostic commands.
 * The owning Level Blueprint retains its BeginPlay and Destroyed event entry points.
 */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleLookAtLevelRuntime : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Executes the two original console commands, in their authored order.
     *
     * Called once by the Level Blueprint's BeginPlay event with bEnabled=true,
     * and once by its Destroyed event with bEnabled=false. Immediately before
     * EACH command, the function evaluates GetPlayerController(0) in the
     * supplied world. The original graph's pure lookup is evaluated by both
     * impure calls, and this implementation preserves that evaluation timing.
     * A missing controller passes through unchanged to ExecuteConsoleCommand,
     * retaining that function's fallback command-routing behavior.
     * Neither command is gated on authority, local control, or controller validity.
     * No animation pose, chooser, playback, or Control Rig graph is modified.
     *
     * @param WorldContextObject The level script's implicit world context.
     * @param bEnabled Whether to enable or disable both diagnostic options.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At|Diagnostics",
        meta = (WorldContext = "WorldContextObject", CallableWithoutWorldContext = "true"))
    static void SetControlRigDebugState(const UObject* WorldContextObject, bool bEnabled);
};
