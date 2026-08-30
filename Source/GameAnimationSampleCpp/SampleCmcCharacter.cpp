#include "SampleCmcCharacter.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
    // The imported user-defined enums serialize their authored entries in this order.
    constexpr int64 WalkGait = 0;
    constexpr int64 RunGait = 1;
    constexpr int64 SprintGait = 2;

    int64 ReadEnumValue(const UObject& Owner, FName Name, int64 DefaultValue)
    {
        const FProperty* Property = Owner.GetClass()->FindPropertyByName(Name);
        if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            return EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(&Owner));
        }
        if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            return ByteProperty->GetPropertyValue_InContainer(&Owner);
        }
        return DefaultValue;
    }

    bool ReadInputIntent(const UObject& Owner, FName Name)
    {
        static const FName InputStateName(TEXT("CharacterInputState"));
        const FStructProperty* InputState = FindFProperty<FStructProperty>(Owner.GetClass(), InputStateName);
        const FBoolProperty* Intent = InputState ? FindFProperty<FBoolProperty>(InputState->Struct, Name) : nullptr;
        return Intent && Intent->GetPropertyValue_InContainer(InputState->ContainerPtrToValuePtr<void>(&Owner));
    }

    int64 ReadGait(const UObject& Owner)
    {
        static const FName GaitName(TEXT("Gait"));
        return ReadEnumValue(Owner, GaitName, RunGait);
    }
}

FVector2D ASampleCmcCharacter::GetMovementInputScaleValue(FVector2D Input) const
{
    static const FName StickModeName(TEXT("MovementStickMode"));
    const int64 Mode = ReadEnumValue(*this, StickModeName, 2);
    return Mode == 0 || Mode == 1 ? Input.GetSafeNormal() : Input;
}

bool ASampleCmcCharacter::CanSprint() const
{
    static const FName SprintIntent(TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936"));
    if (!ReadInputIntent(*this, SprintIntent))
    {
        return false;
    }
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (Movement->bOrientRotationToMovement)
    {
        return true;
    }

    const FVector Direction = IsLocallyControlled() ? GetPendingMovementInputVector() : Movement->GetCurrentAcceleration();
    return FMath::Abs((GetActorRotation() - Direction.Rotation()).GetNormalized().Yaw) < 50.0;
}

void ASampleCmcCharacter::UpdateRotation_PreCMC()
{
    static const FName StrafeIntent(TEXT("WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9"));
    static const FName AimIntent(TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8"));
    const bool bUseControllerRotation = ReadInputIntent(*this, StrafeIntent) || ReadInputIntent(*this, AimIntent);
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bUseControllerDesiredRotation = bUseControllerRotation;
    Movement->bOrientRotationToMovement = !bUseControllerRotation;
    Movement->RotationRate = FRotator(0.0, Movement->IsFalling() ? 200.0 : -1.0, 0.0);
}

double ASampleCmcCharacter::CalculateMaxAcceleration() const
{
    const double Speed = GetCharacterMovement()->Velocity.Size2D();
    return ReadGait(*this) == SprintGait
        ? FMath::GetMappedRangeValueClamped(FVector2D(300.0, 700.0), FVector2D(800.0, 300.0), Speed)
        : 800.0;
}

double ASampleCmcCharacter::CalculateGroundFriction() const
{
    const double Speed = GetCharacterMovement()->Velocity.Size2D();
    return ReadGait(*this) == SprintGait
        ? FMath::GetMappedRangeValueClamped(FVector2D(0.0, 500.0), FVector2D(5.0, 3.0), Speed)
        : 5.0;
}

double ASampleCmcCharacter::CalculateMaxSpeed() const
{
    static const FName WalkSpeeds(TEXT("WalkSpeeds"));
    static const FName RunSpeeds(TEXT("RunSpeeds"));
    static const FName SprintSpeeds(TEXT("SprintSpeeds"));
    const int64 Gait = ReadGait(*this);
    const FName SpeedsName = Gait == WalkGait ? WalkSpeeds : Gait == SprintGait ? SprintSpeeds : RunSpeeds;
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    return CalculateDirectionalSpeed(SpeedsName, Movement->MaxWalkSpeed, Movement->bUseControllerDesiredRotation);
}

double ASampleCmcCharacter::CalculateMaxCrouchSpeed() const
{
    static const FName CrouchSpeeds(TEXT("CrouchSpeeds"));
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    return CalculateDirectionalSpeed(CrouchSpeeds, Movement->MaxWalkSpeedCrouched, !Movement->bOrientRotationToMovement);
}

double ASampleCmcCharacter::CalculateDirectionalSpeed(FName SpeedsName, double Fallback, bool bUseStrafeCurve) const
{
    const FStructProperty* SpeedsProperty = FindFProperty<FStructProperty>(GetClass(), SpeedsName);
    if (!SpeedsProperty || SpeedsProperty->Struct != TBaseStructure<FVector>::Get())
    {
        return Fallback;
    }

    const FVector& Speeds = *SpeedsProperty->ContainerPtrToValuePtr<FVector>(this);
    if (!bUseStrafeCurve)
    {
        return Speeds.X;
    }

    static const FName StrafeCurveName(TEXT("StrafeSpeedMapCurve"));
    const FObjectPropertyBase* CurveProperty = FindFProperty<FObjectPropertyBase>(GetClass(), StrafeCurveName);
    const UCurveFloat* Curve = CurveProperty
        ? Cast<UCurveFloat>(CurveProperty->GetObjectPropertyValue_InContainer(this))
        : nullptr;
    const float Direction = FMath::Abs(
        UKismetAnimationLibrary::CalculateDirection(GetCharacterMovement()->Velocity, GetActorRotation()));
    const double Strafe = Curve ? Curve->GetFloatValue(Direction) : 0.0;
    return Strafe < 1.0
        ? FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0), FVector2D(Speeds.X, Speeds.Y), Strafe)
        : FMath::GetMappedRangeValueClamped(FVector2D(1.0, 2.0), FVector2D(Speeds.Y, Speeds.Z), Strafe);
}
