#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "SampleLookAtExample.generated.h"

class ASpectatorPawn;
class UAnimInstance;
class UMediaSource;

/** Routes possession, patrol reset and information-panel behavior in the isolated Look At interface. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleLookAtInterface : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Seeds gesture options from the widget map and applies its original network startup presentation. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    void InitializeLookAtPresentation();

    /** Toggles the information panel and forwards Play to its authored media player. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    void ToggleLookAtAbout();

    /** Collapses the information panel, then pauses and rewinds its authored media player. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    void CloseLookAtAbout();

    /**
     * Updates the selected page in the order used by the imported widget and opens its authored media.
     * @param Title Localized label for the About button.
     * @param PageIndex Slot in the existing widget switcher.
     * @param Source Media source selected by the authored zone. The widget retains it in its existing field.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    void ApplyLookAtAboutPage(FText Title, int32 PageIndex, UMediaSource* Source);

protected:
    /** Restores the authored presentation whenever the widget is constructed. */
    virtual void NativeConstruct() override;

    /** Binds the authored About and Close buttons once when the widget is initialized. */
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

    /**
     * Handles the authored zone event. Always captures the requested zone type.
     * For an accepted pawn, enables controller look-at before dispatching the authored continuation.
     * @param SelectedPawn Pawn supplied by the zone event.
     * @param ZoneType Authored zone type.
     * @param Gizmo Optional target actor supplied by the zone event.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    void RouteLookAtZoneSelection(APawn* SelectedPawn, const FString& ZoneType, AActor* Gizmo);

    /** Resolves the authored gizmo offset, including the zero target when the optional gizmo is absent. */
    UFUNCTION(BlueprintPure, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    FVector ResolveLookAtGizmoTarget(AActor* Gizmo) const;

    /**
     * Notifies the authored Widget after the Blueprint's fixed animation cast and enable write.
     * @param Widget The controller's authored Level Widget receiver.
     * @param ZoneType The stored selected zone type.
     * @param AnimInstance The exact animation instance accepted by the authored cast.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Look At", meta = (BlueprintInternalUseOnly = "true"))
    void NotifyLookAtZoneSelected(UUserWidget* Widget, const FString& ZoneType, UAnimInstance* AnimInstance);

protected:
    /** Runs the asset-owned target, animation cast and widget presentation after controller state is published. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Sample|Look At")
    void ApplyLookAtZoneSelection();

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
