#include "SampleSmartObjectTasks.h"

#include "AIController.h"
#include "Async/Async.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameplayTask.h"
#include "Kismet/GameplayStatics.h"
#include "MoverComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "SampleCmcCharacter.h"
#include "SampleMoverCharacter.h"
#include "SampleSmartObjectInteractionComponent.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "SmartObjectComponent.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    const FName ApproachActorName(TEXT("Actor"));
    const FName ApproachMoverName(TEXT("PossibleOwnerMoverComponent"));
    const FName ApproachAnimationName(TEXT("Smart Object Animation Component"));
    const FName MoveCompletedName(TEXT("OnMovementCompleted"));
    const FName MontageFinishedName(TEXT("On Montage Finished"));
    const FName MontageFinishedDelegateName(TEXT("Owner Montage Finished"));

    // StateTree tasks execute on the game thread. Weak entries do not extend their lifetime,
    // and the subsystem comparison separates identical handles in different worlds.
    TArray<TWeakObjectPtr<USampleClaimSmartObjectTask>> PendingClaimTasks;

    template <typename T>
    T* FindStruct(UObject& Task, FName Name, const UScriptStruct* Type)
    {
        const auto* Property = FindFProperty<FStructProperty>(Task.GetClass(), Name);
        return Property && Property->Struct == Type ? Property->ContainerPtrToValuePtr<T>(&Task) : nullptr;
    }

    const FObjectPropertyBase* FindActorProperty(const UObject& Task, FName Name)
    {
        const auto* Property = FindFProperty<FObjectPropertyBase>(Task.GetClass(), Name);
        return Property && Property->PropertyClass->IsChildOf(AActor::StaticClass()) ? Property : nullptr;
    }

    AActor* ReadActor(const UObject& Task, FName Name)
    {
        const auto* Property = FindActorProperty(Task, Name);
        return Property ? Cast<AActor>(Property->GetObjectPropertyValue_InContainer(&Task)) : nullptr;
    }

    double ReadTaskNumber(const UObject& Task, FName Name)
    {
        const auto* Property = FindFProperty<FNumericProperty>(Task.GetClass(), Name);
        return Property && Property->IsFloatingPoint()
            ? Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Task)) : 0.0;
    }

    const FMapProperty* FindCooldownMap(const AActor* Controller)
    {
        const auto* Map = IsValid(Controller) ? FindFProperty<FMapProperty>(Controller->GetClass(), TEXT("Cooldowns")) : nullptr;
        const auto* Value = Map ? CastField<FNumericProperty>(Map->ValueProp) : nullptr;
        return Value && Value->IsFloatingPoint() && CastField<FStrProperty>(Map->KeyProp) ? Map : nullptr;
    }

    TArray<FSmartObjectRequestResult>* FindSearchResults(UObject& Task)
    {
        const auto* Array = FindFProperty<FArrayProperty>(Task.GetClass(), TEXT("SearchResults"));
        const auto* Item = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
        return Item && Item->Struct == FSmartObjectRequestResult::StaticStruct()
            ? Array->ContainerPtrToValuePtr<TArray<FSmartObjectRequestResult>>(&Task) : nullptr;
    }

    FString ReadSearchMode(const UObject& Task)
    {
        static const FName Name(TEXT("SearchType"));
        if (const auto* Property = FindFProperty<FEnumProperty>(Task.GetClass(), Name))
        {
            const int64 Value = Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                Property->ContainerPtrToValuePtr<void>(&Task));
            return Property->GetEnum()->GetNameStringByValue(Value);
        }
        if (const auto* Property = FindFProperty<FByteProperty>(Task.GetClass(), Name);
            Property && Property->Enum)
        {
            return Property->Enum->GetNameStringByValue(Property->GetPropertyValue_InContainer(&Task));
        }
        return {};
    }

    FSmartObjectRequestResult SelectByDistance(const USmartObjectSubsystem& Subsystem,
        const TArray<FSmartObjectRequestResult>& Results, const FVector& Origin, bool bClosest)
    {
        FSmartObjectRequestResult Selected;
        double BestDistance = 0.0;
        for (const FSmartObjectRequestResult& Result : Results)
        {
            const TOptional<FTransform> Transform = Subsystem.GetSlotTransform(Result);
            if (!Transform)
            {
                continue;
            }
            const double Distance = FVector::DistSquared(Transform->GetLocation(), Origin);
            if (!Selected.IsValid() || (bClosest ? Distance <= BestDistance : Distance >= BestDistance))
            {
                Selected = Result;
                BestDistance = Distance;
            }
        }
        return Selected;
    }

    FSmartObjectClaimHandle ClaimFreeSlot(USmartObjectSubsystem& Subsystem,
        const FSmartObjectSlotHandle& Slot, const AActor& Actor)
    {
        // CanBeClaimed alone permits overriding lower-priority reservations.
        if (Slot.IsValid() && Subsystem.GetSlotState(Slot) == ESmartObjectSlotState::Free)
        {
            return Subsystem.MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal,
                FConstStructView::Make(FSmartObjectActorUserData(&Actor)));
        }
        return {};
    }
}

