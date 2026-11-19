#include "Characters/SampleMoverCharacter.h"
#include "Characters/SampleMoverCharacterInternal.h"

#include "Animation/AnimInstance.h"
#include "Animation/SampleAnimationTickComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataDrivenCVars/DataDrivenCVars.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DrawDebugLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoverDataModelTypes.h"
#include "PhysicsControlBPLibrary.h"
#include "PhysicsControlComponent.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/StructOnScope.h"
#include "UserDefinedStructSupport.h"
#include "Utilities/SampleLogChannels.h"
#include "Utilities/SamplePropertyAccess.h"

namespace
{
    constexpr int32 ConfirmRagdollEntryAction = 0x52414701;
    constexpr int32 ReleaseRagdollFacingAction = 0x52414702;
    constexpr int32 RestoreRagdollProfileAction = 0x52414703;
    constexpr int32 CheckRagdollRecoveryAction = 0x52414704;
    constexpr double SettledRagdollSpeed = 10.0;

    void SetLegCollision(USkeletalMeshComponent& Mesh, const TArray<FName>& Left,
        const TArray<FName>& Right, bool bEnabled)
    {
        if (Left.IsEmpty() || Right.IsEmpty())
        {
            return;
        }

        // Physics Control exposes these batched operations as reflected functions.
        auto* Library = GetMutableDefault<UPhysicsControlBPLibrary>();
        const FName Name = bEnabled
            ? GET_FUNCTION_NAME_CHECKED(UPhysicsControlBPLibrary, EnableCollisionBetweenBodyArrays)
            : GET_FUNCTION_NAME_CHECKED(UPhysicsControlBPLibrary, DisableCollisionBetweenBodyArrays);
        UFunction* Function = Library->FindFunction(Name);
        if (!Function)
        {
            return;
        }

        const auto* First = FindFProperty<FObjectPropertyBase>(Function, TEXT("FirstComponent"));
        const auto* Second = FindFProperty<FObjectPropertyBase>(Function, TEXT("SecondComponent"));
        const auto* FirstBones = FindFProperty<FArrayProperty>(Function, TEXT("FirstBoneNames"));
        const auto* SecondBones = FindFProperty<FArrayProperty>(Function, TEXT("SecondBoneNames"));
        if (!First || !Second || !FirstBones || !SecondBones ||
            !CastField<FNameProperty>(FirstBones->Inner) || !CastField<FNameProperty>(SecondBones->Inner))
        {
            return;
        }

        FStructOnScope Args(Function);
        uint8* Data = Args.GetStructMemory();
        First->SetObjectPropertyValue_InContainer(Data, &Mesh);
        Second->SetObjectPropertyValue_InContainer(Data, &Mesh);
        *FirstBones->ContainerPtrToValuePtr<TArray<FName>>(Data) = Left;
        *SecondBones->ContainerPtrToValuePtr<TArray<FName>>(Data) = Right;
        Library->ProcessEvent(Function, Data);
    }

    template <typename ValueType>
    ValueType ReadStructResult(ASampleMoverCharacter& Pawn, FName Name, ValueType Fallback)
    {
        UFunction* Function = Pawn.FindFunction(Name);
        const auto* Result = Function ? CastField<FStructProperty>(Function->GetReturnProperty()) : nullptr;
        if (!Result || Result->Struct != TBaseStructure<ValueType>::Get())
        {
            return Fallback;
        }
        FStructOnScope Args(Function);
        Pawn.ProcessEvent(Function, Args.GetStructMemory());
        return *Result->ContainerPtrToValuePtr<ValueType>(Args.GetStructMemory());
    }
}

void ASampleMoverCharacter::InitializeRagdollRuntime()
{
    InitializeMoverRuntime();
    if (auto* Variables = GEngine ? GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>() : nullptr)
    {
        Variables->OnDataDrivenCVarDelegate.AddUniqueDynamic(this, &ASampleMoverCharacter::HandleRagdollProfileVariable);
    }

    auto* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    auto* Physics = FindComponentByClass<UPhysicsControlComponent>();
    USampleAnimationTickComponent* PostAnimation = nullptr;
    if (const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("AC_PostABPTick")))
    {
        PostAnimation = Cast<USampleAnimationTickComponent>(Field->GetObjectPropertyValue_InContainer(this));
    }
    if (!PostAnimation)
    {
        PostAnimation = FindComponentByClass<USampleAnimationTickComponent>();
    }
    if (PostAnimation)
    {
        if (Mesh)
        {
            PostAnimation->AddTickPrerequisiteComponent(Mesh);
        }
        PostAnimation->BindTick(*this, TEXT("PostABPTick"));
        if (Physics)
        {
            Physics->AddTickPrerequisiteComponent(PostAnimation);
        }
    }
    if (Mesh && Physics)
    {
        Physics->CreateControlsAndBodyModifiersFromPhysicsControlAsset(Mesh, nullptr, NAME_None);
    }
    RefreshRagdollDefaultProfile();
}

