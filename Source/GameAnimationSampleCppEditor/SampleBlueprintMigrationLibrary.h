#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleBlueprintMigrationLibrary.generated.h"

class UEdGraph;
class UBlueprint;

/** Editor-only graph operations for local Blueprint migrations. */
UCLASS()
class USampleBlueprintMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Rebinds a single self call while preserving its wires and layout.
     * Both functions must have matching pins. The migration owns compiling
     * and saving the Blueprint after updating its parent and declarations.
     * @param Graph Graph that contains the call.
     * @param OldFunction Existing self function name.
     * @param NewFunction Replacement self function name.
     * @return True if exactly one existing call was rebound.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RetargetSelfCall(UEdGraph* Graph, FName OldFunction, FName NewFunction);

    /** @return Object paths of the Enhanced Input actions declared in the graph. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static TArray<FString> ListEnhancedInputActions(const UEdGraph* Graph);

    /**
     * Removes isolated Enhanced Input event chains from a Blueprint graph.
     * Validates all requested actions before editing. Rejects shared chains and
     * leaves the caller responsible for compilation and package save.
     * @param Blueprint Blueprint that owns the graph.
     * @param Graph Graph that contains the Enhanced Input event nodes.
     * @param ActionPaths Full object paths of the actions to remove, exactly once each.
     * @return True when every action had an independent event chain and was removed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveEnhancedInputActions(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FString>& ActionPaths);

    /** Removes the shared mouse-wheel and D-pad camera style input graph after validating its call paths. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveSandboxCameraKeys(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Removes the two authored flight keys and their shared Flying/Falling branch. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveMoverFlightKeys(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Replaces the two crouch tails in the finalized Mover mode callback, preserving presentation and ordering. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardMoverTransitionCrouch(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Forwards ragdoll demo spawning and transfers the authored target class to the native call. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardRagdollDemoSpawn(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Moves the CMC landing velocity and retriggerable flag into native code around the existing foley call. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardCmcLandingState(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Replaces the Mover direction calculations while preserving the rotation-offset Chooser and curve sample. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardMoverDirectionSector(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Removes the isolated click chains for the native Look At possession and patrol buttons. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveLookAtGameplayButtons(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Replaces traversal ledge setup after the authored parent construction call. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardTraversalLedgeSetup(UBlueprint* Blueprint, UEdGraph* Graph);

    /** Forwards Look At zone selection state and preserves the authored animation and widget dispatch. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardLookAtZoneSelection(UBlueprint* Blueprint, UEdGraph* Graph);

    /**
     * Reads compiled function flags before an authored function or event is replaced.
     * @param Blueprint Blueprint whose generated class owns the function.
     * @param FunctionName Name of the compiled function.
     * @return Unreal function flags, or zero if the function does not exist.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int64 GetCompiledFunctionFlags(const UBlueprint* Blueprint, FName FunctionName);

    /**
     * Removes one custom event and its isolated chain from the given graph.
     * @return True if the event was uniquely identified and no other entry shares its nodes.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveCustomEvent(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName);

    /** Removes named custom events and shared nodes only when no unlisted entry reaches them. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveCustomEvents(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FName>& EventNames);

    /** Replaces the paired collision tail and montage cleanup while preserving the authored chooser and proxy. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplacePairedInteractionCollision(UBlueprint* Blueprint, UEdGraph* MatchGraph, UEdGraph* EventGraph);

    /**
     * Removes an isolated override and its isolated graph body after native code takes over.
     * @param Blueprint Owner of the event graph.
     * @param Graph Event graph to edit.
     * @param EventName Reflected name of the overridden event.
     * @return True when the event is absent or was removed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveIsolatedEventOverride(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName);

    /**
     * Keeps a Blueprint event or interface entry and replaces its isolated body with a native call.
     * @param Blueprint Owner of the event graph.
     * @param Graph Graph containing the event.
     * @param EventName Existing reflected event name.
     * @param FunctionName Native function with matching inputs.
     * @return True when the event forwards to the native function.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ForwardIsolatedEventOverride(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName, FName FunctionName);

    /** Removes a declared Blueprint member before its value is moved to a native property. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool RemoveDeclaredVariable(UBlueprint* Blueprint, FName VariableName);

    /**
     * Replaces a void Blueprint function body with a static native call.
     * The call receives matching inputs by name and the graph retains its
     * original typed signature. Callers own compile and save.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceVoidFunctionBody(UBlueprint* Blueprint, UEdGraph* Graph, UClass* Library, FName FunctionName);

    /**
     * Replaces the two geometry stages of the reference traversal function
     * while preserving initialization, both Choosers, and event dispatch.
     * @param Blueprint Traversal component that owns the graph.
     * @param Graph Existing TryTraversalAction graph.
     * @return True when the native call is wired or already installed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceTraversalGeometry(UBlueprint* Blueprint, UEdGraph* Graph);

    /**
     * Rewires the collision and activity nodes at the four traversal callback sites.
     * Preserves the authored montage, warp target, and movement mode nodes.
     * @param Blueprint Traversal component owning the event graph.
     * @param Graph Existing traversal event graph.
     * @return True when the expected sites were converted or already installed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceTraversalCollision(UBlueprint* Blueprint, UEdGraph* Graph);

    /**
     * Replaces the CMC and Mover mode sequences at their four callback sites.
     * Keeps the authored playback, collision and warp target ordering.
     * @param Blueprint Traversal component with the native parent.
     * @param Graph Component event graph after the collision migration.
     * @return True when all four sites were converted or already installed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceTraversalModes(UBlueprint* Blueprint, UEdGraph* Graph);

    /**
     * Keeps the typed multicast event and replaces its recipient body with native replay.
     * @param Blueprint Traversal component with the native parent.
     * @param Graph Component event graph containing the multicast.
     * @return True when the replay body was converted or already installed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceTraversalReplay(UBlueprint* Blueprint, UEdGraph* Graph);

    /**
     * Connects a zero-argument Blueprint enum query to a native byte-valued member.
     * The existing graph and typed return pin preserve the imported enum contract.
     * @param Blueprint Blueprint that owns the function.
     * @param Graph Pure enum query to convert.
     * @param FunctionName Native inherited function supplying the underlying byte.
     * @return True when the enum adapter is connected.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceEnumQuery(UBlueprint* Blueprint, UEdGraph* Graph, FName FunctionName);

    /**
     * Replaces a Blueprint function body with a native member call.
     * @param Blueprint Mover pawn with the native parent.
     * @param Graph Existing typed function with zero or one output.
     * @param FunctionName Native inherited function with matching input names.
     * @param OutputName Existing output pin, or None for void functions.
     * @return True when the native call is connected or already installed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceMoverInputFunction(UBlueprint* Blueprint, UEdGraph* Graph, FName FunctionName, FName OutputName);

    /**
     * Connects ragdoll entry, capsule collision and recovery to native code.
     * Preserves the events and graph segments that supply the injury state and recovery playback.
     * @param Blueprint Imported ragdoll pawn with the native Mover class in its inheritance chain.
     * @param EventGraph Graph containing the entry and recovery events.
     * @param ModeGraph Override that observes finalized Mover mode changes.
     * @return True when all lifecycle hooks are installed.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceRagdollLifecycle(UBlueprint* Blueprint, UEdGraph* EventGraph, UEdGraph* ModeGraph);

    /**
     * Routes the authored interact input through the native player request.
     * Keeps the shared animation cancellation gate and opens it only on a successful request.
     * @param Blueprint Imported Smart Object interaction component.
     * @param EventGraph Its original event graph.
     * @return True when the input is routed to C++.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplacePlayerSmartObjectInput(UBlueprint* Blueprint, UEdGraph* EventGraph);
};
