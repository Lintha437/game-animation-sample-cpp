#include "SampleCmcCharacter.h"

#include "Curves/CurveFloat.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetAnimationLibrary.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"

namespace
{
    // The imported user-defined enums serialize their authored entries in this order.
    constexpr int64 WalkGait = 0;
    constexpr int64 RunGait = 1;
    constexpr int64 SprintGait = 2;
    const FName GaitName(TEXT("Gait"));
    const FName StickModeName(TEXT("MovementStickMode"));

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
        return ReadEnumValue(Owner, GaitName, RunGait);
    }

    void WriteGait(UObject& Owner, int64 Gait)
    {
        FProperty* Property = Owner.GetClass()->FindPropertyByName(GaitName);
        if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                EnumProperty->ContainerPtrToValuePtr<void>(&Owner), Gait);
        }
        else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            ByteProperty->SetPropertyValue_InContainer(&Owner, static_cast<uint8>(Gait));
        }
    }

    double GetActionMagnitude(const UEnhancedInputComponent* Input, const UInputAction* Action)
    {
        return Input && Action ? Input->GetBoundActionValue(Action).Get<FVector2D>().Size() : 0.0;
    }
}

ASampleCmcCharacter::ASampleCmcCharacter()
    : CameraRelativeMoveAction(FSoftObjectPath(TEXT("/Game/Input/IA_Move.IA_Move")))
    , WorldRelativeMoveAction(FSoftObjectPath(TEXT("/Game/Input/IA_Move_WorldSpace.IA_Move_WorldSpace")))
    , LookAction(FSoftObjectPath(TEXT("/Game/Input/IA_Look.IA_Look")))
    , GamepadLookAction(FSoftObjectPath(TEXT("/Game/Input/IA_Look_Gamepad.IA_Look_Gamepad")))
    , CrouchAction(FSoftObjectPath(TEXT("/Game/Input/IA_Crouch.IA_Crouch")))
{
}

void ASampleCmcCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!Input)
    {
        return;
    }
    const UInputAction* CameraMove = CameraRelativeMoveAction.LoadSynchronous();
    const UInputAction* WorldMove = WorldRelativeMoveAction.LoadSynchronous();
    if (CameraMove)
    {
        Input->BindActionValue(CameraMove);
    }
    if (WorldMove)
    {
        Input->BindActionValue(WorldMove);
    }
    if (!bUseNativeInputHandlers)
    {
        return;
    }

    const auto Bind = [this, Input](const UInputAction* Action, auto Handler)
    {
        if (Action)
        {
            Input->BindAction(Action, ETriggerEvent::Triggered, this, Handler);
        }
    };
    Bind(CameraMove, &ASampleCmcCharacter::HandleCameraRelativeMove);
    Bind(WorldMove, &ASampleCmcCharacter::HandleWorldRelativeMove);
    Bind(LookAction.LoadSynchronous(), &ASampleCmcCharacter::HandleLook);
    Bind(GamepadLookAction.LoadSynchronous(), &ASampleCmcCharacter::HandleGamepadLook);
    Bind(CrouchAction.LoadSynchronous(), &ASampleCmcCharacter::HandleCrouch);
}

void ASampleCmcCharacter::HandleCameraRelativeMove(const FInputActionValue& Value)
{
    const FVector2D Scaled = GetMovementInputScaleValue(Value.Get<FVector2D>());
    const FRotationMatrix View(FRotator(0.0, GetControlRotation().Yaw, 0.0));
    AddMovementInput(View.GetUnitAxis(EAxis::Y), Scaled.X);
    AddMovementInput(View.GetUnitAxis(EAxis::X), Scaled.Y);
}

void ASampleCmcCharacter::HandleWorldRelativeMove(const FInputActionValue& Value)
{
    const FVector2D Scaled = Value.Get<FVector2D>().GetSafeNormal();
    AddMovementInput(FVector::RightVector, Scaled.X);
    AddMovementInput(FVector::ForwardVector, Scaled.Y);
}

void ASampleCmcCharacter::HandleLook(const FInputActionValue& Value)
{
    AddLookInput(Value.Get<FVector2D>());
}

void ASampleCmcCharacter::HandleGamepadLook(const FInputActionValue& Value)
{
    AddLookInput(Value.Get<FVector2D>() * GetWorld()->GetDeltaSeconds());
}

void ASampleCmcCharacter::AddLookInput(const FVector2D& Delta)
{
    AddControllerYawInput(Delta.X);
    AddControllerPitchInput(Delta.Y);
}

void ASampleCmcCharacter::HandleCrouch()
{
    if (GetCharacterMovement()->IsFalling())
    {
        return;
    }
    if (bIsCrouched)
    {
        UnCrouch();
    }
    else
    {
        Crouch();
    }
}

int64 ASampleCmcCharacter::SelectGait() const
{
    static const FName ThresholdName(TEXT("AnalogWalk/RunThreshold"));
    static const FName WalkIntent(TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3"));
    const int64 StickMode = ReadEnumValue(*this, StickModeName, 2);
    const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
    const FNumericProperty* ThresholdProperty = FindFProperty<FNumericProperty>(GetClass(), ThresholdName);
    const double Threshold = ThresholdProperty && ThresholdProperty->IsFloatingPoint()
        ? ThresholdProperty->GetFloatingPointPropertyValue(ThresholdProperty->ContainerPtrToValuePtr<void>(this))
        : 0.5;
    const bool bFullInput = StickMode == 0 || StickMode == 2 ||
        GetActionMagnitude(Input, CameraRelativeMoveAction.Get()) >= Threshold ||
        GetActionMagnitude(Input, WorldRelativeMoveAction.Get()) >= Threshold;

    if (CanSprint())
    {
        return bFullInput ? SprintGait : RunGait;
    }
    return ReadInputIntent(*this, WalkIntent) || !bFullInput ? WalkGait : RunGait;
}

void ASampleCmcCharacter::UpdateMovementBeforeSimulation()
{
    WriteGait(*this, SelectGait());
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxAcceleration = CalculateMaxAcceleration();
    Movement->BrakingDecelerationWalking = CalculateBrakingDeceleration();
    Movement->GroundFriction = CalculateGroundFriction();
    Movement->MaxWalkSpeed = CalculateMaxSpeed();
    Movement->MaxWalkSpeedCrouched = CalculateMaxCrouchSpeed();
}

double ASampleCmcCharacter::CalculateBrakingDeceleration() const
{
    const bool bHasInput = UKismetMathLibrary::NotEqual_VectorVector(GetPendingMovementInputVector(), FVector::ZeroVector);
    return bHasInput ? 500.0 : 2000.0;
}

FVector2D ASampleCmcCharacter::GetMovementInputScaleValue(FVector2D Input) const
{
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
