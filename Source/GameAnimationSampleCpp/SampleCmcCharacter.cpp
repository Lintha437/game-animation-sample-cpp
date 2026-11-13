#include "SampleCmcCharacter.h"

#include "SamplePawnSetup.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KismetAnimationLibrary.h"
#include "Math/RotationMatrix.h"
#include "Net/Core/PushModel/PushModel.h"
#include "SampleAnimationTickComponent.h"
#include "SampleSmartObjectInteractionComponent.h"
#include "SampleTraversalClearance.h"
#include "SampleTraversalComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    // The imported user-defined enums serialize their authored entries in this order.
    constexpr int64 WalkGait = 0;
    constexpr int64 RunGait = 1;
    constexpr int64 SprintGait = 2;
    constexpr int32 LandingWindowAction = 0x434D4301;
    const FName JustLandedName(TEXT("JustLanded"));
    const FName GaitName(TEXT("Gait"));
    const FName StickModeName(TEXT("MovementStickMode"));
    const FName InputStateName(TEXT("CharacterInputState"));
    const FName SprintIntent(TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936"));
    const FName WalkIntent(TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3"));
    const FName StrafeIntent(TEXT("WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9"));
    const FName AimIntent(TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8"));
    const FName CrouchIntent(TEXT("WantsToCrouch_9_47D0791142F36BB93FFD9EA12F96F2B9"));

    struct FInputIntentField
    {
        FName Name;
        bool FSampleCmcInputIntent::* Member;
    };
    const FInputIntentField InputIntentFields[] = {
        {SprintIntent, &FSampleCmcInputIntent::bWantsToSprint},
        {WalkIntent, &FSampleCmcInputIntent::bWantsToWalk},
        {StrafeIntent, &FSampleCmcInputIntent::bWantsToStrafe},
        {AimIntent, &FSampleCmcInputIntent::bWantsToAim},
        {CrouchIntent, &FSampleCmcInputIntent::bWantsToCrouch},
    };

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
    , WalkAction(FSoftObjectPath(TEXT("/Game/Input/IA_Walk.IA_Walk")))
    , SprintAction(FSoftObjectPath(TEXT("/Game/Input/IA_Sprint.IA_Sprint")))
    , StrafeAction(FSoftObjectPath(TEXT("/Game/Input/IA_Strafe.IA_Strafe")))
    , AimAction(FSoftObjectPath(TEXT("/Game/Input/IA_Aim.IA_Aim")))
{
}

void ASampleCmcCharacter::InstallSandboxInputMapping()
{
    SamplePawnSetup::InstallInputMapping(*this);
}

void ASampleCmcCharacter::ActivateSandboxCamera()
{
    SamplePawnSetup::ActivateCamera(*this, TEXT("Camera(NotUsedByDefault)"));
}

void ASampleCmcCharacter::InitializeCmcRuntime()
{
    if (const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("AC_PreCMCTick")))
    {
        if (auto* PreMovement = Cast<USampleAnimationTickComponent>(Field->GetObjectPropertyValue_InContainer(this)))
        {
            GetCharacterMovement()->AddTickPrerequisiteComponent(PreMovement);
            PreMovement->BindTick(*this, GET_FUNCTION_NAME_CHECKED(ASampleCmcCharacter, UpdateBeforeCmcTick));
        }
    }
    if (auto* CharacterMesh = GetMesh())
    {
        CharacterMesh->AddTickPrerequisiteActor(this);
    }
    if (GetLocalRole() == ROLE_SimulatedProxy)
    {
        OnCharacterMovementUpdated.AddUniqueDynamic(this, &ASampleCmcCharacter::HandleCmcMovementUpdated);
    }
}

void ASampleCmcCharacter::UpdateBeforeCmcTick()
{
    UpdateRotation_PreCMC();
    UpdateMovementBeforeSimulation();
}

void ASampleCmcCharacter::HandleCmcMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
    UpdateSimulatedGroundTransition(OldVelocity);
}

void ASampleCmcCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    OnCharacterMovementUpdated.RemoveDynamic(this, &ASampleCmcCharacter::HandleCmcMovementUpdated);
    Super::EndPlay(EndPlayReason);
}