void USampleMotionMatchApproachTask::InitializeApproachTask()
{
    UnbindApproachCallbacks();
    AActor* Actor = ReadActor(*this, ApproachActorName);
    const auto CacheComponent = [this, Actor](FName Name) -> UActorComponent*
    {
        const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), Name);
        if (!Field || !Field->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
        {
            return nullptr;
        }
        UActorComponent* Component = IsValid(Actor) ? Actor->GetComponentByClass(Field->PropertyClass) : nullptr;
        Field->SetObjectPropertyValue_InContainer(this, Component);
        return Component;
    };
    UActorComponent* Animation = CacheComponent(ApproachAnimationName);
    CacheComponent(ApproachMoverName);

    AAIController* Controller = UAIBlueprintHelperLibrary::GetAIController(Actor);
    if (Controller && FindFunction(MoveCompletedName))
    {
        FScriptDelegate Handler;
        Handler.BindUFunction(this, MoveCompletedName);
        Controller->ReceiveMoveCompleted.AddUnique(Handler);
        ApproachController = Controller;
    }
    const auto* Delegate = Animation
        ? FindFProperty<FMulticastDelegateProperty>(Animation->GetClass(), MontageFinishedDelegateName) : nullptr;
    const UFunction* HandlerFunction = FindFunction(MontageFinishedName);
    if (Delegate && HandlerFunction && HandlerFunction->IsSignatureCompatibleWith(Delegate->SignatureFunction))
    {
        FScriptDelegate Handler;
        Handler.BindUFunction(this, MontageFinishedName);
        Delegate->AddDelegate(Handler, Animation, Delegate->ContainerPtrToValuePtr<void>(Animation));
        ApproachAnimation = Animation;
    }
}

double USampleMotionMatchApproachTask::ReadApproachDistance() const
{
    AActor* Actor = ReadActor(*this, ApproachActorName);
    const AAIController* Controller = UAIBlueprintHelperLibrary::GetAIController(Actor);
    const UPathFollowingComponent* Following = Controller ? Controller->GetPathFollowingComponent() : nullptr;
    const FNavPathSharedPtr Path = Following ? Following->GetPath() : nullptr;
    if (Path)
    {
        return Path->IsValid() ? Path->GetLength() : -1.0;
    }
    const auto* Field = FindFProperty<FStructProperty>(GetClass(), TEXT("Destination"));
    const FVector Destination = Field && Field->Struct == TBaseStructure<FTransform>::Get()
        ? Field->ContainerPtrToValuePtr<FTransform>(this)->GetLocation() : FVector::ZeroVector;
    return FVector::Dist(Actor ? Actor->GetActorLocation() : FVector::ZeroVector, Destination);
}

