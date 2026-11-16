#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleNpcControllerDebugMigration.generated.h"

class UBlueprint;

/** Audits and forwards the imported NPC controller's callable Debug graph. */
UCLASS()
class GAMEANIMATIONSAMPLECPPEDITOR_API USampleNpcControllerDebugMigration : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Audits the complete Debug graph and native function contract without changing the asset.
     * Returns 0 for an unsupported graph, 1 for the verified native forward,
     * or 2 for the supported original graph.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ClassifyNpcDebug(UBlueprint* Blueprint);

    /**
     * Forwards the verified original Debug body to the native instance function.
     * Keeps the original graph and function entry. Returns 1 for a verified
     * forward or 0 on rejection. The caller owns compilation, backup and saving.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ForwardNpcDebug(UBlueprint* Blueprint);
};
