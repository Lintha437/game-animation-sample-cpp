#include "SampleSmartObjectInteractionComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

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
