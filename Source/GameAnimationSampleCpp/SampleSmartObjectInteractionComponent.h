#pragma once

#include "Components/ActorComponent.h"
#include "SampleSmartObjectInteractionComponent.generated.h"

/** Applies capsule collision and correction policy for the sample's authored paired interactions. */
UCLASS(Abstract, Blueprintable, ClassGroup = (Gameplay))
class GAMEANIMATIONSAMPLECPP_API USampleSmartObjectInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * Applies a movement ignore entry to each of the owner's capsules.
     * @param bShouldIgnore True during the paired interaction, false during cleanup.
     * @param OtherActor Actor whose components should be ignored during capsule movement.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    void UpdateInteractionCollision(bool bShouldIgnore, AActor* OtherActor);

    /**
     * Applies the sample's temporary CMC correction override on peers that render the interaction.
     * @param IgnoreCorrections True during the authored interaction window.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Interaction")
    void UpdateInteractionMovementCorrection(bool IgnoreCorrections);
};
