#include "SmartObjects/SampleNpcController.h"

#include "Components/StateTreeAIComponent.h"
#include "DrawDebugLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void ASampleNpcController::DebugNative()
{
    const FDebugDrawer Drawer = UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(
        this,
        FName(TEXT("Draw Debug Library")),
        EDrawDebugLogVerbosity::Display,
        true,
        true);

    const APawn* ControlledPawn = GetPawn();
    const FVector StartLocation = ControlledPawn ? ControlledPawn->GetActorLocation() : FVector::ZeroVector;
    const FRotator PawnControlRotation = ControlledPawn ? ControlledPawn->GetControlRotation() : FRotator::ZeroRotator;
    const FVector EndLocation = StartLocation + (PawnControlRotation.Vector() * 50.0f);

    const FDrawDebugLineStyle LineStyle(
        FLinearColor(1.0f, 0.0f, 1.0f, 1.0f),
        0.0f,
        EDrawDebugLineType::Solid,
        2.5f,
        1.0f,
        1.0f);

    FDrawDebugArrowSettings ArrowSettings;
    ArrowSettings.bArrowheadOnStart = false;
    ArrowSettings.bArrowLineEndsAtStartHead = false;
    ArrowSettings.ArrowHeadStartSize = 5.0f;
    ArrowSettings.ArrowHeadStartType = EDrawDebugArrowHead::Simple;
    ArrowSettings.bArrowheadOnEnd = true;
    ArrowSettings.bArrowLineEndsAtEndHead = false;
    ArrowSettings.ArrowHeadEndSize = 5.0f;
    ArrowSettings.ArrowHeadEndType = EDrawDebugArrowHead::Simple;

    UDrawDebugLibrary::DrawDebugArrow(Drawer, StartLocation, EndLocation, LineStyle, true, ArrowSettings);
}

void ASampleNpcController::BeginPlay()
{
    Super::BeginPlay();
    const float StartupDelay = GetNetMode() == NM_DedicatedServer ? 8.0f : 2.0f;
    GetWorldTimerManager().SetTimer(StateTreeStartupTimer, this, &ASampleNpcController::StartNpcStateTree, StartupDelay, false);
}

void ASampleNpcController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(StateTreeStartupTimer);
    Super::EndPlay(EndPlayReason);
}

void ASampleNpcController::StartNpcStateTree()
{
    if (auto* StateTree = FindComponentByClass<UStateTreeAIComponent>())
    {
        StateTree->StartLogic();
    }
}
