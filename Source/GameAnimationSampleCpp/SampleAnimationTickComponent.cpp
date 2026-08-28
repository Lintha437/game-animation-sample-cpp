#include "SampleAnimationTickComponent.h"

#include "UObject/UnrealType.h"

USampleAnimationTickComponent::USampleAnimationTickComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USampleAnimationTickComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    static const FName TickDelegateName(TEXT("Tick"));
    const FMulticastInlineDelegateProperty* Property =
        FindFProperty<FMulticastInlineDelegateProperty>(GetClass(), TickDelegateName);
    if (!ensureMsgf(Property && Property->SignatureFunction && Property->SignatureFunction->NumParms == 0,
        TEXT("%s requires a zero-parameter Tick delegate"), *GetClass()->GetPathName()))
    {
        return;
    }

    const FMulticastScriptDelegate* Delegate = Property->GetMulticastDelegate(
        Property->ContainerPtrToValuePtr<void>(this));
    Delegate->ProcessDelegate<UObject>(nullptr);
}
