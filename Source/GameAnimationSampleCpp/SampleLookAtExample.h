#pragma once

#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
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

/** Owns selection and target-gizmo lifetime for an authored Look At POI zone. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLookAtZone : public AActor
{
    GENERATED_BODY()

public:
    ASampleLookAtZone();

    /** Selects this zone and hands its pawn and gizmo to the existing animation callback. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At")
    void SelectLookAtZone();

    /** Clears selection, destroys the owned gizmo, and disables the assigned pawn's look-at flag. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At")
    void DeselectLookAtZone();

protected:
    /** Binds the meshes and hitbox authored for this zone to native pointer handlers. */
    virtual void BeginPlay() override;

    /** Follows the assigned pawn with the active gizmo after refreshing the authored tooltip. */
    virtual void Tick(float DeltaSeconds) override;

    /** Optional gizmo class used by zones that direct the controller's look-at target. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Look At")
    TSoftClassPtr<AActor> TargetGizmoClass;

private:
    UFUNCTION()
    void OnZoneHoverBegin(UPrimitiveComponent* Component);

    UFUNCTION()
    void OnZoneHoverEnd(UPrimitiveComponent* Component);

    UFUNCTION()
    void OnZoneClicked(UPrimitiveComponent* Component, FKey Button);

    void RefreshZoneColor();
    void ApplyZoneColor(const FLinearColor& Color);
    FVector GetTargetGizmoPosition() const;
};