FVector USampleMotionMatchApproachTask::ReadApproachVelocity() const
{
    const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), ApproachMoverName);
    const auto* Mover = Field ? Cast<UMoverComponent>(Field->GetObjectPropertyValue_InContainer(this)) : nullptr;
    if (IsValid(Mover))
    {
        return Mover->GetVelocity();
    }
    const AActor* Actor = ReadActor(*this, ApproachActorName);
    return IsValid(Actor) ? Actor->GetVelocity() : FVector::ZeroVector;
}

void USampleMotionMatchApproachTask::ExitState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    UnbindApproachCallbacks();
    Super::ExitState(Context, Transition);
}

void USampleMotionMatchApproachTask::UnbindApproachCallbacks()
{
    if (AAIController* Controller = ApproachController.Get())
    {
        FScriptDelegate Handler;
        Handler.BindUFunction(this, MoveCompletedName);
        Controller->ReceiveMoveCompleted.Remove(Handler);
    }
    if (UActorComponent* Animation = ApproachAnimation.Get())
    {
        if (const auto* Delegate = FindFProperty<FMulticastDelegateProperty>(Animation->GetClass(), MontageFinishedDelegateName))
        {
            FScriptDelegate Handler;
            Handler.BindUFunction(this, MontageFinishedName);
            Delegate->RemoveDelegate(Handler, Animation, Delegate->ContainerPtrToValuePtr<void>(Animation));
        }
    }
    ApproachController.Reset();
    ApproachAnimation.Reset();
}

EStateTreeRunStatus USampleFindSmartObjectTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    auto* Results = FindSearchResults(*this);
    auto* Result = FindStruct<FSmartObjectRequestResult>(*this, TEXT("Search Result"), FSmartObjectRequestResult::StaticStruct());
    auto* Candidate = FindStruct<FSmartObjectSlotHandle>(*this, TEXT("CandidateSlot"), FSmartObjectSlotHandle::StaticStruct());
    const auto* Target = FindActorProperty(*this, TEXT("Smart Object"));
    const auto* Extents = FindStruct<FVector>(*this, TEXT("Search Box Extents"), TBaseStructure<FVector>::Get());
    const auto* Tags = FindStruct<FGameplayTagContainer>(*this, TEXT("Smart Object User Tags"), FGameplayTagContainer::StaticStruct());
    if (!Results || !Result || !Candidate || !Target || !Extents || !Tags)
    {
        return EStateTreeRunStatus::Failed;
    }

    Results->Reset();
    *Result = {};
    *Candidate = {};
    Target->SetObjectPropertyValue_InContainer(this, nullptr);
    const AActor* Actor = ReadActor(*this, TEXT("Actor"));
    const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(Context.GetWorld());
    if (!IsValid(Actor) || !Subsystem)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FVector Origin = Actor->GetActorLocation();
    FSmartObjectRequestFilter Filter;
    Filter.UserTags = *Tags;
    Filter.BehaviorDefinitionClasses.Add(UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());
    const FSmartObjectRequest Request(FBox::BuildAABB(Origin, *Extents), Filter);
    if (!Subsystem->FindSmartObjects_BP(Request, *Results, Actor) || Results->IsEmpty())
    {
        return EStateTreeRunStatus::Failed;
    }

    const FString Mode = ReadSearchMode(*this);
    if (Mode == TEXT("NewEnumerator1"))
    {
        *Result = (*Results)[0];
    }
    else if (Mode == TEXT("NewEnumerator2"))
    {
        for (int32 Index = Results->Num() - 1; Index > 0; --Index)
        {
            Results->Swap(Index, FMath::RandRange(0, Index));
        }
        *Result = (*Results)[0];
    }
    else
    {
        *Result = SelectByDistance(*Subsystem, *Results, Origin, Mode != TEXT("NewEnumerator3"));
    }

    if (Result->IsValid())
    {
        *Candidate = Result->SlotHandle;
        if (const auto* Component = Subsystem->GetSmartObjectComponentByRequestResult(*Result))
        {
            AActor* Owner = Component->GetOwner();
            Target->SetObjectPropertyValue_InContainer(this, Owner);
            return IsValid(Owner) && Candidate->IsValid() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
        }
    }
    return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus USampleClaimSmartObjectTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    auto* Claim = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("ClaimedHandle"), FSmartObjectClaimHandle::StaticStruct());
    const auto* Slot = FindStruct<FSmartObjectSlotHandle>(*this, TEXT("SlotToBeClaimed"), FSmartObjectSlotHandle::StaticStruct());
    Super::EnterState(Context, Transition);

    // A sustained parent state can be reselected while its interaction child is active.
    if (Transition.ChangeType == EStateTreeStateChangeType::Sustained && Claim && Claim->IsValid())
    {
        if (PendingClaim == *Claim && PendingClaimRevoked && PendingClaimRevoked->Load())
        {
            ReleasePendingClaim(true);
            Claim->Invalidate();
            return EStateTreeRunStatus::Failed;
        }
        if (PendingClaim == *Claim || TransferredClaim == *Claim)
        {
            return EStateTreeRunStatus::Running;
        }
    }
    ReleasePendingClaim(false);
    TransferredClaim.Invalidate();

    PendingActor = ReadActor(*this, TEXT("Actor"));
    AActor* Actor = PendingActor.Get();
    AActor* Target = ReadActor(*this, TEXT("SmartObject"));
    PendingPlayerRequestId = 0;
    if (Actor && Slot)
    {
        if (const auto* Component = Actor->FindComponentByClass<USampleSmartObjectInteractionComponent>())
        {
            PendingPlayerRequestId = Component->GetPlayerInteractionRequestId(Target, *Slot);
        }
    }
    if (Claim)
    {
        Claim->Invalidate();
    }
    auto* Subsystem = USmartObjectSubsystem::GetCurrent(Context.GetWorld());
    if (!Claim || !Slot || !IsValid(Actor) || !Subsystem)
    {
        ResetFailedPlayerActivity();
        return EStateTreeRunStatus::Failed;
    }

    *Claim = ArbitrateClaim(*Subsystem, *Slot, *Actor, Target);
    if (!Claim->IsValid())
    {
        ResetFailedPlayerActivity();
        return EStateTreeRunStatus::Failed;
    }

    if (!TrackPendingClaim(*Subsystem, *Claim))
    {
        ReleasePendingClaim(true);
        Claim->Invalidate();
        return EStateTreeRunStatus::Failed;
    }
    return EStateTreeRunStatus::Running;
}

