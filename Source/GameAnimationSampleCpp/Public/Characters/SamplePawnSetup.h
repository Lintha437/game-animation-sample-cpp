#pragma once

#include "CoreMinimal.h"

class APawn;
class UInputComponent;

namespace SamplePawnSetup
{
    /** Adds the authored sandbox input context for the local owner. */
    void InstallInputMapping(APawn& Pawn);

    /** Activates the authored camera stack or the named legacy camera for the current controller. */
    void ActivateCamera(APawn& Pawn, FName LegacyCamera);

    /** Binds mouse-wheel and D-pad camera style changes to this pawn's input component. */
    void BindCameraStyleKeys(APawn& Pawn, UInputComponent& Input);
}
