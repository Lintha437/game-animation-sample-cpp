#include "SampleLocomotionCharacter.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"

ASampleLocomotionCharacter::ASampleLocomotionCharacter()
{
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
}

void ASampleLocomotionCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    SetRequestedGait(StandingGait);
    SetStrafing(false);
}

void ASampleLocomotionCharacter::MoveRelativeToYaw(FVector2D Axis, float ReferenceYaw)
{
    if (Axis.ContainsNaN() || Axis.IsNearlyZero() || !FMath::IsFinite(ReferenceYaw))
    {
        return;
    }

    Axis = Axis.GetSafeNormal() * FMath::Min(Axis.Size(), 1.0);
    const FRotator Rotation(0.0, ReferenceYaw, 0.0);
    const FVector WorldInput = Rotation.RotateVector(FVector(Axis.Y, Axis.X, 0.0));
    AddMovementInput(WorldInput);
}

void ASampleLocomotionCharacter::SetRequestedGait(ESampleGait Gait)
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    switch (Gait)
    {
    case ESampleGait::Walk:
        Movement->MaxWalkSpeed = WalkSpeed;
        break;
    case ESampleGait::Run:
        Movement->MaxWalkSpeed = RunSpeed;
        break;
    case ESampleGait::Sprint:
        Movement->MaxWalkSpeed = SprintSpeed;
        break;
    default:
        return;
    }

    RequestedGait = Gait;
    if (Gait != ESampleGait::Sprint)
    {
        StandingGait = Gait;
    }
}

void ASampleLocomotionCharacter::SetStrafing(bool bEnabled)
{
    bStrafing = bEnabled;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = !bEnabled;
    Movement->bUseControllerDesiredRotation = bEnabled;
}

void ASampleLocomotionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!Input)
    {
        return;
    }

    auto Bind = [this, Input](const UInputAction* Action, ETriggerEvent Event, auto Callback)
    {
        if (Action)
        {
            Input->BindAction(Action, Event, this, Callback);
        }
    };

    Bind(CameraMoveAction, ETriggerEvent::Triggered, &ASampleLocomotionCharacter::InputMove);
    Bind(WorldMoveAction, ETriggerEvent::Triggered, &ASampleLocomotionCharacter::InputWorldMove);
    Bind(LookAction, ETriggerEvent::Triggered, &ASampleLocomotionCharacter::InputLook);
    Bind(WalkAction, ETriggerEvent::Started, &ASampleLocomotionCharacter::ToggleWalk);

    auto BindHold = [&Bind](const UInputAction* Action, auto Press, auto Release)
    {
        Bind(Action, ETriggerEvent::Started, Press);
        Bind(Action, ETriggerEvent::Completed, Release);
        Bind(Action, ETriggerEvent::Canceled, Release);
    };

    BindHold(JumpAction, &ASampleLocomotionCharacter::Jump, &ASampleLocomotionCharacter::StopJumping);
    BindHold(CrouchAction, &ASampleLocomotionCharacter::StartCrouch, &ASampleLocomotionCharacter::StopCrouch);
    BindHold(SprintAction, &ASampleLocomotionCharacter::StartSprint, &ASampleLocomotionCharacter::StopSprint);
    BindHold(StrafeAction, &ASampleLocomotionCharacter::StartStrafe, &ASampleLocomotionCharacter::StopStrafe);
}

void ASampleLocomotionCharacter::InputMove(const FInputActionValue& Value)
{
    MoveRelativeToYaw(Value.Get<FVector2D>(), GetControlRotation().Yaw);
}

void ASampleLocomotionCharacter::InputWorldMove(const FInputActionValue& Value)
{
    MoveRelativeToYaw(Value.Get<FVector2D>(), 0.0f);
}

void ASampleLocomotionCharacter::InputLook(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

void ASampleLocomotionCharacter::StartCrouch()
{
    Crouch();
}

void ASampleLocomotionCharacter::StopCrouch()
{
    UnCrouch();
}

void ASampleLocomotionCharacter::ToggleWalk()
{
    const ESampleGait NextGait = StandingGait == ESampleGait::Walk ? ESampleGait::Run : ESampleGait::Walk;
    if (RequestedGait == ESampleGait::Sprint)
    {
        StandingGait = NextGait;
        return;
    }
    SetRequestedGait(NextGait);
}

void ASampleLocomotionCharacter::StartSprint()
{
    SetRequestedGait(ESampleGait::Sprint);
}

void ASampleLocomotionCharacter::StopSprint()
{
    SetRequestedGait(StandingGait);
}

void ASampleLocomotionCharacter::StartStrafe()
{
    SetStrafing(true);
}

void ASampleLocomotionCharacter::StopStrafe()
{
    SetStrafing(false);
}
