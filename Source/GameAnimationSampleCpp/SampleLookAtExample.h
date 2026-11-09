#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "SampleLookAtExample.generated.h"

class ASpectatorPawn;

/** Routes the isolated Look At interface's possession and patrol reset buttons. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleLookAtInterface : public UUserWidget
{
    GENERATED_BODY()

protected:
    /** Binds the three authored buttons once per initialized widget instance. */
    virtual void NativeOnInitialized() override;

private:
    /** Requests player possession from the sample's first player controller. */
    UFUNCTION()
    void SpawnDemoPlayer();

    /** Returns the sample's first player controller to its spectator. */
    UFUNCTION()
    void DespawnDemoPlayer();

    /** Restores the selected patrol pawn's authored transform through normal actor movement. */
    UFUNCTION()
    void ResetPatrolPawn();

    /** Reflects the reference interface's visible and collapsed states after a valid toolbar request. */
    void SetPlayerButtonVisibility(bool bPlayerMode);
};

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

    /**
     * Publishes the selected zone before the authored animation and widget callbacks run.
     * @param SelectedPawn Pawn assigned to the selected zone.
     * @param ZoneType Authored zone type sent to the interface, even if the pawn is unsupported.
     * @param Gizmo Optional actor that supplies the look-at target.
     * @return True if the pawn matches the controller's authored character type.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At")
    bool CaptureLookAtZoneSelection(APawn* SelectedPawn, const FString& ZoneType, AActor* Gizmo);

protected:
    /** Applies the scene camera FOV and creates the authored interface for the owning player. */
    virtual void BeginPlay() override;

    /** Removes the locally owned interface before level teardown. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Authored pawn type accepted for zone selection and player-mode possession. */
    UPROPERTY(EditDefaultsOnly, Category = "Sample|Look At")
    TSoftClassPtr<APawn> DemoPlayerClass;

    /** Interface created for the local controller and removed when the controller leaves play. */
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

/** Converts cursor movement and wheel input into the Look At POI target transform. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLookAtGizmo : public AActor
{
    GENERATED_BODY()

public:
    ASampleLookAtGizmo();

    /** Applies one pointer sample. Shift can start control only while the hitbox is hovered. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At")
    void UpdateGizmoControls(bool bShiftDown, FVector2D Cursor, float Wheel);

protected:
    /** Creates the dynamic material and applies the authored idle color. */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Binds the authored hitbox and initializes the tooltip drawer. */
    virtual void BeginPlay() override;

    /** Samples the local player's pointer at the original tick interval. */
    virtual void Tick(float DeltaSeconds) override;

private:
    UFUNCTION()
    void OnGizmoHoverBegin(UPrimitiveComponent* Component);

    UFUNCTION()
    void OnGizmoHoverEnd(UPrimitiveComponent* Component);

    void UpdateGizmoColor(FName ColorName);
};
