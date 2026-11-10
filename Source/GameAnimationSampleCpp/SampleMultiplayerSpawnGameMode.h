#pragma once

#include "GameFramework/GameModeBase.h"
#include "SampleMultiplayerSpawnGameMode.generated.h"

/**
 * Server-side spawn placement shared by the demonstration GameModes.
 *
 * Preserves authored PlayerStart selection and pawn classes. When occupied,
 * searches nearby walkable ground for a clear location before possession.
 * Placement is calculated only on the authority and replicated normally.
 */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleMultiplayerSpawnGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    /**
     * Spawns the requested pawn at the authored transform or a nearby clear spot.
     * A blocked map with no suitable ground reports failure instead of stacking pawns.
     * @param NewPlayer Controller being spawned or restarted.
     * @param SpawnTransform Authored transform from the GameMode restart flow.
     * @return Newly spawned pawn, or nullptr if a safe location is unavailable.
     */
    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(
        AController* NewPlayer, const FTransform& SpawnTransform) override;
};
