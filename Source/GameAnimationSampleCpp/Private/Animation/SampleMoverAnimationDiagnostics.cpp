#include "Animation/SampleMoverAnimationDiagnostics.h"

FString USampleMoverAnimationDiagnostics::FormatStateLine(const FString& MovementMode,
    const FString& Stance, const FString& Gait, const FString& MovementDirection)
{
    FString Line = MovementMode;
    Line += TEXT(" | ");
    Line += Stance;
    Line += TEXT(" | ");
    Line += Gait;
    Line += TEXT(" | ");
    Line += MovementDirection;
    return Line;
}

FString USampleMoverAnimationDiagnostics::FormatGraphDiagnosticLine(
    ESampleAnimationDiagnosticField Field, const FString& Value)
{
    FString Line;
    switch (Field)
    {
    case ESampleAnimationDiagnosticField::CurrentState:
        Line = TEXT("Current State: ");
        break;
    case ESampleAnimationDiagnosticField::LastTransition:
        Line = TEXT("Last Transition: ");
        break;
    case ESampleAnimationDiagnosticField::CurrentAnim:
        Line = TEXT("Current Anim: ");
        break;
    case ESampleAnimationDiagnosticField::LastMotionMatchingCost:
        Line = TEXT("Last MM Cost: ");
        break;
    }
    Line += Value;
    return Line;
}