FSmartObjectClaimHandle USampleClaimSmartObjectTask::ArbitrateClaim(USmartObjectSubsystem& Subsystem,
    const FSmartObjectSlotHandle& Slot, const AActor& Actor, AActor* Target)
{
    FSmartObjectClaimHandle Claim = ClaimFreeSlot(Subsystem, Slot, Actor);
    if (!Claim.IsValid() && IsValid(Target))
    {
        if (auto* Component = Target->FindComponentByClass<USmartObjectComponent>())
        {
            // The authored fallback uses the target itself as the filter's UserActor.
            const FSmartObjectHandle Object = Slot.IsValid()
                ? Slot.GetSmartObjectHandle() : Component->GetRegisteredHandle();
            TArray<FSmartObjectRequestResult> Results;
            USmartObjectBlueprintFunctionLibrary::FindSmartObjectsInComponent(
                FSmartObjectRequestFilter{}, Component, Results, Target);
            for (const FSmartObjectRequestResult& Result : Results)
            {
                if (Result.SlotHandle == Slot || Result.SlotHandle.GetSmartObjectHandle() != Object)
                {
                    continue;
                }
                Claim = ClaimFreeSlot(Subsystem, Result.SlotHandle, Actor);
                if (Claim.IsValid())
                {
                    break;
                }
            }
        }
    }
    return Claim;
}

