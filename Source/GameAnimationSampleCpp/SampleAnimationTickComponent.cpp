#include "SampleAnimationTickComponent.h"

#include "UObject/UnrealType.h"

USampleAnimationTickComponent::USampleAnimationTickComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
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
    auto* Delegate = FindAuthoredTick();
    if (Delegate && Function && Function->NumParms == 0)
    {
        FScriptDelegate Binding;
        Binding.BindUFunction(&Receiver, Callback);
        Delegate->AddUnique(Binding);
    }
}

void USampleAnimationTickComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (const auto* Delegate = FindAuthoredTick();
        ensureMsgf(Delegate, TEXT("%s requires a zero-parameter Tick delegate"), *GetClass()->GetPathName()))
    {
        Delegate->ProcessDelegate<UObject>(nullptr);
    }
}