void ASampleMoverCharacter::UpdateRagdollRuntime()
{
    UpdateMoverRuntime();
    if (const auto* Debug = FindFProperty<FIntProperty>(GetClass(), TEXT("DebugState")))
    {
        Debug->SetPropertyValue_InContainer(this,
            UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCVar.CharacterPhysics.DrawDebug")));
    }
    if (ResolveMovementModeValue() == static_cast<uint8>(SampleMoverInternal::EAuthoredMovementMode::Ragdoll))
    {
        UpdateRagdollBehavior();
    }
    TraceRagdollObstacles();
    UpdateCharacterInteractionCandidates();
}

bool ASampleMoverCharacter::TryHandleRagdollModeTransition(FName PreviousMovementModeName, FName NewMovementModeName)
{
    const bool bExitRagdoll = PreviousMovementModeName == SampleMoverInternal::RagdollMode;
    if (bExitRagdoll && !GetClass()->IsFunctionImplementedInScript(
        GET_FUNCTION_NAME_CHECKED(ASampleMoverCharacter, RunMoverRagdollExit)))
    {
        return false;
    }
    if (NewMovementModeName == SampleMoverInternal::RagdollMode)
    {
        ApplyRagdollCapsuleCollision();
    }
    if (bExitRagdoll)
    {
        RunMoverRagdollExit();
    }
    return true;
}

FMoverInputCmdContext ASampleMoverCharacter::BuildRagdollInputCommand(int32 SimTimeMs)
{
    FMoverInputCmdContext Command = BuildMoverInputCommand(SimTimeMs);
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    const auto* Snapshot = SamplePropertyAccess::FindStructProperty(*this, TEXT("RagdollInputs_PreSim"));
    if (!Mover || Mover->GetNextMovementModeName() != SampleMoverInternal::RagdollMode ||
        !Snapshot || !Snapshot->Struct->IsA<UUserDefinedStruct>())
    {
        return Command;
    }

    const auto* Transform = FindFProperty<FStructProperty>(Snapshot->Struct, SampleMoverInternal::RagdollTransformField);
    if (!Transform || Transform->Struct != TBaseStructure<FTransform>::Get())
    {
        return Command;
    }
    void* Data = Snapshot->ContainerPtrToValuePtr<void>(this);
    *Transform->ContainerPtrToValuePtr<FTransform>(Data) = ResolveRagdollTransform();

    auto* Ragdoll = static_cast<FMoverUserDefinedDataStruct*>(Command.InputCollection.FindOrAddDataByType(Snapshot->Struct));
    Ragdoll->InitializeAs(Snapshot->Struct, static_cast<const uint8*>(Data));
    if (auto* Defaults = Command.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>())
    {
        Defaults->OrientationIntent = ResolveRagdollFacing();
    }
    return Command;
}

FTransform ASampleMoverCharacter::ResolveRagdollTransform() const
{
    const USkeletalMeshComponent* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    if (!Mesh)
    {
        return GetActorTransform();
    }

    const FVector Pelvis = Mesh->GetBoneTransform(TEXT("pelvis"), RTS_World).GetLocation();
    const FTransform Spine = Mesh->GetBoneTransform(TEXT("spine_05"), RTS_World);
    FVector Heading = Spine.GetLocation() - Pelvis;
    if (Spine.GetUnitAxis(EAxis::Y).Z > 0.0)
    {
        Heading = -Heading;
    }
    return FTransform(FRotationMatrix::MakeFromZX(FVector::UpVector, Heading).ToQuat(), Pelvis);
}

FVector ASampleMoverCharacter::ResolveRagdollFacing() const
{
    if (SamplePropertyAccess::ReadBool(*this, TEXT("Ragdoll_FreezeCapsuleRotation")))
    {
        return GetActorForwardVector();
    }
    if (SamplePropertyAccess::ReadBool(*this, TEXT("Ragdoll_PlayRollingGetups")))
    {
        return SamplePropertyAccess::ReadVector(*this, TEXT("Ragdoll_COM_Velocity")).GetSafeNormal2D();
    }
    const auto* Transform = SampleMoverInternal::FindRagdollTransform(
        SamplePropertyAccess::FindStructProperty(*this, TEXT("RagdollInputs_PreSim")), *this);
    return Transform ? Transform->GetUnitAxis(EAxis::X) : GetActorForwardVector();
}

void ASampleMoverCharacter::UpdateRagdollVelocities()
{
    const USkeletalMeshComponent* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    if (!Mesh)
    {
        return;
    }
    const FVector Center = Mesh->GetSkeletalCenterOfMass();
    const double DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0;
    const FVector PreviousCenter = SamplePropertyAccess::ReadVector(*this, TEXT("Ragdoll_COM_LastFrame"));
    SamplePropertyAccess::WriteVector(*this, TEXT("Ragdoll_COM_Velocity"),
        DeltaSeconds > SMALL_NUMBER ? (Center - PreviousCenter) / DeltaSeconds : FVector::ZeroVector);
    SamplePropertyAccess::WriteVector(*this, TEXT("Ragdoll_COM_LastFrame"), Center);
    const FVector SpineVelocity = Mesh->GetPhysicsLinearVelocity(TEXT("spine_05"));
    SamplePropertyAccess::WriteVector(*this, TEXT("Ragdoll_SpineVelocity"), SpineVelocity);
    SamplePropertyAccess::WriteNumber(GetClass(), this, TEXT("Ragdoll_Speed"), SpineVelocity.Size());
}

void ASampleMoverCharacter::UpdateRagdollGravity()
{
    if (auto* Physics = FindComponentByClass<UPhysicsControlComponent>())
    {
        constexpr double GravityCutoffSpeed = -2500.0;
        const double Multiplier = SamplePropertyAccess::ReadVector(*this, TEXT("Ragdoll_COM_Velocity")).Z <= GravityCutoffSpeed
            ? 0.0 : SamplePropertyAccess::ReadNumber(GetClass(), this, TEXT("Ragdoll_GravityMultiplier"));
        Physics->SetBodyModifierGravityMultiplier(TEXT("All"), static_cast<float>(Multiplier), true, true);
    }
}

void ASampleMoverCharacter::UpdateRagdollBehavior()
{
    UpdateRagdollVelocities();
    UpdatePredictedRagdollImpact();
    UpdateRagdollRollForce();
    UpdateRagdollGravity();
    if (!IsPlayerControlled() && FMath::IsNearlyZero(SamplePropertyAccess::ReadNumber(GetClass(), this, TEXT("Ragdoll_Speed")), SettledRagdollSpeed))
    {
        ScheduleRagdollRecoveryCheck();
    }
}

void ASampleMoverCharacter::UpdatePredictedRagdollImpact()
{
    const auto* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    const auto* DirectionProperty = SamplePropertyAccess::FindStructProperty(*this, TEXT("Ragdoll_ImpactDirection"));
    if (!Mesh || !DirectionProperty || DirectionProperty->Struct != TBaseStructure<FVector2D>::Get())
    {
        return;
    }

    const FTransform Socket = Mesh->GetSocketTransform(TEXT("RagdollTrace"), RTS_World);
    FPredictProjectilePathParams Params;
    Params.StartLocation = Socket.GetLocation();
    Params.LaunchVelocity = SamplePropertyAccess::ReadVector(*this, TEXT("Ragdoll_SpineVelocity"));
    Params.bTraceWithCollision = true;
    Params.bTraceWithChannel = true;
    Params.TraceChannel = ECC_PhysicsBody;
    Params.ProjectileRadius = 20.0f;
    Params.MaxSimTime = 1.0f;
    Params.SimFrequency = 10.0f;
    Params.ActorsToIgnore.Add(this);
    if (const auto* Debug = FindFProperty<FIntProperty>(GetClass(), TEXT("DebugState"));
        Debug && Debug->GetPropertyValue_InContainer(this) == 1)
    {
        Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
    }

    FPredictProjectilePathResult Prediction;
    UGameplayStatics::PredictProjectilePath(this, Params, Prediction);
    const auto& Destination = Prediction.LastTraceDestination;
    const FVector Direction = (Destination.Location - Socket.GetLocation()).GetSafeNormal(0.0001);
    const FVector Relative = Socket.InverseTransformVectorNoScale(Direction);
    const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Relative.Y, Relative.X));
    const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Relative.Z, Relative.Size2D()));
    SamplePropertyAccess::WriteNumber(GetClass(), this, TEXT("Ragdoll_TimeToImpact"), Destination.Time);
    *DirectionProperty->ContainerPtrToValuePtr<FVector2D>(this) = FVector2D(Yaw, Pitch);
}

