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

    /**
     * Prepares the authored Chooser input from the cached camera interface state and current camera-style CVar.
     * @param CharacterProperties Cached S_CharacterPropertiesForCamera. Preserved when output is separate.
     * @param SelectionProperties Caller-owned output of the same struct type; use separate storage for the Chooser.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Camera",
        meta = (CustomStructureParam = "CharacterProperties,SelectionProperties"))
    static void PrepareImportedCameraSelection(const int32& CharacterProperties, int32& SelectionProperties);

    /** Validates both wildcard types before preparing the Chooser input. */
    DECLARE_FUNCTION(execPrepareImportedCameraSelection);

    /**
     * Tests whether the authored crouch offset rig should be activated this evaluation.
     * Reads the camera-style CVar independently of the earlier Chooser evaluation.
     * @param CharacterProperties Cached S_CharacterPropertiesForCamera.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Camera",
        meta = (CustomStructureParam = "CharacterProperties"))
    static bool ShouldActivateImportedCrouchRig(const int32& CharacterProperties);

    /** Validates the wildcard type before reading the cached stance. */
    DECLARE_FUNCTION(execShouldActivateImportedCrouchRig);

    /**
     * Copies the cached authored state, overriding only the style with the Blueprint's integer-to-byte
     * and safe byte-to-enum conversions. Returns false without writing for invalid storage or schema.
     * Both storage pointers must refer to initialized instances of Struct; they may alias.
     */
    static bool BuildCameraSelection(const UScriptStruct& Struct, const void* CharacterProperties,
        void* SelectionProperties, int32 CameraStyle);

    /** Evaluates the original integer style and stance checks without narrowing the style. */
    static bool IsCrouchRigEligible(int32 CameraStyle, uint8 Stance);
};
