#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleMoverModeAssetMigrationLibrary.generated.h"

class UBlueprint;
class UCharacterMoverComponent;

/** Replaces imported Mover mode and transition Blueprint instances with native classes. */
UCLASS()
class USampleMoverModeAssetMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Replaces legacy Mover mode and transition instances on a CharacterMoverComponent.
     * The component owns newly instantiated mode and transition objects.
     * @param Mover Target movement component on a Blueprint template or level actor.
     * @param bValidateOnly Validate conversion without mutating the component.
     * @return Count of replaced mode or transition instances, or -1 on invalid input.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ReplaceMoverModeInstancesOnComponent(UCharacterMoverComponent* Mover, bool bValidateOnly);

    /**
     * Replaces legacy Mover mode and transition instances across a Blueprint's SCS and inherited templates.
     * The caller owns Blueprint compilation, backups, and package persistence.
     * @param Blueprint Pawn Blueprint containing CharacterMoverComponent templates or overrides.
     * @param bValidateOnly Validate conversion without modifying the Blueprint.
     * @return Count of replaced mode or transition instances, or -1 on invalid input.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Migration")
    static int32 ReplaceMoverModeInstancesInBlueprint(UBlueprint* Blueprint, bool bValidateOnly);
};