void ASampleMoverCharacter::UpdateRagdollRollForce()
{
    if (!GetWorld())
    {
        return;
    }
    const FVector FloorNormal = SamplePropertyAccess::ReadVector(*this, TEXT("FloorNormal"));
    const double Cosine = FMath::Clamp(FVector::DotProduct(FloorNormal, FVector::UpVector), -1.0, 1.0);
    const double Slope = FMath::RadiansToDegrees(FMath::Acos(Cosine));
    const FVector Target = FloorNormal * FMath::GetMappedRangeValueClamped(
        FVector2D(20.0, 30.0), FVector2D(0.0, 1.0), Slope);
    const FVector Current = SamplePropertyAccess::ReadVector(*this, TEXT("Ragdoll_AutoRollForce"));
    const float Speed = Target.SizeSquared() < Current.SizeSquared() ? 0.5f : 2.0f;
    const FVector Automatic = FMath::VInterpConstantTo(Current, Target, GetWorld()->GetDeltaSeconds(), Speed);
    SamplePropertyAccess::WriteVector(*this, TEXT("Ragdoll_AutoRollForce"), Automatic);
    const FVector MoveInput = ReadStructResult(*this, TEXT("Get_MoveInput"), FVector::ZeroVector);
    const FVector Roll = (MoveInput + Automatic).GetClampedToMaxSize(1.0);
    SamplePropertyAccess::WriteVector(*this, TEXT("Ragdoll_RollForce"), Roll);
    if (auto* Mesh = SampleMoverInternal::GetRagdollMesh(*this); Mesh && Mesh->IsAnySimulatingPhysics())
    {
        Mesh->AddTorqueInDegrees(FVector::CrossProduct(FVector::UpVector, Roll) * 100000.0, TEXT("Spine_05"), true);
        const auto* Debug = FindFProperty<FIntProperty>(GetClass(), TEXT("DebugState"));
        if (Debug && Debug->GetPropertyValue_InContainer(this) > 0)
        {
            UDrawDebugLibrary::DrawDebugOrientedArrow(UDrawDebugLibrary::MakeDebugDrawer(this), GetActorLocation(),
                Roll.Rotation(), Roll.Size() * 100.0, FDrawDebugLineStyle(), false);
        }
    }
}