void ASampleCmcCharacter::UpdateSimulatedGroundTransition(FVector OldVelocity)
{
    const auto* Previous = FindFProperty<FBoolProperty>(GetClass(), TEXT("WasMovingOnGroundLastFrame_Simulated"));
    if (!Previous)
    {
        return;
    }
    const bool bOnGround = GetCharacterMovement()->IsMovingOnGround();
    if (bOnGround != Previous->GetPropertyValue_InContainer(this))
    {
        const FName Event = bOnGround ? TEXT("Custom OnLanded Event") : TEXT("Custom OnJumped Event");
        if (UFunction* Callback = FindFunction(Event); Callback && Callback->NumParms == 1)
        {
            FStructOnScope Arguments(Callback);
            uint8* Data = Arguments.GetStructMemory();
            if (bOnGround)
            {
                const auto* Velocity = FindFProperty<FStructProperty>(Callback, TEXT("LandVelocity"));
                if (Velocity && Velocity->Struct == TBaseStructure<FVector>::Get())
                {
                    *Velocity->ContainerPtrToValuePtr<FVector>(Data) = OldVelocity;
                    ProcessEvent(Callback, Data);
                }
            }
            else
            {
                const auto* Speed = FindFProperty<FNumericProperty>(Callback, TEXT("GroundSpeedBeforeJump"));
                if (Speed && Speed->IsFloatingPoint())
                {
                    Speed->SetFloatingPointPropertyValue(Speed->ContainerPtrToValuePtr<void>(Data), OldVelocity.Size2D());
                    ProcessEvent(Callback, Data);
                }
            }
        }
    }
    Previous->SetPropertyValue_InContainer(this, bOnGround);
}

void ASampleCmcCharacter::RecordLandingVelocity(FVector LandVelocity)
{
    const auto* Velocity = FindFProperty<FStructProperty>(GetClass(), TEXT("LandVelocity"));
    if (Velocity && Velocity->Struct == TBaseStructure<FVector>::Get())
    {
        *Velocity->ContainerPtrToValuePtr<FVector>(this) = LandVelocity;
    }
}

void ASampleCmcCharacter::StartLandingWindow()
{
    if (const auto* JustLanded = FindFProperty<FBoolProperty>(GetClass(), JustLandedName))
    {
        JustLanded->SetPropertyValue_InContainer(this, true);
        UKismetSystemLibrary::RetriggerableDelay(this, 0.3f, FLatentActionInfo(
            0, LandingWindowAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleCmcCharacter, ClearLandingWindow), this));
    }
}

void ASampleCmcCharacter::ClearLandingWindow()
{
    if (const auto* JustLanded = FindFProperty<FBoolProperty>(GetClass(), JustLandedName))
    {
        JustLanded->SetPropertyValue_InContainer(this, false);
    }
}

void ASampleCmcCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (bUseNativeCameraStyleKeys && PlayerInputComponent)
    {
        SamplePawnSetup::BindCameraStyleKeys(*this, *PlayerInputComponent);
    }
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
    if (bUseNativeCmcJumpInput)
    {
        if (const UInputAction* Jump = CmcJumpAction.LoadSynchronous())
        {
            Input->BindAction(Jump, ETriggerEvent::Started, this, &ASampleCmcCharacter::HandleCmcJumpStarted);
            Input->BindAction(Jump, ETriggerEvent::Triggered, this, &ASampleCmcCharacter::HandleCmcJumpTriggered);
        }
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
    if (bUseNativeInputIntentHandlers)
    {
        Bind(WalkAction.LoadSynchronous(), &ASampleCmcCharacter::HandleWalkIntent);
        Bind(SprintAction.LoadSynchronous(), &ASampleCmcCharacter::HandleSprintIntent);
        Bind(StrafeAction.LoadSynchronous(), &ASampleCmcCharacter::HandleStrafeIntent);
        Bind(AimAction.LoadSynchronous(), &ASampleCmcCharacter::HandleAimIntent);
    }
    if (auto* Interaction = FindComponentByClass<USampleSmartObjectInteractionComponent>())
    {
        Interaction->BindPlayerInteractionInput(*Input);
    }
}

