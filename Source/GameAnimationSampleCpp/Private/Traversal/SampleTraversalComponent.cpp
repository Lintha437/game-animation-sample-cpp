#include "Traversal/SampleTraversalComponent.h"

#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Math/RotationMatrix.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    const FName TraversingMode(TEXT("Traversing"));
    const FName WalkingMode(TEXT("Walking"));
    const FName MoverFlag(TEXT("IsMoverCharacter"));
    constexpr float CorrectionGracePeriod = 0.2f;
    constexpr float MaxRemoteExitWait = 2.0f;
    const FName FrontLedgeTarget(TEXT("FrontLedge"));
    const FName BackLedgeTarget(TEXT("BackLedge"));
    const FName BackFloorTarget(TEXT("BackFloor"));

    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    UFunction* FindTraversalPlayback(USampleTraversalComponent& Component)
    {
        const auto* IsMover = FindFProperty<FBoolProperty>(Component.GetClass(), MoverFlag);
        if (!IsMover)
        {
            return nullptr;
        }
        UFunction* Play = Component.FindFunction(IsMover->GetPropertyValue_InContainer(&Component)
            ? TEXT("PerformTraversalAction_Mover") : TEXT("PerformTraversalAction_CMC"));
        return Play && Play->NumParms == 0 ? Play : nullptr;
    }

    bool HasChosenMontage(const FStructProperty& Result, const void* Data)
    {
        const auto* Montage = FindFProperty<FObjectPropertyBase>(Result.Struct,
            TEXT("ChosenMontage_71_7693BD7146A29E9A85C28DB52F45F7FD"));
        return Montage && IsValid(Cast<UAnimMontage>(Montage->GetObjectPropertyValue_InContainer(Data)));
    }

    // Motion Warping is an authored component. Verify its loaded API before invoking it
    // without introducing a runtime module dependency.
    struct FTraversalWarpFunctions
    {
        UFunction* Update = nullptr;
        UFunction* Remove = nullptr;
        const FNameProperty* UpdateName = nullptr;
        const FStructProperty* UpdateLocation = nullptr;
        const FStructProperty* UpdateRotation = nullptr;
        const FNameProperty* RemoveName = nullptr;

        bool Resolve(UActorComponent& Component)
        {
            const UClass* WarpClass = FindObject<UClass>(nullptr, TEXT("/Script/MotionWarping.MotionWarpingComponent"));
            if (!WarpClass || !Component.IsA(WarpClass))
            {
                return false;
            }
            Update = Component.FindFunction(TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"));
            Remove = Component.FindFunction(TEXT("RemoveWarpTarget"));
            if (!Update || Update->NumParms != 3 || !Remove || Remove->NumParms != 2 ||
                !CastField<FIntProperty>(Remove->GetReturnProperty()))
            {
                return false;
            }
            UpdateName = FindFProperty<FNameProperty>(Update, TEXT("WarpTargetName"));
            UpdateLocation = FindFProperty<FStructProperty>(Update, TEXT("TargetLocation"));
            UpdateRotation = FindFProperty<FStructProperty>(Update, TEXT("TargetRotation"));
            RemoveName = FindFProperty<FNameProperty>(Remove, TEXT("WarpTargetName"));
            return UpdateName && RemoveName && UpdateLocation && UpdateRotation &&
                UpdateLocation->Struct == TBaseStructure<FVector>::Get() &&
                UpdateRotation->Struct == TBaseStructure<FRotator>::Get();
        }

        void Set(UActorComponent& Component, FName Name, const FVector& Location, const FRotator& Rotation) const
        {
            FStructOnScope Parameters(Update);
            void* Data = Parameters.GetStructMemory();
            UpdateName->SetPropertyValue_InContainer(Data, Name);
            *UpdateLocation->ContainerPtrToValuePtr<FVector>(Data) = Location;
            *UpdateRotation->ContainerPtrToValuePtr<FRotator>(Data) = Rotation;
            Component.ProcessEvent(Update, Data);
        }

        void Clear(UActorComponent& Component, FName Name) const
        {
            FStructOnScope Parameters(Remove);
            RemoveName->SetPropertyValue_InContainer(Parameters.GetStructMemory(), Name);
            Component.ProcessEvent(Remove, Parameters.GetStructMemory());
        }
    };

    bool ReceivesAutonomousMoves(const AActor* Owner)
    {
        return Owner && Owner->HasAuthority() && Owner->GetRemoteRole() == ROLE_AutonomousProxy;
    }

    UCharacterMovementComponent* FindCharacterMovement(const UActorComponent& Component)
    {
        const ACharacter* Character = Cast<ACharacter>(Component.GetOwner());
        return Character ? Character->GetCharacterMovement() : nullptr;
    }

    bool UsesAirborneExit(const USampleTraversalComponent& Component)
    {
        static const FName ResultName(TEXT("TraversalResult"));
        static const FName ActionName(TEXT("ActionType_2_96B748004D5339073D1AB49946EAF925"));
        constexpr TCHAR AirborneAction[] = TEXT("NewEnumerator1");

        const FStructProperty* Result = FindFProperty<FStructProperty>(Component.GetClass(), ResultName);
        if (!Result)
        {
            return false;
        }
        const void* Data = Result->ContainerPtrToValuePtr<void>(&Component);
        const FProperty* Action = Result->Struct->FindPropertyByName(ActionName);
        if (const FEnumProperty* Enum = CastField<FEnumProperty>(Action))
        {
            const int64 Airborne = Enum->GetEnum()->GetValueByNameString(AirborneAction);
            return Airborne != INDEX_NONE && Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                Enum->ContainerPtrToValuePtr<void>(Data)) == Airborne;
        }
        if (const FByteProperty* Byte = CastField<FByteProperty>(Action); Byte && Byte->Enum)
        {
            return Byte->GetPropertyValue_InContainer(Data) == Byte->Enum->GetValueByNameString(AirborneAction);
        }
        return false;
    }
}

