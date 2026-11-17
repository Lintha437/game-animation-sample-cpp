#pragma once

#include "Sandbox/SampleMultiplayerSpawnGameMode.h"
#include "SampleSandboxGameMode.generated.h"

struct FStreamableHandle;

/**
 * Authoritative pawn selection and replacement for the reference sandbox.
 * Map assets continue to select their GameMode subclass and its authored pawn options.
 */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleSandboxGameMode : public ASampleMultiplayerSpawnGameMode
{
    GENERATED_BODY()

public:
    /**
     * Resolves the selected pawn for login and restart. Missing or unusable choices
     * fall back to the default pawn class of the active GameMode.
     * @param InController Controller for which the engine is requesting a pawn.
     * @return The resolved pawn class.
     */
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

    /**
     * Restarts possessed players at their current transforms with their aim preserved.
     * Requires the selected class to be loaded. Controllers that do not yet possess a
     * pawn remain in the normal login and restart flow.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Sandbox")
    void ResetAllPlayers();

    /** Advances the authored pawn index, then lets the existing selection delegate restart players. */
    void CyclePawn();

    /** Advances the authored presentation index, including the default selection at index -1. */
    void CycleVisualOverride();

    /** Returns the configured visual class for the current selection, or an empty choice. */
    TSoftClassPtr<AActor> GetSelectedVisualOverride() const;

protected:
    /** Subscribes to the authored pawn-selection console variable. */
    virtual void BeginPlay() override;

    /** Unsubscribes and cancels pending work before the world tears down. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Reads the indexed soft class while preserving subclass-authored options. */
    TSoftClassPtr<APawn> GetSelectedPawn() const;

    /** Starts loading the current requested pawn and supersedes any pending request. */
    UFUNCTION()
    void HandlePawnSelectionChanged(FString VariableName);

    /** Owned asynchronous request for the latest console-variable selection. */
    TSharedPtr<FStreamableHandle> PendingPawnLoad;
};
