#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleMoverAnimationDiagnostics.generated.h"

/** Labels used by the animation graph debug panels. */
UENUM(BlueprintType)
enum class ESampleAnimationDiagnosticField : uint8
{
    CurrentState,
    LastTransition,
    CurrentAnim,
    LastMotionMatchingCost
};

/** Shared status text formatting for the CMC and Mover animation diagnostics. */
UCLASS()
class GAMEANIMATIONSAMPLECPP_API USampleMoverAnimationDiagnostics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Joins the existing enum display strings in draw order using the authored separator. */
    UFUNCTION(BlueprintPure, Category = "Sample|Animation|Diagnostics")
    static FString FormatStateLine(const FString& MovementMode, const FString& Stance,
        const FString& Gait, const FString& MovementDirection);

    /** Prefixes an already-converted value with its authored debug panel label. */
    UFUNCTION(BlueprintPure, Category = "Sample|Animation|Diagnostics")
    static FString FormatGraphDiagnosticLine(ESampleAnimationDiagnosticField Field, const FString& Value);
};