void ASampleMoverCharacter::ScheduleRagdollRecoveryCheck()
{
    if (GetWorld())
    {
        UKismetSystemLibrary::Delay(this, 3.0f, FLatentActionInfo(
            0, CheckRagdollRecoveryAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, CompleteRagdollRecoveryCheck), this));
    }
}

void ASampleMoverCharacter::CompleteRagdollRecoveryCheck()
{
    if (ResolveMovementModeValue() == static_cast<uint8>(SampleMoverInternal::EAuthoredMovementMode::Ragdoll) &&
        FMath::IsNearlyZero(SamplePropertyAccess::ReadNumber(GetClass(), this, TEXT("Ragdoll_Speed")), SettledRagdollSpeed))
    {
        RequestRagdollRecovery();
    }
}

void ASampleMoverCharacter::ApplySandboxZeroGravity(bool bEnabled)
{
    const auto* Gravity = FindFProperty<FNumericProperty>(GetClass(), TEXT("Ragdoll_GravityMultiplier"));
    auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (Mover && Gravity && Gravity->IsFloatingPoint())
    {
        Mover->SetGravityOverride(bEnabled, FVector::ZeroVector);
        Gravity->SetFloatingPointPropertyValue(Gravity->ContainerPtrToValuePtr<void>(this), bEnabled ? 0.0 : 1.0);
    }
}

bool ASampleMoverCharacter::CanStartRagdollCharacterInteraction() const
{
    const USkeletalMeshComponent* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    return CanReceiveRagdollCharacterInteraction() && (!Anim || !Anim->IsSlotActive(TEXT("DefaultSlot"))) &&
        !SamplePropertyAccess::ReadStructBool(
            SamplePropertyAccess::FindStructProperty(*this, SampleMoverInternal::MoverPlayerInputState),
            *this, SampleMoverInternal::MoverAimIntent);
}

bool ASampleMoverCharacter::CanReceiveRagdollCharacterInteraction() const
{
    return ResolveMovementModeValue() == static_cast<uint8>(SampleMoverInternal::EAuthoredMovementMode::Grounded);
}

void ASampleMoverCharacter::UpdateCharacterInteractionCandidates()
{
    const auto* Property = FindFProperty<FArrayProperty>(GetClass(), TEXT("InteractableActors"));
    const auto* Item = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!Item)
    {
        return;
    }

    FScriptArrayHelper Candidates(Property, Property->ContainerPtrToValuePtr<void>(this));
    Candidates.EmptyValues();
    if (!CanStartRagdollCharacterInteraction())
    {
        return;
    }

    constexpr float SearchRadius = 200.0f;
    constexpr double HeightTolerance = 50.0;
    constexpr double HalfArcDegrees = 60.0;
    constexpr float ProbeRadius = 20.0f;
    constexpr float ProbeHalfHeight = 60.0f;
    const FTransform Transform = GetActorTransform();
    const FVector Origin = Transform.GetLocation();
    TArray<AActor*> Nearby;
    const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes{ObjectTypeQuery3};
    UKismetSystemLibrary::SphereOverlapActors(this, Origin, SearchRadius, ObjectTypes,
        Property->GetOwnerClass(), {this}, Nearby);

    for (AActor* Actor : Nearby)
    {
        const auto* Target = Cast<ASampleMoverCharacter>(Actor);
        if (!IsValid(Target) || !Target->CanReceiveRagdollCharacterInteraction())
        {
            continue;
        }

        const FVector Destination = Target->GetActorLocation();
        const FRotator RelativeFacing = UKismetMathLibrary::FindRelativeLookAtRotation(Transform, Destination);
        if (!FMath::IsNearlyEqual(Origin.Z, Destination.Z, HeightTolerance) ||
            FMath::Abs(RelativeFacing.Yaw) > HalfArcDegrees)
        {
            continue;
        }

        FHitResult Hit;
        if (UKismetSystemLibrary::CapsuleTraceSingleByProfile(this, Origin, Destination, ProbeRadius,
            ProbeHalfHeight, SampleMoverInternal::CapsuleProfile, false, {Actor}, EDrawDebugTrace::None, Hit, true))
        {
            continue;
        }

        Item->SetObjectPropertyValue(Candidates.GetRawPtr(Candidates.AddValue()), Actor);
        if (IsPlayerControlled())
        {
            UKismetSystemLibrary::DrawDebugSphere(this, Destination + FVector(0.0, 0.0, 20.0), 1.0f,
                25, FLinearColor::White, 0.0f, 0.5f, EDrawDebugSceneDepthPriorityGroup::Foreground);
        }
    }
}

void ASampleMoverCharacter::HandleMoverShove()
{
    HandleMoverInteractionInput(TEXT("Shove"), TEXT("Demo_Spawn/RotateCharacter"));
}

void ASampleMoverCharacter::HandleMoverTakedown()
{
    HandleMoverInteractionInput(TEXT("Takedown"), TEXT("Demo_SimpleShoot"));
}

