#pragma once

#include "GameFramework/PlayerController.h"
#include "SampleLookAtExample.generated.h"

class ASpectatorPawn;
class UUserWidget;

/** Owns player possession and the local interface in the isolated Look At POI example. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLookAtController : public APlayerController
{
    GENERATED_BODY()

public:
    ASampleLookAtController();

    /** Spawns or reuses the demo player and removes the obsolete zone target gizmos. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At")
    void EnterLookAtPlayerMode();

    /** Returns to the existing spectator, frees the demo player, and deselects the zones. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At")
    void EnterLookAtSpectatorMode();

protected:
    /** Applies the scene camera FOV and creates the authored interface for the owning player. */
    virtual void BeginPlay() override;

    /** Removes the locally owned interface before level teardown. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Look At")
    TSoftClassPtr<APawn> DemoPlayerClass;

    UPROPERTY(EditDefaultsOnly, Category = "Sample|Look At")
    TSoftClassPtr<UUserWidget> DemoWidgetClass;

private:
    TWeakObjectPtr<ASpectatorPawn> PreviousSpectator;
    TWeakObjectPtr<UUserWidget> DemoWidget;
};
