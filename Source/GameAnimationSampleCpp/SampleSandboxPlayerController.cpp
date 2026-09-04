#include "SampleSandboxPlayerController.h"

#include "SampleSandboxGameMode.h"

#include "EnhancedInputComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"

ASampleSandboxPlayerController::ASampleSandboxPlayerController()
    : TeleportAction(FSoftObjectPath(TEXT("/Game/Input/IA_TeleportToTarget.IA_TeleportToTarget")))
    , NextPawnAction(FSoftObjectPath(TEXT("/Game/Input/IA_NextPawn.IA_NextPawn")))
    , NextVisualOverrideAction(FSoftObjectPath(TEXT("/Game/Input/IA_NextVisualOverride.IA_NextVisualOverride")))
{
}

void ASampleSandboxPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
    {
        const auto Bind = [this, Input](const TSoftObjectPtr<UInputAction>& Action, auto Handler)
        {
            if (const UInputAction* Loaded = Action.LoadSynchronous())
            {
                Input->BindAction(Loaded, ETriggerEvent::Triggered, this, Handler);
            }
        };
        Bind(TeleportAction, &ASampleSandboxPlayerController::TeleportToTarget);
        Bind(NextPawnAction, &ASampleSandboxPlayerController::NextPawn);
        Bind(NextVisualOverrideAction, &ASampleSandboxPlayerController::NextVisualOverride);
    }
}

void ASampleSandboxPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if PLATFORM_ANDROID || PLATFORM_IOS
    if (UKismetSystemLibrary::IsControllerAssignedToGamepad(0))
    {
        SetVirtualJoystickVisibility(false);
    }
#endif
}

void ASampleSandboxPlayerController::TeleportToTarget()
{
    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn) || !GetWorld())
    {
        return;
    }
    FVector Start;
    FRotator View;
    GetActorEyesViewPoint(Start, View);
    const FVector End = Start + View.Vector() * TargetTeleportDistance;

    FHitResult Hit;
    const bool bBlocked = UKismetSystemLibrary::SphereTraceSingle(
        this, Start, End, 32.0f, ETraceTypeQuery::TraceTypeQuery1, false, {ControlledPawn},
        EDrawDebugTrace::ForDuration, Hit, true, FLinearColor::Red, FLinearColor::Green, 0.5f);
    const FVector Destination = bBlocked ? Hit.Location + Hit.ImpactNormal * 50.0 : End;
    ControlledPawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
}

void ASampleSandboxPlayerController::NextPawn_Implementation()
{
    if (ASampleSandboxGameMode* Mode = GetWorld()->GetAuthGameMode<ASampleSandboxGameMode>())
    {
        Mode->CyclePawn();
    }
}

void ASampleSandboxPlayerController::NextVisualOverride_Implementation()
{
    if (ASampleSandboxGameMode* Mode = GetWorld()->GetAuthGameMode<ASampleSandboxGameMode>())
    {
        Mode->CycleVisualOverride();
    }
}
