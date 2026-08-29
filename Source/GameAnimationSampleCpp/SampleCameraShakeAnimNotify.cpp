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

    static const FName ShakePropertyName(TEXT("Shake"));
    const FClassProperty* Property = FindFProperty<FClassProperty>(GetClass(), ShakePropertyName);
    if (!ensureMsgf(Property && Property->MetaClass->IsChildOf(UCameraShakeBase::StaticClass()),
        TEXT("%s requires a camera shake class property named Shake"), *GetClass()->GetPathName()))
    {
        return;
    }

    const TSubclassOf<UCameraShakeBase> Shake = Cast<UClass>(Property->GetObjectPropertyValue_InContainer(this));
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
