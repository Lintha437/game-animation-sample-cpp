#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleAnimationDrawContextMigration.generated.h"

class UBlueprint;
class UEdGraph;

/** Identifies the supported state of a diagnostic function graph. */
UENUM(BlueprintType)
enum class ESampleDrawRotationGraphState : uint8
{
    Rejected,
    Original,
    Installed
};

/**
 * Installs the shared native draw-rotation preamble in the CMC and Mover
 * diagnostic function graphs after validating their existing graph topology.
 */
UCLASS()
class USampleAnimationDrawContextMigration final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Audits one exact target without changing the Blueprint or its graph. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static ESampleDrawRotationGraphState ClassifySharedDrawRotation(UBlueprint* Blueprint, UEdGraph* Graph);

    /**
     * Validates and installs the shared draw-rotation resolver in both active
     * Animation Blueprint diagnostic graphs as one editor transaction.
     *
     * The installer requires the known SetDrawOrigin, IsValid, controller,
     * rotation-setter, Sequence, local-control gate, and CVar topology. It
     * reuses the valid-branch SetDrawRotation node and removes only the obsolete
     * IsValid/controller/rotation/invalid-setter nodes. A failed connection
     * restores the original pin links in every graph prepared by this call.
     *
     * Repeated calls succeed without adding duplicate nodes or links.
     *
     * @param CmcBlueprint Blueprint owning Debug_ExperimentalStateMachine.
     * @param CmcGraph Active CMC diagnostic function graph.
     * @param MoverBlueprint Blueprint owning DebugDraws.
     * @param MoverGraph Active Mover diagnostic function graph.
     * @return True when both graphs are already installed or are installed successfully.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static bool InstallSharedDrawRotationPair(
        UBlueprint* CmcBlueprint,
        UEdGraph* CmcGraph,
        UBlueprint* MoverBlueprint,
        UEdGraph* MoverGraph);
};
