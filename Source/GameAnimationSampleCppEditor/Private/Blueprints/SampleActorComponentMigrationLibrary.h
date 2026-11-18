#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleActorComponentMigrationLibrary.generated.h"

class UBlueprint;
class UWorld;

/** Replaces retired sandbox ActorComponent Blueprint subclasses with their native parent classes. */
UCLASS()
class USampleActorComponentMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Replaces SCS templates and inherited overrides referencing retired tick or visual-override
     * Blueprints with native C++ component classes while keeping variable names, GUIDs, tick groups,
     * activation defaults, replication flags, and tags intact.
     *
     * @param Blueprint Target pawn or actor Blueprint.
     * @param bValidateOnly Validate conversion without mutating the Blueprint.
     * @return Number of matching SCS or override templates replaced, or -1 on invalid input or unsupported overrides.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ReplaceActorComponentClassesInBlueprint(UBlueprint* Blueprint, bool bValidateOnly);

    /**
     * Re-runs construction scripts on actors in the loaded level that still instantiate any retired
     * sandbox ActorComponent Blueprint class, returning the number of refreshed actors.
     *
     * @param World Loaded editor level world.
     * @return Number of refreshed actors, or -1 if World is null or any legacy component remains after refresh.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 RefreshLegacyComponentActorsInLevel(UWorld* World);
};
