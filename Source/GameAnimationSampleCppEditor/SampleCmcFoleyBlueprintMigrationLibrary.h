#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleCmcFoleyBlueprintMigrationLibrary.generated.h"

class UBlueprint;

/** Replaces only the two audited CMC Foley volume calculations. */
UCLASS()
class GAMEANIMATIONSAMPLECPPEDITOR_API USampleCmcFoleyBlueprintMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Returns 0 for an unsupported or mixed graph, 1 for an installed graph, or 2 for the supported source. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ClassifyCmcFoleyVolume(UBlueprint* Blueprint);

    /** Replaces both pure calls in memory after validating the complete boundary. The caller owns backup and saving. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 InstallCmcFoleyVolume(UBlueprint* Blueprint);

    /** Verifies the installed graph and current compiled class without saving. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool VerifyCompiledCmcFoleyVolume(UBlueprint* Blueprint);

    /** Compiles without saving and verifies the installed graph and compiled class. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool CompileAndVerifyCmcFoleyVolume(UBlueprint* Blueprint);
};