void USampleTraversalComponent::ReadTraversalDebugSettings(int32& Level, float& Duration)
{
    Level = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.Traversal.DrawDebugLevel"));
    Duration = UKismetSystemLibrary::GetConsoleVariableFloatValue(TEXT("DDCvar.Traversal.DrawDebugDuration"));
}

void USampleTraversalComponent::PrintTraversalDiagnostic(int32 Level, int32 Threshold, const FString& Message,
    bool bPrintToScreen, bool bPrintToLog, FLinearColor Color, float Duration, FName Key)
{
    if (Level >= Threshold)
    {
        UKismetSystemLibrary::PrintString(this, Message, bPrintToScreen, bPrintToLog, Color, Duration, Key);
    }
}

bool USampleTraversalComponent::ResolveTraversalBackFloorLocation(FVector BackLedge, FVector BackNormal,
    FVector BackFloor, double AnimatedLedgeDistance, double AnimatedFloorDistance, FVector& Location)
{
    if (!IsFiniteVector(BackLedge) || !IsFiniteVector(BackNormal) || !IsFiniteVector(BackFloor) ||
        !FMath::IsFinite(AnimatedLedgeDistance) || !FMath::IsFinite(AnimatedFloorDistance))
    {
        return false;
    }
    const double Distance = FMath::Abs(AnimatedLedgeDistance - AnimatedFloorDistance);
    const double X = BackLedge.X + BackNormal.X * Distance;
    const double Y = BackLedge.Y + BackNormal.Y * Distance;
    if (!FMath::IsFinite(Distance) || !FMath::IsFinite(X) || !FMath::IsFinite(Y))
    {
        return false;
    }
    Location = FVector(X, Y, BackFloor.Z);
    return true;
}