bool USampleClaimSmartObjectTask::TrackPendingClaim(USmartObjectSubsystem& Subsystem,
    const FSmartObjectClaimHandle& Claim)
{
    PendingClaim = Claim;
    PendingSubsystem = &Subsystem;
    if (FOnSmartObjectEvent* Events = Subsystem.GetSlotEventDelegate(PendingClaim.SlotHandle))
    {
        PendingClaimRevoked = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
        const TWeakObjectPtr<USampleClaimSmartObjectTask> WeakTask(this);
        const FSmartObjectClaimHandle ObservedClaim = PendingClaim;
        const auto Revoked = PendingClaimRevoked;
        PendingSlotListener = Events->AddLambda([WeakTask, ObservedClaim, Revoked](const FSmartObjectEventData& Event)
        {
            if (Event.SlotHandle == ObservedClaim.SlotHandle && Event.Reason == ESmartObjectChangeReason::OnReleased)
            {
                // The subsystem broadcasts inside the slot mutation. Record revocation immediately,
                // then defer UObject and subsystem operations until outside that callback.
                Revoked->Store(true);
                AsyncTask(ENamedThreads::GameThread, [WeakTask, ObservedClaim]()
                {
                    if (auto* Task = WeakTask.Get())
                    {
                        Task->OnPendingClaimRevoked(ObservedClaim);
                    }
                });
            }
        });
        PendingClaimTasks.RemoveAllSwap([](const auto& Task) { return !Task.IsValid(); });
        PendingClaimTasks.AddUnique(TWeakObjectPtr<USampleClaimSmartObjectTask>(this));
        return true;
    }
    return false;
}

void USampleClaimSmartObjectTask::ExitState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    if (Transition.ChangeType != EStateTreeStateChangeType::Sustained)
    {
        ReleasePendingClaim(true);
        TransferredClaim.Invalidate();
    }
    Super::ExitState(Context, Transition);
}

USampleClaimSmartObjectTask* USampleClaimSmartObjectTask::FindPendingClaim(
    const USmartObjectSubsystem* Subsystem, const FSmartObjectClaimHandle& Claim)
{
    if (Subsystem && Claim.IsValid())
    {
        for (const auto& Entry : PendingClaimTasks)
        {
            if (auto* Task = Entry.Get(); Task && Task->PendingSubsystem.Get() == Subsystem && Task->PendingClaim == Claim)
            {
                return Task;
            }
        }
    }
    return nullptr;
}

void USampleClaimSmartObjectTask::ForgetPendingClaim()
{
    if (auto* Subsystem = PendingSubsystem.Get(); Subsystem && PendingSlotListener.IsValid())
    {
        if (FOnSmartObjectEvent* Events = Subsystem->GetSlotEventDelegate(PendingClaim.SlotHandle))
        {
            Events->Remove(PendingSlotListener);
        }
    }
    PendingSlotListener.Reset();
    PendingClaimRevoked.Reset();
    PendingClaimTasks.RemoveAllSwap([this](const auto& Task) { return !Task.IsValid() || Task.Get() == this; });
    PendingClaim.Invalidate();
    PendingSubsystem.Reset();
}

bool USampleClaimSmartObjectTask::ReleasePendingClaim(bool bResetPlayerActivity)
{
    if (!PendingClaim.IsValid())
    {
        return true;
    }
    auto* Subsystem = PendingSubsystem.Get();
    const FSmartObjectClaimHandle Claim = PendingClaim;
    const bool bRevoked = PendingClaimRevoked && PendingClaimRevoked->Load();
    ForgetPendingClaim();
    if (bResetPlayerActivity || bRevoked)
    {
        ResetFailedPlayerActivity();
    }
    if (bRevoked)
    {
        if (auto* Output = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("ClaimedHandle"), FSmartObjectClaimHandle::StaticStruct());
            Output && *Output == Claim)
        {
            Output->Invalidate();
        }
    }
    if (!Subsystem || bRevoked)
    {
        return true;
    }
    const ESmartObjectSlotState State = Subsystem->GetSlotState(Claim.SlotHandle);
    return (State != ESmartObjectSlotState::Claimed && State != ESmartObjectSlotState::Occupied)
        || Subsystem->MarkSlotAsFree(Claim);
}