void ASampleMoverCharacter::SpawnOrRotateRagdollDemoCharacter(TSubclassOf<ASampleMoverCharacter> DemoCharacterClass)
{
    UWorld* World = GetWorld();
    const APlayerController* ViewController = UGameplayStatics::GetPlayerController(this, 0);
    if (!World || !ViewController || !DemoCharacterClass)
    {
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    ViewController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    constexpr double TraceDistance = 1000.0;
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;
    FHitResult Hit;
    UKismetSystemLibrary::LineTraceSingleForObjects(this, ViewLocation, TraceEnd, {ObjectTypeQuery1}, false, {},
        EDrawDebugTrace::None, Hit, true);

    if (APawn* TargetPawn = Cast<APawn>(Hit.GetActor()))
    {
        if (auto* Target = Cast<ASampleMoverCharacter>(TargetPawn))
        {
            Target->CustomTeleport(FTransform(FRotator(0.0, 90.0, 0.0)), true, true);
        }
        return;
    }

    constexpr double SpawnHeight = 90.0;
    const FVector Location = (Hit.bBlockingHit ? Hit.ImpactPoint : TraceEnd) + FVector(0.0, 0.0, SpawnHeight);
    const FTransform SpawnTransform(FRotator(0.0, ViewRotation.Yaw + 180.0, 0.0), Location);
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    Parameters.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;
    RagdollDemoCharacters.Add(World->SpawnActor<ASampleMoverCharacter>(DemoCharacterClass, SpawnTransform, Parameters));
    constexpr int32 MaxTargets = 3;
    if (RagdollDemoCharacters.Num() > MaxTargets)
    {
        if (ASampleMoverCharacter* Oldest = RagdollDemoCharacters[0].Get())
        {
            Oldest->Destroy();
        }
        RagdollDemoCharacters.RemoveAt(0);
    }
}

void ASampleMoverCharacter::ResolveRagdollDemoAim(FVector& Origin, FVector& Direction) const
{
    const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("GameplayCamera"));
    const auto* Camera = Field ? Cast<USceneComponent>(Field->GetObjectPropertyValue_InContainer(this)) : nullptr;
    const USceneComponent* View = Camera ? Camera->GetChildComponent(0) : nullptr;
    Origin = View ? View->GetComponentLocation() : FVector::ZeroVector;
    Direction = View ? View->GetForwardVector() : FVector::ZeroVector;
}

void ASampleMoverCharacter::DrawRagdollDemoCrosshair() const
{
    if (!SamplePropertyAccess::ReadStructBool(
        SamplePropertyAccess::FindStructProperty(*this, SampleMoverInternal::MoverPlayerInputState),
        *this, SampleMoverInternal::MoverAimIntent))
    {
        return;
    }

    FVector Origin;
    FVector Direction;
    ResolveRagdollDemoAim(Origin, Direction);
    UKismetSystemLibrary::DrawDebugSphere(this, Origin + Direction * 1000.0, 3.0f,
        50, FLinearColor::White, 0.0f, 1.0f, EDrawDebugSceneDepthPriorityGroup::Foreground);
}

void ASampleMoverCharacter::ApplyRagdollDemoHit()
{
    FVector Origin;
    FVector Direction;
    ResolveRagdollDemoAim(Origin, Direction);
    if (Direction.IsZero())
    {
        return;
    }
    constexpr double TraceDistance = 100000.0;
    FHitResult Hit;
    if (!UKismetSystemLibrary::LineTraceSingleForObjects(this, Origin, Origin + Direction * TraceDistance,
        {ObjectTypeQuery4}, false, {}, EDrawDebugTrace::None, Hit, true))
    {
        return;
    }

    auto* Target = Cast<ASampleMoverCharacter>(Hit.GetActor());
    UFunction* Trigger = Target ? Target->FindFunction(TEXT("TriggerRagdoll")) : nullptr;
    const auto* Stop = Trigger ? FindFProperty<FBoolProperty>(Trigger, TEXT("StopActiveMontages")) : nullptr;
    const FName InjuryName(TEXT("Ragdoll_InjuryState"));
    const auto* InjuryByte = Trigger ? FindFProperty<FByteProperty>(Trigger, InjuryName) : nullptr;
    const auto* InjuryEnum = Trigger ? FindFProperty<FEnumProperty>(Trigger, InjuryName) : nullptr;
    const UEnum* InjuryType = InjuryByte ? InjuryByte->Enum.Get() : (InjuryEnum ? InjuryEnum->GetEnum() : nullptr);
    const int64 InjuryValue = InjuryType ? InjuryType->GetValueByNameString(TEXT("NewEnumerator5")) : INDEX_NONE;
    if (!Stop || InjuryValue == INDEX_NONE)
    {
        return;
    }

    FStructOnScope Args(Trigger);
    void* Data = Args.GetStructMemory();
    Stop->SetPropertyValue_InContainer(Data, true);
    SamplePropertyAccess::WriteEnumByte(Trigger, Data, InjuryName, static_cast<uint8>(InjuryValue));
    Target->ProcessEvent(Trigger, Data);
    if (auto* Mesh = SampleMoverInternal::GetRagdollMesh(*Target))
    {
        constexpr double ImpulseSpeed = 2500.0;
        Mesh->AddImpulse(Direction * ImpulseSpeed, Hit.BoneName, true);
    }
}

void ASampleMoverCharacter::HandleMoverInteractionInput(FName InteractionType, FName AimingDemoFunction)
{
    if (SamplePropertyAccess::ReadStructBool(
        SamplePropertyAccess::FindStructProperty(*this, SampleMoverInternal::MoverPlayerInputState),
        *this, SampleMoverInternal::MoverAimIntent))
    {
        if (UFunction* Demo = FindFunction(AimingDemoFunction); Demo && Demo->NumParms == 0)
        {
            ProcessEvent(Demo, nullptr);
        }
    }
    else if (CanStartRagdollCharacterInteraction())
    {
        RequestCharacterInteraction(InteractionType);
    }
}