bool USampleTraversalComponent::UpdateTraversalWarpTargets(UActorComponent* MotionWarping, FVector FrontLedge,
    FVector FrontNormal, FVector FrontOffset, bool bUseBackLedge, FVector BackLedge,
    FRotator BackLedgeRotation, bool bUseBackFloor, FVector BackNormal, FVector BackFloor,
    bool bHasDistanceSamples, double AnimatedLedgeDistance, double AnimatedFloorDistance, FRotator BackFloorRotation)
{
    FTraversalWarpFunctions Functions;
    if (!IsValid(MotionWarping) || !GetOwner() || MotionWarping->GetOwner() != GetOwner() ||
        !Functions.Resolve(*MotionWarping))
    {
        return false;
    }
    const double FrontX = FrontLedge.X + FrontOffset.X;
    const double FrontY = FrontLedge.Y + FrontOffset.Y;
    const double FrontZ = FrontLedge.Z + FrontOffset.Z;
    if (!FMath::IsFinite(FrontX) || !FMath::IsFinite(FrontY) || !FMath::IsFinite(FrontZ) ||
        !IsFiniteVector(FrontNormal) || FrontNormal.IsNearlyZero() || !FMath::IsFinite(FrontNormal.SizeSquared()))
    {
        Functions.Clear(*MotionWarping, FrontLedgeTarget);
        Functions.Clear(*MotionWarping, BackLedgeTarget);
        Functions.Clear(*MotionWarping, BackFloorTarget);
        return false;
    }
    Functions.Set(*MotionWarping, FrontLedgeTarget, FVector(FrontX, FrontY, FrontZ),
        FRotationMatrix::MakeFromX(-FrontNormal).Rotator());

    const bool bBackLedgeValid = IsFiniteVector(BackLedge) && !BackLedgeRotation.ContainsNaN();
    if (bUseBackLedge && bBackLedgeValid)
    {
        Functions.Set(*MotionWarping, BackLedgeTarget, BackLedge, BackLedgeRotation);
    }
    else
    {
        Functions.Clear(*MotionWarping, BackLedgeTarget);
    }

    FVector FloorLocation;
    const bool bFloorValid = bUseBackFloor && bHasDistanceSamples && !BackFloorRotation.ContainsNaN() &&
        ResolveTraversalBackFloorLocation(BackLedge, BackNormal, BackFloor, AnimatedLedgeDistance,
            AnimatedFloorDistance, FloorLocation);
    if (bFloorValid)
    {
        Functions.Set(*MotionWarping, BackFloorTarget, FloorLocation, BackFloorRotation);
    }
    else
    {
        Functions.Clear(*MotionWarping, BackFloorTarget);
    }
    return (!bUseBackLedge || bBackLedgeValid) && (!bUseBackFloor || bFloorValid);
}

void USampleTraversalComponent::InitializeTraversalRuntime()
{
    auto* Owner = GetOwner();
    if (const auto* Snapshot = FindFProperty<FStructProperty>(GetClass(), TEXT("CharacterProperties")))
    {
        void* Cached = Snapshot->ContainerPtrToValuePtr<void>(this);
        UFunction* Query = Owner ? Owner->FindFunction(TEXT("Get_PropertiesForTraversal")) : nullptr;
        const auto* Result = Query ? CastField<FStructProperty>(Query->GetReturnProperty()) : nullptr;
        if (Result && Query->NumParms == 1 && Result->SameType(Snapshot))
        {
            FStructOnScope Parameters(Query);
            Owner->ProcessEvent(Query, Parameters.GetStructMemory());
            Snapshot->CopyCompleteValue(Cached, Result->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()));
        }
        else
        {
            Snapshot->Struct->ClearScriptStruct(Cached);
        }
    }

    const auto* IsMover = FindFProperty<FBoolProperty>(GetClass(), MoverFlag);
    const auto* Pawn = Cast<APawn>(Owner);
    if (Pawn && IsMover && IsMover->GetPropertyValue_InContainer(this))
    {
        const auto* MoverField = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("Mover"));
        auto* Movement = Pawn->FindComponentByClass<UCharacterMoverComponent>();
        if (MoverField && IsValid(Movement) && Movement->IsA(MoverField->PropertyClass))
        {
            MoverField->SetObjectPropertyValue_InContainer(this, Movement);
        }
    }
}