void USampleClaimSmartObjectTask::ResetFailedPlayerActivity() const
{
    if (AActor* Actor = PendingActor.Get())
    {
        if (auto* Component = Actor->FindComponentByClass<USampleSmartObjectInteractionComponent>())
        {
            Component->ResetPlayerInteractionAfterFailedClaim(PendingPlayerRequestId);
        }
    }
}

void USampleClaimSmartObjectTask::OnPendingClaimRevoked(const FSmartObjectClaimHandle& Claim)
{
    if (PendingClaim == Claim && PendingClaimRevoked && PendingClaimRevoked->Load())
    {
        // An external release or priority override revoked this reservation. Do not attempt
        // another release: a different user may already own the same slot on the next call.
        ForgetPendingClaim();
        ResetFailedPlayerActivity();
        if (auto* Output = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("ClaimedHandle"), FSmartObjectClaimHandle::StaticStruct());
            Output && *Output == Claim)
        {
            Output->Invalidate();
        }
        FinishTask(false);
    }
}

EStateTreeRunStatus USampleReleaseSmartObjectTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    const auto* Claim = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("Claimed Handle"), FSmartObjectClaimHandle::StaticStruct());
    if (!Claim || !Claim->IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }
    auto* Pending = USampleClaimSmartObjectTask::FindPendingClaim(USmartObjectSubsystem::GetCurrent(Context.GetWorld()), *Claim);
    return !Pending || Pending->ReleasePendingClaim(false) ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus USampleUseSmartObjectTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    auto* Controller = Cast<AAIController>(ReadActor(*this, TEXT("AIController")));
    const auto* Claim = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("Claimed Handle"), FSmartObjectClaimHandle::StaticStruct());
    auto* Pending = Claim ? USampleClaimSmartObjectTask::FindPendingClaim(
        USmartObjectSubsystem::GetCurrent(Context.GetWorld()), *Claim) : nullptr;
    if (!Pending)
    {
        return EStateTreeRunStatus::Failed;
    }
    const auto FailDispatch = [Pending]()
    {
        Pending->ReleasePendingClaim(true);
        return EStateTreeRunStatus::Failed;
    };
    if (!IsValid(Controller) || (Pending->PendingClaimRevoked && Pending->PendingClaimRevoked->Load()))
    {
        return FailDispatch();
    }

    // The engine task factory is reflected but its class header is private to Gameplay Interactions.
    auto* Type = FindObject<UClass>(nullptr, TEXT("/Script/GameplayInteractionsModule.AITask_UseGameplayInteraction"));
    UFunction* Factory = Type ? Type->FindFunctionByName(TEXT("UseSmartObjectWithGameplayInteraction")) : nullptr;
    const auto* ControllerParam = Factory ? FindFProperty<FObjectPropertyBase>(Factory, TEXT("Controller")) : nullptr;
    const auto* ClaimParam = Factory ? FindFProperty<FStructProperty>(Factory, TEXT("ClaimHandle")) : nullptr;
    const auto* LockParam = Factory ? FindFProperty<FBoolProperty>(Factory, TEXT("bLockAILogic")) : nullptr;
    const auto* ReturnParam = Factory ? CastField<FObjectPropertyBase>(Factory->GetReturnProperty()) : nullptr;
    if (!ControllerParam || !ClaimParam || ClaimParam->Struct != FSmartObjectClaimHandle::StaticStruct() ||
        !LockParam || !ReturnParam)
    {
        return FailDispatch();
    }

    FStructOnScope Args(Factory);
    uint8* Data = Args.GetStructMemory();
    ControllerParam->SetObjectPropertyValue_InContainer(Data, Controller);
    *ClaimParam->ContainerPtrToValuePtr<FSmartObjectClaimHandle>(Data) = *Claim;
    LockParam->SetPropertyValue_InContainer(Data, true);
    Type->GetDefaultObject()->ProcessEvent(Factory, Data);
    auto* Task = Cast<UGameplayTask>(ReturnParam->GetObjectPropertyValue_InContainer(Data));
    if (!Task)
    {
        return FailDispatch();
    }
    // The factory has stored the handle on the engine task. Its OnDestroy releases it,
    // including activation failure; native cleanup must stop before ReadyForActivation.
    Pending->TransferredClaim = *Claim;
    Pending->ForgetPendingClaim();
    Task->ReadyForActivation();
    if (Task->IsFinished())
    {
        Pending->ResetFailedPlayerActivity();
        return EStateTreeRunStatus::Failed;
    }
    return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus USampleFindPatrolLocationTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    AActor* Actor = ReadActor(*this, TEXT("AIReference"));
    auto* Output = FindStruct<FVector>(*this, TEXT("Random Location"), TBaseStructure<FVector>::Get());
    if (!IsValid(Actor) || !Output)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FVector Origin = Actor->GetActorLocation();
    *Output = Origin;
    const double ExcludeDistance = ReadTaskNumber(*this, TEXT("SearchExcludeDistance"));
    const float Radius = static_cast<float>(ReadTaskNumber(*this, TEXT("SearchRadius")));
    TArray<AActor*> Exclusions;
    if (ExcludeDistance > 0.0)
    {
        UGameplayStatics::GetAllActorsOfClass(Actor, PatrolExclusionClass.LoadSynchronous(), Exclusions);
    }
    const double ExcludeSquared = FMath::Square(ExcludeDistance);

    // A fully excluded navigation area must yield instead of spinning on the game thread.
    constexpr int32 MaxAttempts = 16;
    for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
    {
        FVector Candidate;
        if (!UNavigationSystemV1::K2_GetRandomReachablePointInRadius(Actor, Origin, Candidate, Radius))
        {
            break;
        }
        if (!Exclusions.ContainsByPredicate([&](const AActor* Exclusion)
            {
                return FVector::DistSquared(Candidate, Exclusion->GetActorLocation()) < ExcludeSquared;
            }))
        {
            *Output = Candidate;
            break;
        }
    }
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus USampleFocusInteractionTargetTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    auto* Controller = Cast<AAIController>(ReadActor(*this, TEXT("AIController")));
    AActor* Target = ReadActor(*this, TEXT("TargetToFocus"));
    if (IsValid(Controller) && IsValid(Target))
    {
        Controller->K2_SetFocus(Target);
    }
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus USampleClearInteractionFocusTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    if (auto* Controller = Cast<AAIController>(ReadActor(*this, TEXT("AIController"))); IsValid(Controller))
    {
        Controller->K2_ClearFocus();
    }
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus USampleFindSlotTransformTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    auto* Transform = FindStruct<FTransform>(*this, TEXT("Out Slot Transform"), TBaseStructure<FTransform>::Get());
    const auto* Claim = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("Claim Handle"), FSmartObjectClaimHandle::StaticStruct());
    const auto* Subsystem = USmartObjectSubsystem::GetCurrent(Context.GetWorld());
    if (!Transform || !Claim || !Subsystem)
    {
        return EStateTreeRunStatus::Failed;
    }
    *Transform = Subsystem->GetSlotTransform(*Claim).Get(FTransform::Identity);
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus USampleSetInteractionCollisionTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    const AActor* Character = ReadActor(*this, TEXT("Character"));
    const auto* ShouldIgnore = FindFProperty<FBoolProperty>(GetClass(), TEXT("ShouldIgnore"));
    if (IsValid(Character) && ShouldIgnore)
    {
        TInlineComponentArray<UActorComponent*> Components;
        Character->GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            UFunction* Event = Component->FindFunction(TEXT("SetIgnoreActorState"));
            const auto* Ignore = Event ? FindFProperty<FBoolProperty>(Event, TEXT("bShouldIgnore")) : nullptr;
            const auto* Other = Event ? FindFProperty<FObjectPropertyBase>(Event, TEXT("Other Actor")) : nullptr;
            if (Ignore && Other)
            {
                FStructOnScope Args(Event);
                Ignore->SetPropertyValue_InContainer(Args.GetStructMemory(), ShouldIgnore->GetPropertyValue_InContainer(this));
                Other->SetObjectPropertyValue_InContainer(Args.GetStructMemory(), ReadActor(*this, TEXT("Other Actor")));
                Component->ProcessEvent(Event, Args.GetStructMemory());
                break;
            }
        }
    }
    return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus USampleAddInteractionCooldownTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    AActor* Controller = ReadActor(*this, TEXT("AIController"));
    const auto* Map = FindCooldownMap(Controller);
    const auto* Name = FindFProperty<FStrProperty>(GetClass(), TEXT("CooldownName"));
    const UWorld* World = Context.GetWorld();
    if (Map && Name && World)
    {
        FScriptMapHelper Entries(Map, Map->ContainerPtrToValuePtr<void>(Controller));
        const FString Key = Name->GetPropertyValue_InContainer(this);
        const double Expiry = World->GetTimeSeconds() + ReadTaskNumber(*this, TEXT("Cooldown Time"));
        CastFieldChecked<FNumericProperty>(Map->ValueProp)->SetFloatingPointPropertyValue(Entries.FindOrAdd(&Key), Expiry);
    }
    return EStateTreeRunStatus::Succeeded;
}

bool USampleCheckInteractionCooldownCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
    AActor* Controller = ReadActor(*this, TEXT("AIController"));
    const auto* Map = FindCooldownMap(Controller);
    const auto* Name = FindFProperty<FStrProperty>(GetClass(), TEXT("CooldownName"));
    const UWorld* World = Context.GetWorld();
    if (!Map || !Name || !World)
    {
        return true;
    }

    FScriptMapHelper Entries(Map, Map->ContainerPtrToValuePtr<void>(Controller));
    const FString Key = Name->GetPropertyValue_InContainer(this);
    if (const void* Entry = Entries.FindValueFromHash(&Key))
    {
        const double Expiry = CastFieldChecked<FNumericProperty>(Map->ValueProp)->GetFloatingPointPropertyValue(Entry);
        if (Expiry > World->GetTimeSeconds())
        {
            return false;
        }
        Entries.RemovePair(&Key);
    }
    return true;
}

void USampleNpcControllerEvaluator::TreeStart(FStateTreeExecutionContext& Context)
{
    Super::TreeStart(Context);
    AAIController* Controller = UAIBlueprintHelperLibrary::GetAIController(ReadActor(*this, TEXT("Actor")));
    const auto* Output = FindActorProperty(*this, TEXT("AIController"));
    if (Output && IsValid(Controller) && Controller->IsA(Output->PropertyClass))
    {
        Output->SetObjectPropertyValue_InContainer(this, Controller);
    }
}

