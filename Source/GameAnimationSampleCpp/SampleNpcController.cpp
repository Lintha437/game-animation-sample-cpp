#include "SampleNpcController.h"

#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"

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