void USampleTraversalComponent::SetMovementMode_CMC(EMovementMode CMCMovementMode)
{
    if (UCharacterMovementComponent* Movement = FindCharacterMovement(*this))
    {
        Movement->SetMovementMode(CMCMovementMode);
    }
}

EMovementMode USampleTraversalComponent::GetEndMovementMode_CMC() const
{
    return UsesAirborneExit(*this) ? MOVE_Falling : MOVE_Walking;
}

void USampleTraversalComponent::SetReplicationBehavior(bool ClientAuthoritative)
{
    const FBoolProperty* IsMover = FindFProperty<FBoolProperty>(GetClass(), MoverFlag);
    if (IsMover && IsMover->GetPropertyValue_InContainer(this))
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = FindCharacterMovement(*this))
    {
        Movement->bIgnoreClientMovementErrorChecksAndCorrection = ClientAuthoritative;
        Movement->bServerAcceptClientAuthoritativePosition = ClientAuthoritative;
    }
}

void USampleTraversalComponent::SetTraversalActionActive(bool bActive)
{
    if (const FBoolProperty* Activity = FindFProperty<FBoolProperty>(GetClass(), TEXT("DoingTraversalAction")))
    {
        Activity->SetPropertyValue_InContainer(this, bActive);
    }
}

bool USampleTraversalComponent::IsTraversalActionActive() const
{
    const auto* Activity = FindFProperty<FBoolProperty>(GetClass(), TEXT("DoingTraversalAction"));
    return Activity && Activity->GetPropertyValue_InContainer(this);
}

void USampleTraversalComponent::BeginTraversalCollision(UPrimitiveComponent* Capsule, UPrimitiveComponent* Obstacle,
    UPrimitiveComponent* Mesh)
{
    if (TraversalCapsule.Get() == Capsule && TraversalObstacle.Get() == Obstacle && TraversalMesh.Get() == Mesh)
    {
        return;
    }
    EndTraversalCollision();
    TraversalCapsule = Capsule;
    TraversalObstacle = Obstacle;
    TraversalMesh = Mesh;
    if (IsValid(Capsule) && IsValid(Obstacle) && !Capsule->GetMoveIgnoreComponents().Contains(Obstacle))
    {
        Capsule->IgnoreComponentWhenMoving(Obstacle, true);
        bAddedMoveIgnore = true;
    }
    if (IsValid(Mesh))
    {
        SavedMeshResponse = Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic);
        Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
    }
}

void USampleTraversalComponent::EndTraversalCollision()
{
    if (bAddedMoveIgnore)
    {
        UPrimitiveComponent* Capsule = TraversalCapsule.Get();
        UPrimitiveComponent* Obstacle = TraversalObstacle.Get();
        if (Capsule && Obstacle)
        {
            Capsule->IgnoreComponentWhenMoving(Obstacle, false);
        }
    }
    if (UPrimitiveComponent* Mesh = TraversalMesh.Get();
        Mesh && Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Ignore)
    {
        Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, SavedMeshResponse);
    }
    TraversalCapsule.Reset();
    TraversalObstacle.Reset();
    TraversalMesh.Reset();
    bAddedMoveIgnore = false;
}

void USampleTraversalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bGameplayAcquired = false;
    EndTraversalCollision();
    SetTraversalActionActive(false);
    FinishMoverTraversalMode();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CmcCorrectionTimer);
    }
    RestoreCmcCorrection();
    Super::EndPlay(EndPlayReason);
}

void USampleTraversalComponent::BeginTraversalGameplay(bool bMover, UPrimitiveComponent* Capsule,
    UPrimitiveComponent* Obstacle, UPrimitiveComponent* Mesh, UMoverComponent* Mover)
{
    if (bGameplayAcquired)
    {
        return;
    }
    bGameplayUsesMover = bMover;
    bGameplayAcquired = true;
    BeginTraversalCollision(Capsule, Obstacle, bMover ? Mesh : nullptr);
    if (bMover)
    {
        BeginMoverTraversalMode(Mover);
    }
    else
    {
        BeginCmcTraversalMode();
    }
}

