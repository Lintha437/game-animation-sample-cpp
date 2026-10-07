#include "SampleLocomotorExample.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Math/RotationMatrix.h"

ASampleLocomotorController::ASampleLocomotorController()
    : MoveAction(FSoftObjectPath(TEXT("/Game/Input/IA_Move.IA_Move")))
    , MouseLookAction(FSoftObjectPath(TEXT("/Game/Input/IA_Look.IA_Look")))
    , GamepadLookAction(FSoftObjectPath(TEXT("/Game/Input/IA_Look_Gamepad.IA_Look_Gamepad")))
{
}

void ASampleLocomotorController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (auto* Input = Cast<UEnhancedInputComponent>(InputComponent))
    {
        const auto Bind = [this, Input](const TSoftObjectPtr<UInputAction>& Action, auto Handler)
        {
            if (const UInputAction* Loaded = Action.LoadSynchronous())
            {
                Input->BindAction(Loaded, ETriggerEvent::Triggered, this, Handler);
            }
        };
        Bind(MoveAction, &ASampleLocomotorController::MoveFromAction);
        Bind(MouseLookAction, &ASampleLocomotorController::LookFromMouse);
        Bind(GamepadLookAction, &ASampleLocomotorController::LookFromGamepad);
    }
}

void ASampleLocomotorController::MoveFromAction(const FInputActionValue& Value)
{
    if (APawn* ControlledPawn = GetPawn())
    {
        const FVector2D Direction = Value.Get<FVector2D>().GetSafeNormal();
        const FRotationMatrix Facing(GetControlRotation());
        ControlledPawn->AddMovementInput(Facing.GetUnitAxis(EAxis::Y), Direction.X);
        ControlledPawn->AddMovementInput(Facing.GetUnitAxis(EAxis::X), Direction.Y);
    }
}

void ASampleLocomotorController::LookFromMouse(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>() / 15.0;
    AddYawInput(Look.X);
    AddPitchInput(Look.Y);
}

void ASampleLocomotorController::LookFromGamepad(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>() * GetWorld()->GetDeltaSeconds();
    AddYawInput(Look.X);
    AddPitchInput(Look.Y);
}

ASampleLocomotorWalker::ASampleLocomotorWalker()
    : InputMapping(FSoftObjectPath(TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox")))
{
}

void ASampleLocomotorWalker::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InstallLocalMappings();
}

void ASampleLocomotorWalker::PawnClientRestart()
{
    Super::PawnClientRestart();
    InstallLocalMappings();
}

void ASampleLocomotorWalker::InstallLocalMappings()
{
    const auto* Player = Cast<APlayerController>(GetController());
    auto* Input = Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer()) : nullptr;
    if (Input)
    {
        if (const UInputMappingContext* Mapping = InputMapping.LoadSynchronous(); Mapping && !Input->HasMappingContext(Mapping))
        {
            Input->AddMappingContext(Mapping, 0);
        }
    }
}
