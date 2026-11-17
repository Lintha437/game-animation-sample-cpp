#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleControllerInputDisplay.generated.h"

class APlayerController;

/**
 * @brief Reproduces the input sampling and presentation math used by WB_SimpleController.
 *
 * The widget owns its PlayerController reference, sampled vectors, update cadence, and
 * presentation widgets. It calls these stateless functions from its existing Blueprint
 * graphs. Every UObject argument is borrowed for the duration of the call. This library
 * retains no controller, world, widget, or input state and does not own the PIE utility
 * host or the widget's creation and removal lifecycle.
 */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleControllerInputDisplay : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * @brief Accumulates the analog state of the supplied keys in their original array order.
     *
     * Each addition is narrowed to single precision before the next key is sampled. Duplicate
     * keys are counted repeatedly, preserving the original Blueprint loop's behavior.
     *
     * @param PlayerController Borrowed controller selected by the owning widget. A null pointer
     *                         produces zero without retaining or resolving another controller.
     * @param Keys             Borrowed key array. Its order and duplicate entries are significant.
     * @return The single-precision sum of the sampled key values, or zero for a null controller
     *         or an empty array.
     */
    UFUNCTION(BlueprintPure, Category="Sample|Input Display", meta=(BlueprintInternalUseOnly="true"))
    static float SampleAnalogKeys(APlayerController* PlayerController, const TArray<FKey>& Keys);

    /**
     * @brief Samples the current W, S, A, and D analog key states as a two-dimensional vector.
     *
     * The X component is D minus A and the Y component is W minus S. The result is not
     * normalized, so diagonal input can have a magnitude greater than one.
     *
     * @param PlayerController Borrowed controller selected by the owning widget.
     * @return The keyboard contribution with a zero Z component, or zero for a null controller.
     */
    UFUNCTION(BlueprintPure, Category="Sample|Input Display", meta=(BlueprintInternalUseOnly="true"))
    static FVector SampleWSAD(APlayerController* PlayerController);

    /**
     * @brief Converts this frame's mouse delta into the smoothed right-stick contribution.
     *
     * The mouse delta is divided by the current world delta and then by 100. Y is inverted,
     * the target vector is clamped to unit length, and VInterpTo uses speed 10. The caller
     * supplies the previous widget-owned sample as Current. A zero world delta yields a zero
     * target and a zero interpolation step, retaining Current for that call.
     *
     * @param WorldContextObject Borrowed object used to resolve the active world and its delta.
     * @param PlayerController   Borrowed controller whose per-frame mouse delta is sampled.
     * @param Current            Previous MOUSE value owned by the widget.
     * @return The interpolated mouse contribution with a zero Z component.
     */
    UFUNCTION(BlueprintPure, Category="Sample|Input Display", meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly="true"))
    static FVector SampleMouse(const UObject* WorldContextObject, APlayerController* PlayerController, FVector Current);

    /**
     * @brief Combines the left gamepad stick with the widget's stored WSAD contribution.
     *
     * Opacity is the unbounded XY magnitude when it is strictly greater than 0.2 and zero
     * otherwise. Translation uses a full-vector size clamp to [0, 1], then scales X by 40
     * and Y by -40. The caller owns the output values and applies them to the authored widgets.
     *
     * @param PlayerController Borrowed controller selected by the owning widget.
     * @param Contribution     Previous WSAD sample owned by the widget.
     * @param Opacity          Receives the raw thresholded magnitude as a float.
     * @param TranslationX     Receives the clamped X translation as a double.
     * @param TranslationY     Receives the clamped, inverted Y translation as a double.
     */
    UFUNCTION(BlueprintPure, Category="Sample|Input Display", meta=(BlueprintInternalUseOnly="true"))
    static void SampleLeftStick(APlayerController* PlayerController, FVector Contribution,
        float& Opacity, double& TranslationX, double& TranslationY);

    /**
     * @brief Combines the right gamepad stick with the widget's stored MOUSE contribution.
     *
     * Opacity is the unbounded XY magnitude when it is strictly greater than 0.2 and zero
     * otherwise. Translation uses a full-vector size clamp to [0, 1], then scales both X and
     * Y by 40. The caller owns the output values and applies them to the authored widgets.
     *
     * @param PlayerController Borrowed controller selected by the owning widget.
     * @param Contribution     Previous MOUSE sample owned by the widget.
     * @param Opacity          Receives the raw thresholded magnitude as a float.
     * @param TranslationX     Receives the clamped X translation as a double.
     * @param TranslationY     Receives the clamped Y translation as a double.
     */
    UFUNCTION(BlueprintPure, Category="Sample|Input Display", meta=(BlueprintInternalUseOnly="true"))
    static void SampleRightStick(APlayerController* PlayerController, FVector Contribution,
        float& Opacity, double& TranslationX, double& TranslationY);
};
