#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleMoverBlueprintMigrationLibrary.generated.h"

class UBlueprint;

/** Installs guarded native mode-transition dispatch while preserving authored presentation graphs. */
UCLASS()
class GAMEANIMATIONSAMPLECPPEDITOR_API USampleMoverBlueprintMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Returns 0 for an unsupported asset, 1 for a verified installation, or 2 for an audited source graph. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ClassifyMoverModeTransitions(UBlueprint* Blueprint);

    /** Installs the audited hooks and native dispatch in memory. The caller owns backup and saving. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 InstallMoverModeTransitions(UBlueprint* Blueprint);

    /** Verifies the installed graph and its compiled presentation overrides without modifying the asset. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool VerifyCompiledMoverModeTransitions(UBlueprint* Blueprint);

    /** Compiles without saving and verifies the installed graph again. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool CompileAndVerifyMoverModeTransitions(UBlueprint* Blueprint);
};