void USampleTraversalComponent::FinishTraversalGameplay()
{
    if (!bGameplayAcquired)
    {
        return;
    }
    bGameplayAcquired = false;
    SetTraversalActionActive(false);
    EndTraversalCollision();
    if (bGameplayUsesMover)
    {
        FinishMoverTraversalMode();
    }
    else
    {
        FinishCmcTraversalMode();
    }
}

void USampleTraversalComponent::BeginCmcTraversalMode()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CmcCorrectionTimer);
    }
    SetMovementMode_CMC(MOVE_Flying);
    SetReplicationBehavior(true);
    bCmcCorrectionSuspended = true;
}

void USampleTraversalComponent::FinishCmcTraversalMode()
{
    UWorld* World = GetWorld();
    const bool bWaitForClientExit = World && bCmcCorrectionSuspended && ReceivesAutonomousMoves(GetOwner());
    if (!bWaitForClientExit)
    {
        SetMovementMode_CMC(GetEndMovementMode_CMC());
    }
    if (!bCmcCorrectionSuspended)
    {
        return;
    }
    if (World)
    {
        const double Deadline = World->GetTimeSeconds() + MaxRemoteExitWait;
        World->GetTimerManager().SetTimer(CmcCorrectionTimer,
            FTimerDelegate::CreateUObject(this, &USampleTraversalComponent::CompleteCmcCorrection,
                bWaitForClientExit, Deadline),
            CorrectionGracePeriod, false);
    }
    else
    {
        RestoreCmcCorrection();
    }
}

void USampleTraversalComponent::CompleteCmcCorrection(bool bWaitForClientExit, double Deadline)
{
    UWorld* World = GetWorld();
    const UCharacterMovementComponent* Movement = FindCharacterMovement(*this);
    if (bWaitForClientExit && Movement && Movement->MovementMode == MOVE_Flying)
    {
        // The authority's montage can finish before the owner sends its first post-traversal move.
        // Keep accepting movement modes until that move arrives or the handoff times out.
        if (World && World->GetTimeSeconds() < Deadline && ReceivesAutonomousMoves(GetOwner()))
        {
            World->GetTimerManager().SetTimer(CmcCorrectionTimer,
                FTimerDelegate::CreateUObject(this, &USampleTraversalComponent::CompleteCmcCorrection,
                    bWaitForClientExit, Deadline),
                CorrectionGracePeriod, false);
            return;
        }
        SetMovementMode_CMC(GetEndMovementMode_CMC());
    }
    RestoreCmcCorrection();
}

void USampleTraversalComponent::RestoreCmcCorrection()
{
    if (bCmcCorrectionSuspended)
    {
        SetReplicationBehavior(false);
        bCmcCorrectionSuspended = false;
    }
}

void USampleTraversalComponent::BeginMoverTraversalMode(UMoverComponent* Mover)
{
    if (!IsValid(Mover) || Mover->GetOwner() != GetOwner())
    {
        return;
    }
    if (TraversalMover.Get() && TraversalMover.Get() != Mover)
    {
        FinishMoverTraversalMode();
    }
    TraversalMover = Mover;
    const FMoverTimeStep& Step = Mover->GetLastTimeStep();
    MoverStartFrame = Step.ServerFrame;
    MoverStartTimeMs = Step.BaseSimTimeMs;
    Mover->QueueNextMode(TraversingMode, true);
}

void USampleTraversalComponent::FinishMoverTraversalMode()
{
    UMoverComponent* Mover = TraversalMover.Get();
    TraversalMover.Reset();
    if (!Mover)
    {
        return;
    }
    if (Mover->GetMovementModeName() == TraversingMode)
    {
        Mover->QueueNextMode(WalkingMode, true);
        return;
    }
    const FMoverTimeStep& Step = Mover->GetLastTimeStep();
    if (Step.ServerFrame == MoverStartFrame && Step.BaseSimTimeMs == MoverStartTimeMs)
    {
        Mover->QueueNextMode(NAME_None);
    }
}

bool USampleTraversalComponent::ShouldReceiveTraversalReplay() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn && !Pawn->IsLocallyControlled();
}

