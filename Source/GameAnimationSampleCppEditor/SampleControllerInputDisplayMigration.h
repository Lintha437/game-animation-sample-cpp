#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleControllerInputDisplayMigration.generated.h"

class UBlueprint;

/**
 * @brief Validates and forwards the calculation graphs in the existing WB_SimpleController asset.
 *
 * This editor-only helper owns no asset packages and performs no disk I/O. The migration
 * script owns the backup, compilation, package save, and post-save verification. The
 * Editor Utility Widget that hosts the PIE toggle remains an authored asset and retains
 * responsibility for creating and removing the runtime controller widget.
 */
UCLASS()
class GAMEANIMATIONSAMPLECPPEDITOR_API USampleControllerInputDisplayMigration : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * @brief Classifies all seven controller-input graph slices as one consistent generation.
     *
     * @param Blueprint The WB_SimpleController asset to inspect.
     * @return 0 when unsupported or mixed, 1 when all forwarding calls are installed, or 2
     *         when all supported original graphs are present. This function does not mutate.
     */
    UFUNCTION(BlueprintCallable, Category="Sample|Migration")
    static int32 ClassifyInputDisplay(UBlueprint* Blueprint);

    /**
     * @brief Replaces the supported input calculations with calls to the native sampler in memory.
     *
     * All seven graph slices are validated before mutation. The function rejects dirty packages,
     * mixed generations, and incompatible replacement pins. It marks the Blueprint modified but
     * leaves compilation and saving to the calling migration script.
     *
     * @param Blueprint The WB_SimpleController asset to rewrite.
     * @return 0 on validation or rewrite failure, 1 when all forwarding calls are installed, or
     *         2 when the asset already contains the supported original graphs.
     */
    UFUNCTION(BlueprintCallable, Category="Sample|Migration")
    static int32 ForwardInputDisplay(UBlueprint* Blueprint);
};
