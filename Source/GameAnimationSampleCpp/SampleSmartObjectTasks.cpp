#include "SampleSmartObjectTasks.h"

#include "GameFramework/Actor.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "SmartObjectComponent.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "UObject/UnrealType.h"

namespace
{
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
    if (!Claim || !Slot)
    {
        return EStateTreeRunStatus::Failed;
    }
    *Claim = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(
        Context.GetWorld(), *Slot, ReadActor(*this, TEXT("Actor")));
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus USampleReleaseSmartObjectTask::EnterState(FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition)
{
    const auto* Claim = FindStruct<FSmartObjectClaimHandle>(*this, TEXT("Claimed Handle"), FSmartObjectClaimHandle::StaticStruct());
    return Claim && Claim->IsValid() && USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(Context.GetWorld(), *Claim)
        ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
