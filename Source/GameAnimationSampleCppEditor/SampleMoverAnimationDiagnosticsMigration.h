#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleMoverAnimationDiagnosticsMigration.generated.h"

class UBlueprint;

/** Structural state of the audited status line and selected debug panel rows. */
UENUM(BlueprintType)
enum class ESampleAnimationStateLineMigrationState : uint8
{
    Rejected,
    Original,
    Installed
};

/** Editor-only K2 migration for the shared animation diagnostic text formatters. */
UCLASS()
class USampleMoverAnimationDiagnosticsMigration : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Inspects all targeted text edges. Mixed original and installed nodes are rejected. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static ESampleAnimationStateLineMigrationState ClassifyStateLine(UBlueprint* Blueprint);

    /** Replaces the original formatters in memory. The caller owns backup and save. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool ReplaceStateLine(UBlueprint* Blueprint);

    /** Compiles the installed formatters with saving disabled and rechecks the resulting graph. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool CompileAndVerifyStateLine(UBlueprint* Blueprint);
};