void ASampleMoverCharacter::RequestCharacterInteraction(FName InteractionType)
{
    const auto* Candidates = FindFProperty<FArrayProperty>(GetClass(), TEXT("InteractableActors"));
    if (!Candidates)
    {
        return;
    }
    const void* CandidateValues = Candidates->ContainerPtrToValuePtr<void>(this);
    if (FScriptArrayHelper(Candidates, CandidateValues).Num() == 0)
    {
        return;
    }

    UFunction* SelectAnimation = FindFunction(TEXT("Try_MultiCharacterInteraction"));
    const auto* Type = SelectAnimation ? FindFProperty<FNameProperty>(SelectAnimation, TEXT("InteractionType")) : nullptr;
    const auto* Actors = SelectAnimation ? FindFProperty<FArrayProperty>(SelectAnimation, TEXT("Other Actors")) : nullptr;
    if (!Type || !Actors || SelectAnimation->NumParms != 2 || !Actors->SameType(Candidates))
    {
        return;
    }
    FStructOnScope Args(SelectAnimation);
    void* Data = Args.GetStructMemory();
    Type->SetPropertyValue_InContainer(Data, InteractionType);
    Actors->CopyCompleteValue(Actors->ContainerPtrToValuePtr<void>(Data), CandidateValues);
    ProcessEvent(SelectAnimation, Data);
}

void ASampleMoverCharacter::BeginPairedInteractionCollision(UPrimitiveComponent* PartnerCapsule)
{
    auto* Partner = PartnerCapsule ? Cast<ASampleMoverCharacter>(PartnerCapsule->GetOwner()) : nullptr;
    auto* Capsule = SampleMoverInternal::GetRagdollCapsule(*this);
    if (!IsValid(Partner) || Partner == this || !Capsule ||
        PairedInteractions.ContainsByPredicate([Partner](const TSharedPtr<FPairedInteraction>& Pair)
        {
            return Pair->Initiator.Get() == Partner || Pair->Partner.Get() == Partner;
        }))
    {
        return;
    }

    const auto Pair = MakeShared<FPairedInteraction>();
    Pair->Initiator = this;
    Pair->Partner = Partner;
    Pair->InitiatorCapsule = Capsule;
    Pair->PartnerCapsule = PartnerCapsule;
    Pair->bAddedInitiatorIgnore = !Capsule->GetMoveIgnoreComponents().Contains(PartnerCapsule);
    Pair->bAddedPartnerIgnore = !PartnerCapsule->GetMoveIgnoreComponents().Contains(Capsule);
    if (Pair->bAddedInitiatorIgnore)
    {
        Capsule->IgnoreComponentWhenMoving(PartnerCapsule, true);
    }
    if (Pair->bAddedPartnerIgnore)
    {
        PartnerCapsule->IgnoreComponentWhenMoving(Capsule, true);
    }

    auto* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    auto* PartnerMesh = SampleMoverInternal::GetRagdollMesh(*Partner);
    Pair->InitiatorMesh = Mesh;
    Pair->PartnerMesh = PartnerMesh;
    if (Mesh && PartnerMesh &&
        !Mesh->PrimaryComponentTick.GetPrerequisites().Contains(FTickPrerequisite(PartnerMesh, PartnerMesh->PrimaryComponentTick)))
    {
        Mesh->AddTickPrerequisiteComponent(PartnerMesh);
        Pair->bAddedTickPrerequisite = true;
    }
    PairedInteractions.Add(Pair);
    Partner->PairedInteractions.Add(Pair);
}

void ASampleMoverCharacter::EndPairedInteractionCollision()
{
    const auto EndingPairs = MoveTemp(PairedInteractions);
    PairedInteractions.Reset();
    for (const auto& Pair : EndingPairs)
    {
        auto* Capsule = Pair->InitiatorCapsule.Get();
        auto* PartnerCapsule = Pair->PartnerCapsule.Get();
        if (Capsule && PartnerCapsule)
        {
            if (Pair->bAddedInitiatorIgnore)
            {
                Capsule->IgnoreComponentWhenMoving(PartnerCapsule, false);
            }
            if (Pair->bAddedPartnerIgnore)
            {
                PartnerCapsule->IgnoreComponentWhenMoving(Capsule, false);
            }
        }
        if (auto* Mesh = Pair->InitiatorMesh.Get(); Mesh && Pair->bAddedTickPrerequisite)
        {
            if (auto* PartnerMesh = Pair->PartnerMesh.Get())
            {
                Mesh->RemoveTickPrerequisiteComponent(PartnerMesh);
            }
        }
        ASampleMoverCharacter* Peer = Pair->Initiator.Get() == this ? Pair->Partner.Get() : Pair->Initiator.Get();
        if (Peer)
        {
            Peer->PairedInteractions.RemoveSingleSwap(Pair);
        }
    }
}

