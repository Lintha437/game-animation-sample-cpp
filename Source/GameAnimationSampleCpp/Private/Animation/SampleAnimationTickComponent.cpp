#include "Animation/SampleAnimationTickComponent.h"

#include "UObject/UnrealType.h"

USampleAnimationTickComponent::USampleAnimationTickComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_DuringPhysics;
    PrimaryComponentTick.EndTickGroup = TG_PrePhysics;
}

FMulticastScriptDelegate* USampleAnimationTickComponent::FindAuthoredTick()
{
    const auto* Property = FindFProperty<FMulticastInlineDelegateProperty>(GetClass(), TEXT("Tick"));
    return Property && Property->SignatureFunction && Property->SignatureFunction->NumParms == 0
        ? Property->ContainerPtrToValuePtr<FMulticastScriptDelegate>(this) : nullptr;
}

void USampleAnimationTickComponent::BindTick(UObject& Receiver, FName Callback)
{
    const UFunction* Function = Receiver.FindFunction(Callback);
    if (!Function || Function->NumParms != 0)
    {
        return;
    }

    FScriptDelegate Binding;
    Binding.BindUFunction(&Receiver, Callback);
    if (auto* Delegate = FindAuthoredTick())
    {
        Delegate->AddUnique(Binding);
    }
    else
    {
        OnTick.AddUnique(Binding);
    }
}

void USampleAnimationTickComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (const auto* Delegate = FindAuthoredTick())
    {
        Delegate->ProcessDelegate<UObject>(nullptr);
        return;
    }
    OnTick.Broadcast();
}