bool ASampleCmcCharacter::CanStartCmcJump() const
{
    const USkeletalMeshComponent* SkeletalMesh = GetMesh();
    const UAnimInstance* Anim = SkeletalMesh ? SkeletalMesh->GetAnimInstance() : nullptr;
    return !Anim || !Anim->IsSlotActive(TEXT("DefaultSlot"));
}

void ASampleCmcCharacter::BeginCmcRagdoll()
{
    USkeletalMeshComponent* SkeletalMesh = GetMesh();
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!SkeletalMesh || !Capsule || !Movement)
    {
        return;
    }

    Movement->SetMovementMode(MOVE_None);
    SetCmcRagdollFlag(true);
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalMesh->SetCollisionObjectType(ECC_PhysicsBody);
    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SkeletalMesh->SetAllBodiesBelowSimulatePhysics(TEXT("pelvis"), true, true);
}

void ASampleCmcCharacter::EndCmcRagdoll()
{
    USkeletalMeshComponent* SkeletalMesh = GetMesh();
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!SkeletalMesh || !Capsule || !Movement)
    {
        return;
    }

    SetCmcRagdollFlag(false);
    Movement->SetMovementMode(MOVE_Falling);
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SkeletalMesh->SetCollisionObjectType(ECC_Pawn);
    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SkeletalMesh->SetAllBodiesSimulatePhysics(false);
}

void ASampleCmcCharacter::SetCmcRagdollFlag(bool bActive)
{
    if (const auto* Rag = FindFProperty<FBoolProperty>(GetClass(), TEXT("IsRagdolling")))
    {
        Rag->SetPropertyValue_InContainer(this, bActive);
    }
}

void ASampleCmcCharacter::OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal,
    const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
    Super::OnWalkingOffLedge_Implementation(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, TimeDelta);
    UnCrouch();
}

uint8 ASampleCmcCharacter::GetAuthoredGaitValue() const
{
    return static_cast<uint8>(ReadEnumValue(*this, GaitName, WalkGait));
}

uint8 ASampleCmcCharacter::GetAuthoredCameraModeValue() const
{
    if (ReadInputIntent(*this, AimIntent))
    {
        return 2;
    }
    return ReadInputIntent(*this, StrafeIntent) ? 1 : 0;
}

uint8 ASampleCmcCharacter::GetAuthoredCameraStyleValue() const
{
    return static_cast<uint8>(ReadEnumValue(*this, TEXT("CameraStyle"), 0));
}

FSampleTraversalApproach ASampleCmcCharacter::BuildTraversalApproach() const
{
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!Movement)
    {
        return {};
    }

    FSampleTraversalApproach Approach;
    Approach.Direction = GetActorForwardVector();
    Approach.Radius = 30.0f;
    if (Movement->MovementMode == MOVE_Falling || Movement->MovementMode == MOVE_Flying)
    {
        Approach.Distance = 75.0;
        Approach.EndOffset.Z = 50.0;
        Approach.HalfHeight = 86.0f;
    }
    else
    {
        const double LocalSpeed = GetActorQuat().UnrotateVector(Movement->Velocity).X;
        Approach.Distance = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 500.0), FVector2D(75.0, 350.0), LocalSpeed);
        Approach.HalfHeight = 60.0f;
    }
    return Approach;
}

