#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleCameraStateLibrary.generated.h"

class APawn;

/** Supplies pawn state to the imported camera interfaces and rig selectors. */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleCameraStateLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Samples a pawn's camera state into the existing camera interface struct.
     * @param Pawn Camera subject. A missing pawn or invalid Mover produces the authored defaults.
     * @param Properties Caller-owned S_CharacterPropertiesForCamera storage. No references are retained.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Camera",
        meta = (CustomStructureParam = "Properties", BlueprintInternalUseOnly = "true"))
    static void BuildImportedCameraProperties(const APawn* Pawn, int32& Properties);

    /** Validates the wildcard output type before writing the camera interface struct. */
    DECLARE_FUNCTION(execBuildImportedCameraProperties);
};
