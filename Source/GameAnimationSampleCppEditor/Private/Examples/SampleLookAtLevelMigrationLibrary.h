#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleLookAtLevelMigrationLibrary.generated.h"

class UBlueprint;

/** Editor-only, narrowly scoped migration of the Look At POI level script. */
UCLASS()
class USampleLookAtLevelMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Classifies the exact authored or migrated Look At level-script graph.
     * This function only inspects the loaded Blueprint and never modifies it.
     * @param LevelScript The loaded LookAtPOI_Level LevelScriptBlueprint.
     * @return 0 if unsupported, 1 if the exact five-node migrated graph exists,
     * or 2 if the exact nine-node original graph is ready for migration.
     */
    UFUNCTION(BlueprintPure, Category = "Sample|Migration")
    static int32 ClassifyLevelCommandChains(UBlueprint* LevelScript);

    /**
     * Replaces two verified console-command chains with native forwarding calls.
     *
     * The caller must classify the loaded, nested LevelScriptBlueprint and
     * create a verified map backup before calling this mutating function.
     * This function never creates a level Blueprint or saves a package. Each
     * original ReceiveBeginPlay / ReceiveDestroyed event stays in its graph,
     * and the unconnected ReceiveTick event is preserved. Preflight verifies
     * the whole nine-node graph and both per-command player-0 dependencies.
     *
     * @param LevelScript The loaded LookAtPOI_Level LevelScriptBlueprint.
     * @return 2 if modified, 1 if the exact five-node migrated graph already
     * exists, or 0 if the graph differs or migration cannot be completed.
     * A zero after mutation requires discarding unsaved level edits.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ForwardLevelCommandChains(UBlueprint* LevelScript);
};