bool ASampleCmcCharacter::TryCmcTraversal(bool bDrawForDuration)
{
    auto* Traversal = FindComponentByClass<USampleTraversalComponent>();
    return Traversal && GetCharacterMovement() && Traversal->TryAuthoredTraversal(BuildTraversalApproach(),
        bDrawForDuration ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::ForOneFrame);
}

void ASampleCmcCharacter::HandleCmcJumpStarted()
{
    if (CanStartCmcJump() && !TryCmcTraversal(true))
    {
        Jump();
    }
}

void ASampleCmcCharacter::HandleCmcJumpTriggered()
{
    if (CanStartCmcJump())
    {
        TryCmcTraversal(false);
    }
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

void ASampleCmcCharacter::HandleWalkIntent()
{
    FSampleCmcInputIntent Intent = ReadInputIntentSnapshot();
    if (!Intent.bWantsToSprint)
    {
        Intent.bWantsToWalk = !Intent.bWantsToWalk;
        CommitInputIntent(Intent);
    }
}

void ASampleCmcCharacter::HandleSprintIntent(const FInputActionValue& Value)
{
    FSampleCmcInputIntent Intent = ReadInputIntentSnapshot();
    Intent.bWantsToSprint = Value.Get<bool>();
    Intent.bWantsToWalk = false;
    CommitInputIntent(Intent);
}

void ASampleCmcCharacter::HandleStrafeIntent()
{
    FSampleCmcInputIntent Intent = ReadInputIntentSnapshot();
    Intent.bWantsToStrafe = !Intent.bWantsToStrafe;
    CommitInputIntent(Intent);
}

void ASampleCmcCharacter::HandleAimIntent(const FInputActionValue& Value)
{
    FSampleCmcInputIntent Intent = ReadInputIntentSnapshot();
    Intent.bWantsToAim = Value.Get<bool>();
    CommitInputIntent(Intent);
}

FSampleCmcInputIntent ASampleCmcCharacter::ReadInputIntentSnapshot() const
{
    FSampleCmcInputIntent Intent;
    for (const FInputIntentField& Field : InputIntentFields)
    {
        Intent.*Field.Member = ReadInputIntent(*this, Field.Name);
    }
    return Intent;
}

bool ASampleCmcCharacter::WriteInputIntentSnapshot(const FSampleCmcInputIntent& Intent)
{
    const FStructProperty* InputState = FindFProperty<FStructProperty>(GetClass(), InputStateName);
    if (!InputState)
    {
        return false;
    }
    const FBoolProperty* Fields[UE_ARRAY_COUNT(InputIntentFields)];
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(InputIntentFields); ++Index)
    {
        Fields[Index] = FindFProperty<FBoolProperty>(InputState->Struct, InputIntentFields[Index].Name);
        if (!Fields[Index])
        {
            return false;
        }
    }

    void* Data = InputState->ContainerPtrToValuePtr<void>(this);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(InputIntentFields); ++Index)
    {
        Fields[Index]->SetPropertyValue_InContainer(Data, Intent.*InputIntentFields[Index].Member);
    }
    if (HasAuthority())
    {
        MARK_PROPERTY_DIRTY(this, InputState);
    }
    return true;
}

void ASampleCmcCharacter::CommitInputIntent(const FSampleCmcInputIntent& Intent)
{
    if (WriteInputIntentSnapshot(Intent) && !HasAuthority())
    {
        ServerCommitInputIntent(Intent);
    }
}

void ASampleCmcCharacter::ApplyNpcWalkIntent(bool bWalk)
{
    FSampleCmcInputIntent Intent;
    Intent.bWantsToWalk = bWalk;
    WriteInputIntentSnapshot(Intent);
}

void ASampleCmcCharacter::ServerCommitInputIntent_Implementation(FSampleCmcInputIntent Intent)
{
    if (bUseNativeInputIntentHandlers)
    {
        WriteInputIntentSnapshot(Intent);
    }
}

int64 ASampleCmcCharacter::SelectGait() const
{
    static const FName ThresholdName(TEXT("AnalogWalk/RunThreshold"));
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