void ASampleMoverCharacter::BeginRagdollPhysics()
{
    auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    USkeletalMeshComponent* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    if (!Mover || !Mesh || !GetWorld())
    {
        return;
    }

    // Impulses sent by the caller must see simulated bodies before Physics Control ticks.
    Mesh->SetSimulatePhysics(true);
    ApplyRagdollProfile(SampleMoverInternal::RagdollMode);
    Mover->QueueNextMode(SampleMoverInternal::RagdollMode);
    UKismetSystemLibrary::DelayUntilNextFrame(this, FLatentActionInfo(
        0, ConfirmRagdollEntryAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, ConfirmRagdollEntry), this));

    SamplePropertyAccess::WriteBool(*this, TEXT("Ragdoll_FreezeCapsuleRotation"), true);
    UKismetSystemLibrary::RetriggerableDelay(this, 0.25f, FLatentActionInfo(
        0, ReleaseRagdollFacingAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, ReleaseRagdollFacing), this));
}

bool ASampleMoverCharacter::TryTriggerRagdoll(bool bStopActiveMontages, const FMontageBlendSettings& MontageBlendOut,
    uint8 InjuryState, UAnimMontage* MontageToStop)
{
    const FName InjuryName(TEXT("Ragdoll_InjuryState"));
    const auto* InjuryByte = FindFProperty<FByteProperty>(GetClass(), InjuryName);
    const auto* InjuryEnum = FindFProperty<FEnumProperty>(GetClass(), InjuryName);
    const UEnum* InjuryType = InjuryByte ? InjuryByte->Enum.Get() : (InjuryEnum ? InjuryEnum->GetEnum() : nullptr);
    const auto* Trigger = FindFunction(TEXT("TriggerRagdoll"));
    const auto* Parameter = Trigger ? FindFProperty<FProperty>(Trigger, InjuryName) : nullptr;
    const FProperty* InjuryField = InjuryByte ? static_cast<const FProperty*>(InjuryByte) : InjuryEnum;
    if (!InjuryType || !InjuryType->IsValidEnumValue(InjuryState) || !Parameter || !InjuryField->SameType(Parameter))
    {
        return false;
    }

    if (bStopActiveMontages)
    {
        if (auto* Mesh = SampleMoverInternal::GetRagdollMesh(*this))
        {
            if (UAnimInstance* Anim = Mesh->GetAnimInstance())
            {
                Anim->Montage_StopWithBlendSettings(MontageBlendOut, MontageToStop);
            }
        }
    }

    SamplePropertyAccess::WriteEnumByte(GetClass(), this, InjuryName, InjuryState);
    BeginRagdollPhysics();
    return true;
}

void ASampleMoverCharacter::RequestRagdollRecovery()
{
    if (auto* Mover = FindComponentByClass<UCharacterMoverComponent>())
    {
        Mover->QueueNextMode(TEXT("Walking"));
    }
}

void ASampleMoverCharacter::ApplyRagdollCapsuleCollision()
{
    if (UCapsuleComponent* Capsule = SampleMoverInternal::GetRagdollCapsule(*this))
    {
        Capsule->SetCollisionProfileName(SampleMoverInternal::CapsuleProfile);
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    }
}

void ASampleMoverCharacter::RestoreRagdollCapsuleCollision()
{
    if (UCapsuleComponent* Capsule = SampleMoverInternal::GetRagdollCapsule(*this))
    {
        Capsule->SetCollisionProfileName(SampleMoverInternal::CapsuleProfile);
    }
}

void ASampleMoverCharacter::FinishRagdollPhysicsRecovery()
{
    if (auto* Physics = FindComponentByClass<UPhysicsControlComponent>())
    {
        FPhysicsControlData Recovery;
        Recovery.LinearTargetVelocityMultiplier = 0.f;
        Recovery.AngularTargetVelocityMultiplier = 0.f;
        Physics->SetControlDatasInSet(TEXT("ParentSpace"), Recovery);
    }
    if (GetWorld())
    {
        UKismetSystemLibrary::DelayUntilNextFrame(this, FLatentActionInfo(
            0, RestoreRagdollProfileAction, GET_FUNCTION_NAME_STRING_CHECKED(ASampleMoverCharacter, RestoreRagdollDefaultProfile), this));
    }
}

void ASampleMoverCharacter::RefreshRagdollDefaultProfile()
{
    const auto* Default = FindFProperty<FNameProperty>(GetClass(), SampleMoverInternal::DefaultRagdollProfile);
    if (!Default)
    {
        return;
    }
    const int32 Choice = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCVar.CharacterPhysics.DefaultProfile"));
    const FName Profile = Choice == 1
        ? SampleMoverInternal::PhysicalAnimationProfile
        : SampleMoverInternal::KinematicRagdollProfile;
    Default->SetPropertyValue_InContainer(this, Profile);
    if (ResolveMovementModeValue() != static_cast<uint8>(SampleMoverInternal::EAuthoredMovementMode::Ragdoll))
    {
        ApplyRagdollProfile(Profile);
    }
}

void ASampleMoverCharacter::ApplyRagdollProfile(FName PhysicsProfile)
{
    if (const auto* Applied = FindFProperty<FNameProperty>(GetClass(), TEXT("AppliedPhysicsProfile")))
    {
        Applied->SetPropertyValue_InContainer(this, PhysicsProfile);
    }
    if (auto* Physics = FindComponentByClass<UPhysicsControlComponent>())
    {
        Physics->InvokeControlProfile(PhysicsProfile);
    }
    if (USkeletalMeshComponent* Mesh = SampleMoverInternal::GetRagdollMesh(*this))
    {
        if (PhysicsProfile == SampleMoverInternal::RagdollMode)
        {
            SetLegCollision(*Mesh, LeftLegPhysicsBodies, RightLegPhysicsBodies, true);
            Mesh->SetConstraintProfileForAll(NAME_None, false);
        }
        else if (PhysicsProfile == SampleMoverInternal::KinematicRagdollProfile ||
                 PhysicsProfile == SampleMoverInternal::PhysicalAnimationProfile)
        {
            SetLegCollision(*Mesh, LeftLegPhysicsBodies, RightLegPhysicsBodies, false);
            Mesh->SetConstraintProfileForAll(TEXT("Free"), false);
        }
    }
}

