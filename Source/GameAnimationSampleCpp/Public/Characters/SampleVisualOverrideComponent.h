#pragma once

#include "Components/ActorComponent.h"
#include "SampleVisualOverrideComponent.generated.h"

struct FStreamableHandle;

/** Resolves the sandbox's optional visual actor without changing the pawn's movement or animation state. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleVisualOverrideComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Updates tagged child actor components and the source mesh from the authored replicated class. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Visual Override")
    void ApplyVisualOverride();

protected:
    /** Connects the selection event on the authority and requests the initial class. */
    virtual void BeginPlay() override;

    /** Unbinds before teardown and cancels any outstanding class load. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleVisualSelectionChanged(FString VariableName);

    void CompleteVisualLoad(FSoftObjectPath RequestedClass);
    void SetVisualOverride(UClass* Selected);

    /** The single outstanding request for this component. A newer selection cancels it. */
    TSharedPtr<FStreamableHandle> PendingVisualLoad;
    FSoftObjectPath PendingVisualClass;
};
