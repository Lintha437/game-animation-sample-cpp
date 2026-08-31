#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleBlueprintMigrationLibrary.generated.h"

class UEdGraph;

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
};
