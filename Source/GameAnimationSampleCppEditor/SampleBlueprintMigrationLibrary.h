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
};
