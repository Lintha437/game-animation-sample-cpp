#include "SampleAnimationDrawContext.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

FRotator USampleAnimationDrawContext::ResolveDrawRotation(const UAnimInstance* AnimInstance)
{
    if (!IsValid(AnimInstance))
    {
        return FRotator::ZeroRotator;
    }

    const USkeletalMeshComponent* OwningComponent = AnimInstance->GetOwningComponent();
    if (!IsValid(OwningComponent))
    {
        return FRotator::ZeroRotator;
    }

    const APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(AnimInstance, 0);

    return IsValid(PlayerController)
        ? PlayerController->GetControlRotation()
        : FRotator::ZeroRotator;
}
