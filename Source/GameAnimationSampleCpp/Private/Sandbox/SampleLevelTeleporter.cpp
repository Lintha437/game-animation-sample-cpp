#include "Sandbox/SampleLevelTeleporter.h"

#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename ComponentType>
    ComponentType* GetAuthoredComponent(const AActor& Actor, FName Name)
    {
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Actor.GetClass(), Name);
        return Property ? Cast<ComponentType>(Property->GetObjectPropertyValue_InContainer(&Actor)) : nullptr;
    }
}

void ASampleLevelTeleporter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateName();
}

void ASampleLevelTeleporter::BeginPlay()
{
    Super::BeginPlay();
    if (UPrimitiveComponent* Trigger = GetAuthoredComponent<UPrimitiveComponent>(*this, TEXT("Trigger")))
    {
        BoundTrigger = Trigger;
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASampleLevelTeleporter::HandleTriggerOverlap);
    }
}

void ASampleLevelTeleporter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UPrimitiveComponent* Trigger = BoundTrigger.Get())
    {
        Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &ASampleLevelTeleporter::HandleTriggerOverlap);
    }
    BoundTrigger.Reset();
    Super::EndPlay(EndPlayReason);
}

FText ASampleLevelTeleporter::GetDestination() const
{
    const FTextProperty* Property = FindFProperty<FTextProperty>(GetClass(), TEXT("Destination"));
    return Property ? Property->GetPropertyValue_InContainer(this) : FText::GetEmpty();
}

void ASampleLevelTeleporter::UpdateName()
{
    if (UTextRenderComponent* Label = GetAuthoredComponent<UTextRenderComponent>(*this, TEXT("DestinationName")))
    {
        Label->SetText(FText::Format(NSLOCTEXT("SampleLevelTeleporter", "DestinationLabel", "To\r\n{0}"),
            GetDestination()));
    }
}

void ASampleLevelTeleporter::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    UWorld* World = GetWorld();
    if (!World || World->IsNetMode(NM_Client) || !IsValid(OtherActor) || !OtherActor->HasAuthority())
    {
        return;
    }

    const FString Map = TEXT("/Game/Levels/") + GetDestination().ToString();
    if (FPackageName::IsValidLongPackageName(Map) && FPackageName::DoesPackageExist(Map))
    {
        World->ServerTravel(Map + TEXT("?listen"));
    }
}