void USampleTraversalComponent::ApplyTraversalReplay(int32&)
{
    checkNoEntry();
}

void USampleTraversalComponent::PublishTraversalAction(int32&)
{
    checkNoEntry();
}

bool USampleTraversalComponent::TryPublishTraversalSelection(bool, int32&)
{
    checkNoEntry();
    return false;
}

bool USampleTraversalComponent::DispatchTraversalSelection(const FStructProperty* Incoming, const void* Data)
{
    const FName ResultName(TEXT("TraversalResult"));
    const auto* Stored = FindFProperty<FStructProperty>(GetClass(), ResultName);
    if (!Stored || !Incoming || !Data || !Stored->SameType(Incoming) || !HasChosenMontage(*Incoming, Data))
    {
        return false;
    }
    UFunction* Play = FindTraversalPlayback(*this);
    UFunction* Server = FindFunction(TEXT("PerformTraversalAction_Server"));
    const auto* Parameter = Server ? FindFProperty<FStructProperty>(Server, ResultName) : nullptr;
    if (!Play || !Parameter || !Parameter->SameType(Stored) || Server->NumParms != 1 ||
        !Server->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer))
    {
        return false;
    }
    FStructOnScope Params(Server);
    Parameter->CopyCompleteValue(Parameter->ContainerPtrToValuePtr<void>(Params.GetStructMemory()), Data);
    void* Cached = Stored->ContainerPtrToValuePtr<void>(this);
    if (Cached != Data)
    {
        Stored->CopyCompleteValue(Cached, Data);
    }
    ProcessEvent(Play, nullptr);
    ProcessEvent(Server, Params.GetStructMemory());
    ++TraversalDispatchSerial;
    return true;
}

DEFINE_FUNCTION(USampleTraversalComponent::execPublishTraversalAction)
{
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const auto* Incoming = CastField<FStructProperty>(Stack.MostRecentProperty);
    const void* Data = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    P_THIS->DispatchTraversalSelection(Incoming, Data);
    P_NATIVE_END;
}

DEFINE_FUNCTION(USampleTraversalComponent::execTryPublishTraversalSelection)
{
    P_GET_UBOOL(bActionAllowed);
    Stack.MostRecentProperty = nullptr;
    Stack.MostRecentPropertyAddress = nullptr;
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const auto* Incoming = CastField<FStructProperty>(Stack.MostRecentProperty);
    const void* Data = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    *static_cast<bool*>(RESULT_PARAM) = bActionAllowed && P_THIS->DispatchTraversalSelection(Incoming, Data);
    P_NATIVE_END;
}

DEFINE_FUNCTION(USampleTraversalComponent::execApplyTraversalReplay)
{
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const auto* Incoming = CastField<FStructProperty>(Stack.MostRecentProperty);
    const void* Data = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    if (Incoming && Data)
    {
        const auto* Stored = FindFProperty<FStructProperty>(P_THIS->GetClass(), TEXT("TraversalResult"));
        if (Stored && Stored->Struct == Incoming->Struct)
        {
            if (P_THIS->ShouldReceiveTraversalReplay())
            {
                UFunction* Play = FindTraversalPlayback(*P_THIS);
                if (Play && HasChosenMontage(*Incoming, Data))
                {
                    void* Cached = Stored->ContainerPtrToValuePtr<void>(P_THIS);
                    if (Cached != Data)
                    {
                        Stored->Struct->CopyScriptStruct(Cached, Data);
                    }
                    P_THIS->ProcessEvent(Play, nullptr);
                }
            }
            else if (P_THIS->bCmcCorrectionSuspended)
            {
                const auto* Activity = FindFProperty<FBoolProperty>(P_THIS->GetClass(), TEXT("DoingTraversalAction"));
                if (Activity && Activity->GetPropertyValue_InContainer(P_THIS))
                {
                    // A correction sent before the server saw the request may have restored Walking.
                    P_THIS->SetMovementMode_CMC(MOVE_Flying);
                }
            }
        }
    }
    P_NATIVE_END;
}
