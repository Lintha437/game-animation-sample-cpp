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
};
