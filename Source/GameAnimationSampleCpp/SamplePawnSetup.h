#pragma once

#include "CoreMinimal.h"

class APawn;

namespace SamplePawnSetup
{
    /** Adds the authored sandbox input context for the local owner. */
    void InstallInputMapping(APawn& Pawn);

    /** Activates the authored camera stack or the named legacy camera for the current controller. */
    void ActivateCamera(APawn& Pawn, FName LegacyCamera);
}