EStateTreeRunStatus USampleSetNpcInputTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    AActor* Actor = ReadActor(*this, TEXT("Character"));
    const auto* Walk = FindFProperty<FBoolProperty>(GetClass(), TEXT("WantsToWalk"));
    if (!IsValid(Actor) || !Walk)
    {
        return EStateTreeRunStatus::Running;
    }
    const bool bWalk = Walk->GetPropertyValue_InContainer(this);
    if (auto* Cmc = Cast<ASampleCmcCharacter>(Actor))
    {
        Cmc->ApplyNpcWalkIntent(bWalk);
    }
    else if (auto* Mover = Cast<ASampleMoverCharacter>(Actor))
    {
        Mover->ApplyNpcWalkIntent(bWalk);
    }
    else if (UFunction* Function = Actor->FindFunction(TEXT("Set_CharacterInputState")))
    {
        const auto* State = FindFProperty<FStructProperty>(Function, TEXT("Desired Input State"));
        const auto* WalkField = State ? FindFProperty<FBoolProperty>(State->Struct,
            TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3")) : nullptr;
        if (WalkField)
        {
            FStructOnScope Args(Function);
            WalkField->SetPropertyValue_InContainer(State->ContainerPtrToValuePtr<void>(Args.GetStructMemory()), bWalk);
            Actor->ProcessEvent(Function, Args.GetStructMemory());
        }
    }
    return EStateTreeRunStatus::Running;
}