void ASampleMoverCharacter::TraceRagdollObstacles()
{
    const USkeletalMeshComponent* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    if (!Mesh || !GetWorld())
    {
        return;
    }
    const FVector Neck = Mesh->GetSocketLocation(TEXT("neck_01"));
    const FVector Feet = (Mesh->GetSocketLocation(TEXT("foot_l")) + Mesh->GetSocketLocation(TEXT("foot_r"))) * 0.5;
    const auto* Debug = FindFProperty<FIntProperty>(GetClass(), TEXT("DebugState"));
    const auto Draw = Debug && Debug->GetPropertyValue_InContainer(this) == 1
        ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;
    TArray<FHitResult> Hits;
    if (UKismetSystemLibrary::SphereTraceMultiForObjects(this, Neck, Feet, 15.f,
        {EObjectTypeQuery::ObjectTypeQuery7}, false, {}, Draw, Hits, true))
    {
        HandleMoverRagdollInput();
    }
}

void ASampleMoverCharacter::HandleMoverRagdollInput()
{
    UFunction* Trigger = FindFunction(TEXT("TriggerRagdoll"));
    const auto* Stop = Trigger ? FindFProperty<FBoolProperty>(Trigger, TEXT("StopActiveMontages")) : nullptr;
    const auto* Blend = Trigger ? FindFProperty<FStructProperty>(Trigger, TEXT("MontageBlendOut")) : nullptr;
    const auto* DefaultBlend = SamplePropertyAccess::FindStructProperty(*this, TEXT("DefaultRagdollMontageBlendOut"));
    if (Stop && Blend && DefaultBlend && Blend->SameType(DefaultBlend))
    {
        FStructOnScope Args(Trigger);
        uint8* Data = Args.GetStructMemory();
        Stop->SetPropertyValue_InContainer(Data, true);
        Blend->CopyCompleteValue(Blend->ContainerPtrToValuePtr<void>(Data), DefaultBlend->ContainerPtrToValuePtr<void>(this));
        ProcessEvent(Trigger, Data);
    }
}

void ASampleMoverCharacter::HandleRagdollProfileVariable(FString CVarName)
{
    if (CVarName == TEXT("DDCVar.CharacterPhysics.DefaultProfile"))
    {
        RefreshRagdollDefaultProfile();
    }
}

void ASampleMoverCharacter::HandleRagdollContact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    const auto* Mesh = SampleMoverInternal::GetRagdollMesh(*this);
    if (OtherActor == this || !Mesh || NormalImpulse.SizeSquared() <= FMath::Square(100.0))
    {
        return;
    }
    const double BoneSpeed = Mesh->GetPhysicsLinearVelocity(Hit.BoneName).Size();
    if (BoneSpeed <= 50.0)
    {
        return;
    }

    UFunction* Foley = FindFunction(TEXT("PlayRagdollSound"));
    const auto* Volume = Foley ? FindFProperty<FNumericProperty>(Foley, TEXT("Volume")) : nullptr;
    if (Volume && Foley->NumParms == 1 && Volume->IsFloatingPoint())
    {
        FStructOnScope Args(Foley);
        uint8* Data = Args.GetStructMemory();
        const double Gain = FMath::GetMappedRangeValueClamped(FVector2D(50.0, 500.0), FVector2D(0.3, 1.0), BoneSpeed);
        Volume->SetFloatingPointPropertyValue(Volume->ContainerPtrToValuePtr<void>(Data), Gain);
        ProcessEvent(Foley, Data);
    }
}

void ASampleMoverCharacter::ConfirmRagdollEntry()
{
    if (ResolveMovementModeValue() != static_cast<uint8>(SampleMoverInternal::EAuthoredMovementMode::Ragdoll))
    {
        if (const auto* Default = FindFProperty<FNameProperty>(GetClass(), SampleMoverInternal::DefaultRagdollProfile))
        {
            ApplyRagdollProfile(Default->GetPropertyValue_InContainer(this));
        }
        ReleaseRagdollFacing();
    }
}

void ASampleMoverCharacter::ReleaseRagdollFacing()
{
    SamplePropertyAccess::WriteBool(*this, TEXT("Ragdoll_FreezeCapsuleRotation"), false);
}

void ASampleMoverCharacter::RestoreRagdollDefaultProfile()
{
    const auto* Mover = FindComponentByClass<UCharacterMoverComponent>();
    if (Mover && Mover->GetNextMovementModeName() != SampleMoverInternal::RagdollMode)
    {
        if (const auto* Default = FindFProperty<FNameProperty>(GetClass(), SampleMoverInternal::DefaultRagdollProfile))
        {
            ApplyRagdollProfile(Default->GetPropertyValue_InContainer(this));
        }
    }
}
