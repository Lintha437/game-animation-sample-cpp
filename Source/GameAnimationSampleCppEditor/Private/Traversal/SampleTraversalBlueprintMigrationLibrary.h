#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleTraversalBlueprintMigrationLibrary.generated.h"

class UBlueprint;

/** Editor-only migration of the three audited traversal graphs. */
UCLASS()
class USampleTraversalBlueprintMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Checks the audited connected-pin census and the literals required for supported rewiring. */
    UFUNCTION(BlueprintPure, Category = "Sample|Migration")
    static bool MatchesTraversalSource(UBlueprint* Blueprint);

    /**
     * Rewires the audited source graphs in memory. The caller must first back up the
     * clean package. False requires discarding unsaved edits; this API never saves.
     * Call ValidateTraversalGraphs after compiling and before saving.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool MigrateTraversalGraphs(UBlueprint* Blueprint);

    /** Checks native hook identity, counts and retained execution boundaries without modifying the asset. */
    UFUNCTION(BlueprintPure, Category = "Sample|Migration")
    static bool ValidateTraversalGraphs(UBlueprint* Blueprint);

    /** Returns a deterministic graph signature, including actual pin defaults, for installation receipts. */
    UFUNCTION(BlueprintPure, Category = "Sample|Migration")
    static FString TraversalGraphSignature(UBlueprint* Blueprint);

    /** Returns the compiled function flags, or zero when the function is absent. */
    UFUNCTION(BlueprintPure, Category = "Sample|Migration")
    static int64 TraversalFunctionFlags(UBlueprint* Blueprint, FName FunctionName);
};
