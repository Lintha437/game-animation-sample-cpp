#include "SampleLocomotionAnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"

FSampleLocomotionSnapshot FSampleLocomotionSnapshot::Capture(const ACharacter* Character)
{
    FSampleLocomotionSnapshot Result;
    if (!Character)
    {
        return Result;
    }

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FRotator Heading(0.0, Character->GetActorRotation().Yaw, 0.0);
    Result.LocalVelocity = Heading.UnrotateVector(Movement->Velocity);
    Result.GroundSpeed = Result.LocalVelocity.Size2D();
    if (Result.GroundSpeed > UE_KINDA_SMALL_NUMBER)
    {
        Result.DirectionDegrees = FMath::RadiansToDegrees(
            FMath::Atan2(Result.LocalVelocity.Y, Result.LocalVelocity.X));
    }
    Result.bHasAcceleration = !Movement->GetCurrentAcceleration().IsNearlyZero();
    Result.bIsFalling = Movement->IsFalling();
    Result.bIsCrouched = Character->bIsCrouched;

    if (const ASampleLocomotionCharacter* NativeCharacter = Cast<ASampleLocomotionCharacter>(Character))
    {
        Result.RequestedGait = NativeCharacter->GetRequestedGait();
        Result.bIsStrafing = NativeCharacter->IsStrafing();
    }
    return Result;
}

void USampleLocomotionAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    Locomotion = FSampleLocomotionSnapshot();
}

void USampleLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    Locomotion = FSampleLocomotionSnapshot::Capture(Cast<ACharacter>(TryGetPawnOwner()));
}
