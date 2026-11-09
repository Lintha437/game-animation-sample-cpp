#include "SampleSmartObjectInteractionComponent.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameplayStateTreeBlueprintFunctionLibrary.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoverComponent.h"
#include "SmartObjectBlueprintFunctionLibrary.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "StateTree.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    const FName PlayerInteractionState(TEXT("PlayerIsInteracting"));

    void CacheInteractionComponent(UActorComponent& Receiver, FName Name, UActorComponent* Component)
    {
        const auto* Property = FindFProperty<FObjectPropertyBase>(Receiver.GetClass(), Name);
        if (Property && (!Component || Component->IsA(Property->PropertyClass)))
        {
            Property->SetObjectPropertyValue_InContainer(&Receiver, Component);
        }
    }

    FSmartObjectSlotHandle SelectPlayerSlot(const USmartObjectSubsystem& Subsystem,
        const TArray<FSmartObjectRequestResult>& Results, const FVector& Origin)
    {
        FSmartObjectSlotHandle Selected;
        double BestDistanceSquared = FMath::Square(300.0);
        for (const FSmartObjectRequestResult& Result : Results)
        {
            const TOptional<FTransform> Transform = Subsystem.GetSlotTransform(Result);
            if (Transform && Subsystem.GetSlotState(Result.SlotHandle) == ESmartObjectSlotState::Free)
            {
                const double DistanceSquared = FVector::DistSquared(Origin, Transform->GetLocation());
                if (DistanceSquared < BestDistanceSquared)
                {
                    Selected = Result.SlotHandle;
                    BestDistanceSquared = DistanceSquared;
                }
            }
        }
        return Selected;
    }

    bool SendPlayerInteraction(UStateTreeComponent& Tree, UScriptStruct& Type, FGameplayTag Tag,
        AActor& Target, const FSmartObjectSlotHandle& Slot)
    {
        const FObjectPropertyBase* ActorField = nullptr;
        const FStructProperty* SlotField = nullptr;
        for (TFieldIterator<FProperty> It(&Type); It; ++It)
        {
            if (const auto* Actor = CastField<FObjectPropertyBase>(*It);
                Actor && Actor->PropertyClass->IsChildOf(AActor::StaticClass()))
            {
                if (ActorField || !Target.IsA(Actor->PropertyClass))
                {
                    return false;
                }
                ActorField = Actor;
            }
            if (const auto* Struct = CastField<FStructProperty>(*It); Struct && Struct->Struct == FSmartObjectSlotHandle::StaticStruct())
            {
                if (SlotField)
                {
                    return false;
                }
                SlotField = Struct;
            }
        }
        if (!ActorField || !SlotField)
        {
            return false;
        }

        FStructOnScope Payload(&Type);
        uint8* Data = Payload.GetStructMemory();
        ActorField->SetObjectPropertyValue_InContainer(Data, &Target);
        *SlotField->ContainerPtrToValuePtr<FSmartObjectSlotHandle>(Data) = Slot;
        Tree.SendStateTreeEvent(Tag, FConstStructView(&Type, Data));
        return true;
    }
}

void USampleSmartObjectInteractionComponent::UpdateInteractionCollision(bool bShouldIgnore, AActor* OtherActor)
{
    const AActor* Owner = GetOwner();
    if (!Owner || !IsValid(OtherActor))
    {
        return;
    }

    TInlineComponentArray<UCapsuleComponent*> Capsules;
    Owner->GetComponents(Capsules);
    for (UCapsuleComponent* Capsule : Capsules)
    {
        Capsule->IgnoreActorWhenMoving(OtherActor, bShouldIgnore);
    }
}

void USampleSmartObjectInteractionComponent::UpdateInteractionMovementCorrection(bool IgnoreCorrections)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    const auto* Property = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("Character Movement Component"));
    auto* Movement = Property ? Cast<UCharacterMovementComponent>(Property->GetObjectPropertyValue_InContainer(this)) : nullptr;
    if (Movement)
    {
        Movement->bIgnoreClientMovementErrorChecksAndCorrection = IgnoreCorrections;
    }
}

void USampleSmartObjectInteractionComponent::CacheInteractionComponents()
{
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    CacheInteractionComponent(*this, TEXT("Owner Skeletal Mesh"), Owner->FindComponentByClass<USkeletalMeshComponent>());
    if (auto* Movement = Owner->FindComponentByClass<UCharacterMovementComponent>())
    {
        CacheInteractionComponent(*this, TEXT("Character Movement Component"), Movement);
    }
    else
    {
        CacheInteractionComponent(*this, TEXT("Mover Component"), Owner->FindComponentByClass<UMoverComponent>());
    }
}

