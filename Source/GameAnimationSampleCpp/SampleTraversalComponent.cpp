#include "SampleTraversalComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    const FName TraversingMode(TEXT("Traversing"));
    const FName WalkingMode(TEXT("Walking"));
    const FName MoverFlag(TEXT("IsMoverCharacter"));

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
    SetMovementMode_CMC(GetEndMovementMode_CMC());
    if (!bCmcCorrectionSuspended)
    {
        return;
    }
    if (UWorld* World = GetWorld())
    {
        constexpr float CorrectionGracePeriod = 0.2f;
        World->GetTimerManager().SetTimer(CmcCorrectionTimer, this, &USampleTraversalComponent::RestoreCmcCorrection,
            CorrectionGracePeriod, false);
    }
    else
    {
        RestoreCmcCorrection();
    }
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

DEFINE_FUNCTION(USampleTraversalComponent::execPublishTraversalAction)
{
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    const auto* Incoming = CastField<FStructProperty>(Stack.MostRecentProperty);
    const void* Data = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    const FName ResultName(TEXT("TraversalResult"));
    const auto* Stored = FindFProperty<FStructProperty>(P_THIS->GetClass(), ResultName);
    if (Stored && Incoming && Data && Stored->SameType(Incoming))
    {
        void* CachedResult = Stored->ContainerPtrToValuePtr<void>(P_THIS);
        Stored->CopyCompleteValue(CachedResult, Data);
        if (UFunction* Play = P_THIS->FindFunction(TEXT("PerformTraversalAction")); Play && Play->NumParms == 0)
        {
            P_THIS->ProcessEvent(Play, nullptr);
        }

        UFunction* Server = P_THIS->FindFunction(TEXT("PerformTraversalAction_Server"));
        const auto* Parameter = Server ? FindFProperty<FStructProperty>(Server, ResultName) : nullptr;
        if (Parameter && Parameter->SameType(Stored) && Server->NumParms == 1 &&
            Server->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer))
        {
            FStructOnScope Params(Server);
            Parameter->CopyCompleteValue(Parameter->ContainerPtrToValuePtr<void>(Params.GetStructMemory()), CachedResult);
            P_THIS->ProcessEvent(Server, Params.GetStructMemory());
        }
    }
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
                UFunction* Play = P_THIS->FindFunction(TEXT("PerformTraversalAction"));
                if (Play && Play->ParmsSize == 0)
                {
                    Stored->Struct->CopyScriptStruct(Stored->ContainerPtrToValuePtr<void>(P_THIS), Data);
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
