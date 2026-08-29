#include "SampleCmcCharacter.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "UObject/UnrealType.h"

double ASampleCmcCharacter::CalculateMaxCrouchSpeed() const
{
    // Keep the serialized Blueprint fields and their inherited defaults in place.
    const FStructProperty* SpeedsProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("CrouchSpeeds"));
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!SpeedsProperty || SpeedsProperty->Struct != TBaseStructure<FVector>::Get())
    {
        return Movement->MaxWalkSpeedCrouched;
    }

    const FVector& Speeds = *SpeedsProperty->ContainerPtrToValuePtr<FVector>(this);
    if (Movement->bOrientRotationToMovement)
    {
        return Speeds.X;
    }

    const FObjectPropertyBase* CurveProperty = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("StrafeSpeedMapCurve"));
    const UCurveFloat* Curve = CurveProperty
        ? Cast<UCurveFloat>(CurveProperty->GetObjectPropertyValue_InContainer(this))
        : nullptr;
    const float Direction = FMath::Abs(UKismetAnimationLibrary::CalculateDirection(Movement->Velocity, GetActorRotation()));
    const double Strafe = Curve ? Curve->GetFloatValue(Direction) : 0.0;
    return Strafe < 1.0
        ? FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0), FVector2D(Speeds.X, Speeds.Y), Strafe)
        : FMath::GetMappedRangeValueClamped(FVector2D(1.0, 2.0), FVector2D(Speeds.Y, Speeds.Z), Strafe);
}
