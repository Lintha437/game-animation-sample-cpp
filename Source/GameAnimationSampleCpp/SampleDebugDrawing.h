#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleDebugDrawing.generated.h"

/** Implements the shared diagnostic drawing functions used by the sample's animation debug views. */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleDebugDrawing : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Draws the selected circle, radial arrow and optional label in the caller's world. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawDirection(const UObject* WorldContextObject, bool DrawCircle, bool DrawAxis,
        bool DrawArrow, bool DrawString, const FString& String, FVector StringOffset,
        FVector Center, FVector Direction, FVector Offset, double Radius, double Length, double Size,
        int32 Segments, FLinearColor Color, double Duration, double Thickness);

    /** Draws tick marks over the supplied radial interval, applying each yaw to the base rotation. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawHeadingThresholds(const UObject* WorldContextObject, FVector Center, FVector Offset,
        FRotator InRotation, const TArray<double>& YawAngles, double StartRadius, double EndRadius,
        FLinearColor Color, float Duration, float Thickness);

    /** Draws each name and its state below the rotated origin. A missing state reads as false. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawBooleanRows(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
        FVector Offset, const TArray<FString>& BoolNames, const TArray<bool>& BoolValues);

    /** Draws the optional heading and string history with a case-insensitive highlight match. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawStringRows(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
        FVector Offset, const FString& Label, const FString& Prefix, const TArray<FString>& Strings,
        const FString& HighlightedString, const FString& Highlight);

    /** Draws the label and a line for each object's display name without retaining the objects. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Debug", meta = (WorldContext = "WorldContextObject"))
    static void DrawObjectNames(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
        FVector Offset, const FString& ArrayLabel, const TArray<UObject*>& Objects);

    /**
     * Draws axes, series and the legend using the imported S_DebugGraphLineProperties array.
     * @param WorldContextObject Owner for scene drawing and Visual Logger submissions.
     * @param Location World-space base of the plot.
     * @param Rotation Orientation of the plot and the local offset.
     * @param Offset Displacement in the rotated plot axes.
     * @param GraphSize Plot width and height in world units.
     * @param MinValue Lower value bound. Only Y is used because X spans the plot width.
     * @param MaxValue Upper value bound. Only Y is used because X spans the plot width.
     * @param XaxisLabel Label along the horizontal axis.
     * @param YaxisLabel Label along the vertical axis.
     * @param Lines Newest series first. The plot and legend submit in reverse order without modifying the caller's array.
     */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "Sample|Debug",
        meta = (WorldContext = "WorldContextObject", ArrayParm = "Lines", BlueprintInternalUseOnly = "true"))
    static void DrawHistoryGraph(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
        FVector Offset, FVector2D GraphSize, FVector2D MinValue, FVector2D MaxValue,
        const FString& XaxisLabel, const FString& YaxisLabel, const TArray<int32>& Lines);

    /** Resolves and validates the authored wildcard array before accessing its series. */
    DECLARE_FUNCTION(execDrawHistoryGraph);

    /**
     * Inserts a value at the front of the caller-owned history and removes the item at MaxHistoryCount if present.
     * Only one item is removed per call, including when the existing history is already oversized.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Debug")
    static void AddStringHistory(UPARAM(ref) TArray<FString>& InOutValues, const FString& NewValue, int32 MaxHistoryCount);
};
