#include "SampleCameraShakeAnimNotify.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

void USampleCameraShakeAnimNotify::Notify(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    // Transitional support for the imported child until its animation events are converted.
    const FClassProperty* Legacy = FindFProperty<FClassProperty>(GetClass(), TEXT("Shake"));
    const TSubclassOf<UCameraShakeBase> Shake = Legacy && Legacy->MetaClass->IsChildOf(UCameraShakeBase::StaticClass())
        ? TSubclassOf<UCameraShakeBase>(Cast<UClass>(Legacy->GetObjectPropertyValue_InContainer(this))) : ShakeClass;
    if (Shake)
    {
        APlayerCameraManager::PlayWorldCameraShake(World, Shake, Owner->GetActorLocation(),
            500.0f, 1000.0f, 1.0f, false);
    }
}

FString USampleCameraShakeAnimNotify::GetNotifyName_Implementation() const
{
    return TEXT("CameraShake");
}