bool USampleSmartObjectInteractionComponent::TryStartPlayerInteraction()
{
    AActor* Owner = GetOwner();
    const auto* IsInteracting = FindFProperty<FBoolProperty>(GetClass(), PlayerInteractionState);
    const auto* TreeProperty = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("StateTreeComponent"));
    if (!IsValid(Owner) || !IsInteracting || !TreeProperty || UAIBlueprintHelperLibrary::GetAIController(Owner))
    {
        return false;
    }

    auto* Tree = Cast<UStateTreeComponent>(TreeProperty->GetObjectPropertyValue_InContainer(this));
    if (!IsValid(Tree))
    {
        if (!UGameplayStateTreeBlueprintFunctionLibrary::RunStateTree(Owner, PlayerInteractionTree.LoadSynchronous()))
        {
            return false;
        }
        Tree = Owner->FindComponentByClass<UStateTreeComponent>();
        CacheInteractionComponent(*this, TEXT("StateTreeComponent"), Tree);
    }
    const auto* Subsystem = USmartObjectSubsystem::GetCurrent(GetWorld());
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("SmartObject.ObjectType.Player"), false);
    if (!Tree || !Subsystem || !Tag.IsValid())
    {
        return false;
    }

    const FVector Origin = Owner->GetActorLocation();
    const FVector Forward = Owner->GetActorForwardVector();
    FHitResult Hit;
    if (!UKismetSystemLibrary::BoxTraceSingleForObjects(Owner, Origin + Forward * 15.0, Origin + Forward * 200.0,
        FVector(1.0, 64.0, 86.0), Owner->GetActorRotation(), {EObjectTypeQuery::ObjectTypeQuery1}, false, {Owner},
        EDrawDebugTrace::None, Hit, true))
    {
        return false;
    }

    AActor* Target = Hit.GetActor();
    FSmartObjectRequestFilter Filter;
    Filter.UserTags.AddTag(Tag);
    Filter.BehaviorDefinitionClasses.Add(UGameplayInteractionSmartObjectBehaviorDefinition::StaticClass());
    TArray<FSmartObjectRequestResult> Results;
    if (!IsValid(Target) || !USmartObjectBlueprintFunctionLibrary::FindSmartObjectsInActor(Filter, Target, Results, Owner))
    {
        return false;
    }
    const FSmartObjectSlotHandle Slot = SelectPlayerSlot(*Subsystem, Results, Origin);
    if (Target->GetActorTransform().InverseTransformPosition(Origin).X <= 0.0 ||
        IsInteracting->GetPropertyValue_InContainer(this) || !Slot.IsValid())
    {
        IsInteracting->SetPropertyValue_InContainer(this, false);
        return false;
    }

    UScriptStruct* Payload = PlayerInteractionPayload.LoadSynchronous();
    if (!Payload || !SendPlayerInteraction(*Tree, *Payload, Tag, *Target, Slot))
    {
        return false;
    }
    IsInteracting->SetPropertyValue_InContainer(this, true);
    return true;
}

void USampleSmartObjectInteractionComponent::BindPlayerInteractionInput(UEnhancedInputComponent& Input)
{
    if (!bUseNativePlayerInteractionInput)
    {
        return;
    }
    const auto Bind = [this, &Input](const TCHAR* Path, void (USampleSmartObjectInteractionComponent::*Handler)())
    {
        if (const UInputAction* Action = LoadObject<UInputAction>(nullptr, Path))
        {
            Input.BindAction(Action, ETriggerEvent::Triggered, this, Handler);
        }
    };
    Bind(TEXT("/Game/Input/IA_Interact.IA_Interact"), &USampleSmartObjectInteractionComponent::HandlePlayerInteractionInput);
    Bind(TEXT("/Game/Input/IA_Move.IA_Move"), &USampleSmartObjectInteractionComponent::RequestPlayerInteractionCancellation);
}

void USampleSmartObjectInteractionComponent::HandlePlayerInteractionInput()
{
    if (TryStartPlayerInteraction())
    {
        OpenPlayerInteractionGate();
    }
}

void USampleSmartObjectInteractionComponent::ClearPlayerInteractionState()
{
    if (const auto* State = FindFProperty<FBoolProperty>(GetClass(), PlayerInteractionState))
    {
        State->SetPropertyValue_InContainer(this, false);
    }
}

void USampleSmartObjectInteractionComponent::GetNpcApproachMetrics(
    const FTransform& Destination, double& Distance, double& Angle) const
{
    Distance = 0.0;
    Angle = 0.0;
    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const FVector Position = Owner->GetActorLocation();
    Distance = FVector::Dist(Position, Destination.GetLocation());
    const FVector Direction = Destination.InverseTransformPosition(Position).GetSafeNormal(0.0001);
    Angle = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
}
