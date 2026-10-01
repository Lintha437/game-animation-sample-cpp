#include "SampleSandboxObstacles.h"

#include "Components/PrimitiveComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoverComponent.h"
#include "SampleMoverCharacter.h"
#include "UObject/UnrealType.h"

namespace
{
    template <class T>
    T* FindAuthoredComponent(const AActor& Owner, FName Name)
    {
        const auto* Property = FindFProperty<FObjectPropertyBase>(Owner.GetClass(), Name);
        return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(&Owner)) : nullptr;
    }

    double ReadObstacleNumber(const AActor& Owner, FName Name)
    {
        const auto* Property = FindFProperty<FNumericProperty>(Owner.GetClass(), Name);
        return Property && Property->IsFloatingPoint()
            ? Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Owner)) : 0.0;
    }

    bool ReadObstacleFlag(const AActor& Owner, FName Name)
    {
        const auto* Property = FindFProperty<FBoolProperty>(Owner.GetClass(), Name);
        return Property && Property->GetPropertyValue_InContainer(&Owner);
    }
}

void ASampleZeroGravityZone::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (GetWorld()->IsGameWorld())
    {
        if (auto* Zone = FindAuthoredComponent<UPrimitiveComponent>(*this, TEXT("ZeroGZone")))
        {
            Zone->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASampleZeroGravityZone::OnZoneEntered);
            Zone->OnComponentEndOverlap.AddUniqueDynamic(this, &ASampleZeroGravityZone::OnZoneExited);
        }
    }
}

void ASampleZeroGravityZone::OnZoneEntered(UPrimitiveComponent* Zone, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
    if (auto* Pawn = Cast<ASampleMoverCharacter>(OtherActor))
    {
        Pawn->ApplySandboxZeroGravity(true);
    }
}

void ASampleZeroGravityZone::OnZoneExited(UPrimitiveComponent* Zone, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
    if (auto* Pawn = Cast<ASampleMoverCharacter>(OtherActor))
    {
        Pawn->ApplySandboxZeroGravity(false);
    }
}

void ASamplePhysicsObstacle::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Mover = FindAuthoredComponent<UMoverComponent>(*this, TEXT("Mover")))
    {
        Mover->InputProducer = FindAuthoredComponent<UActorComponent>(*this, TEXT("ChaosPathedMovementController"));
    }
}

void ASampleTimedObstacle::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Timeline = FindAuthoredComponent<UTimelineComponent>(*this, TEXT("Timeline")))
    {
        const double Duration = ReadObstacleNumber(*this, TEXT("Duration"));
        Timeline->SetPlayRate(Duration != 0.0 ? 1.0 / Duration : 0.0);
        Timeline->SetLooping(ReadObstacleFlag(*this, TEXT("Loop")));
        if (ReadObstacleFlag(*this, TEXT("AutoStart")))
        {
            UKismetSystemLibrary::Delay(this, ReadObstacleNumber(*this, TEXT("StartDelay")), FLatentActionInfo(
                0, 0, GET_FUNCTION_NAME_STRING_CHECKED(ASampleTimedObstacle, StartObstacleTimeline), this));
        }
    }
}

void ASampleTimedObstacle::StartObstacleTimeline()
{
    if (auto* Timeline = FindAuthoredComponent<UTimelineComponent>(*this, TEXT("Timeline")))
    {
        Timeline->Play();
    }
}
