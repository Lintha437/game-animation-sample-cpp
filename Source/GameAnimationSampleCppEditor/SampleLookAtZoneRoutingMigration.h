#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleLookAtZoneRoutingMigration.generated.h"

class UBlueprint;

/** Editor-only migration of the audited PC_LookAtPOI zone routing chain. */
UCLASS()
class USampleLookAtZoneRoutingMigration : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Returns 0 for unsupported, 1 for fully migrated, 2 for legacy, 3 for native entry with an authored Widget call, or 4 for native routing with the original gizmo expression. */
    UFUNCTION(BlueprintPure, Category = "Sample|Migration")
    static int32 ClassifyZoneRouting(UBlueprint* Blueprint);

    /** Replaces the audited controller path in memory. Requires a verified package backup before use. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool MigrateZoneRouting(UBlueprint* Blueprint);

    /** Discards the target package's in-memory edits and reloads its restored disk image. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReloadZoneRoutingPackage(UBlueprint* Blueprint);
};
