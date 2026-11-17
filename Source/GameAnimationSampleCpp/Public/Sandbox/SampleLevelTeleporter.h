#pragma once

#include "GameFramework/Actor.h"
#include "SampleLevelTeleporter.generated.h"

/**
 * Runs the sample's level portal using the existing placed actor configuration.
 *
 * The authored subclass supplies the Trigger collision component, DestinationName
 * text component, and Destination text. These serialized values remain owned by
 * the local asset and placed instances.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLevelTeleporter : public AActor
{
    GENERATED_BODY()

public:
    /**
     * Refreshes the existing world-space label from the destination's authored text.
     * Missing label components have no effect during reconstruction.
     */
    UFUNCTION(BlueprintCallable, Category = "Level Teleporter")
    void UpdateName();

    /** Refreshes the label after Blueprint component templates have been instantiated. */
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    /** Binds the trigger for this instance and retains a weak reference for cleanup. */
    virtual void BeginPlay() override;

    /** Removes the trigger binding before the actor leaves play. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    FText GetDestination() const;

    TWeakObjectPtr<UPrimitiveComponent> BoundTrigger;
};
