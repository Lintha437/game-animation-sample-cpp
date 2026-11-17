#pragma once

#include "GameFramework/Actor.h"
#include "SampleSandboxFixtures.generated.h"

class ASampleTeleporterDestination;
class ASampleLevelVisuals;

/**
 * Updates the transform of an authored sandbox spectator camera.
 * The derived asset owns the camera component, watched actor and follow/look settings.
 */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleStillCamera : public AActor
{
    GENERATED_BODY()

public:
    /** Enables the tick used to aim at and follow the assigned actor. */
    ASampleStillCamera();

    /**
     * Aims from the current camera position before applying the target's translation delta.
     * A valid target refreshes the saved location even when following is disabled.
     * @param DeltaSeconds Time supplied by the actor tick.
     */
    virtual void Tick(float DeltaSeconds) override;
};

/** Routes the authored trigger to a destination in the current world. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleTeleporterSender : public AActor
{
    GENERATED_BODY()

public:
    /** Reads the destination stored on this instance of the original sender asset. */
    ASampleTeleporterDestination* GetDestination() const;

    /** Displays the destination's configured name or the missing-destination label. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Teleporter")
    void UpdateName();

    /** Copies destination color to the plate and direction pointer materials. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Teleporter")
    void UpdateColor();

    /** Faces the destination on the horizontal plane and clears pitch and roll. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Teleporter")
    void UpdateRotation();

    /** Matches the destination plate's width and uses the sample plate thickness. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Teleporter")
    void UpdateScale();

protected:
    /** Refreshes the label, materials and pose from the assigned destination. */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Binds the authored trigger after the asset's components are initialized. */
    virtual void PostInitializeComponents() override;

private:
    UFUNCTION()
    void OnTriggerEntered(UPrimitiveComponent* Trigger, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};

/** Owns the destination label, plate presentation, and authored arrival point. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleTeleporterDestination : public AActor
{
    GENERATED_BODY()

protected:
    /** Refreshes the destination and linked senders when the construction script reruns. */
    virtual void OnConstruction(const FTransform& Transform) override;
};

/** Registers the authored dummy with nearby sandbox pawns through their existing target interface. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleTargetDummy : public AActor
{
    GENERATED_BODY()

protected:
    /** Binds both overlap edges so nearby pawns can maintain their target candidates. */
    virtual void PostInitializeComponents() override;

private:
    UFUNCTION()
    void OnTriggerEntered(UPrimitiveComponent* Trigger, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

    UFUNCTION()
    void OnTriggerExited(UPrimitiveComponent* Trigger, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex);

    /** Calls the Blueprint interface while preserving any implementation supplied by the receiver. */
    void NotifyTargetReceiver(AActor* OtherActor, FName FunctionName);
};

/** Dispatches a level button's authored action and debounces physical overlap presses. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLevelButton : public AActor
{
    GENERATED_BODY()

public:
    /**
     * Broadcasts the existing ButtonPressed delegate, then dispatches the configured command
     * when enabled. Explicit presses bypass the overlap cooldown.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Button")
    void SimulatePress();

protected:
    /** Applies the per-instance text, material color, and plate scale. */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Registers the overlap handler once the authored button trigger is ready. */
    virtual void PostInitializeComponents() override;

private:
    UFUNCTION()
    void OnTriggerEntered(UPrimitiveComponent* Trigger, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

    /** Owns the overlap-only cooldown in world time. Further overlaps do not extend it. */
    double NextOverlapPressTime = 0.0;
};

/** Moves a sandbox block relative to the authored starting transform. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLevelBlock : public AActor
{
    GENERATED_BODY()

public:
    /** Samples the configured translation, rotation, and scale offsets from the starting transform. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Level Block")
    void RandomizeBlockOffset();

    /** Restores the construction-time transform and refreshes the height label. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Level Block")
    void ResetBlockOffset();

    /** Uses the configured label or the current block height in meters. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Level Block")
    void RefreshBlockText();

    /**
     * Rebuilds the traversal spline list and opposite pairs from the four authored ledges.
     * Called after parent construction. The spline components are owned by the block.
     */
    UFUNCTION(BlueprintCallable, Category = "Sample|Level Block")
    void RebuildTraversalLedges();

    /** Applies the active style's material group to a block that opts into level colors. */
    void ApplyLevelStyle(const ASampleLevelVisuals& Visuals);

protected:
    /** Creates the dynamic grid material and captures the baseline for offset changes. */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Binds the placed randomization button once the block's components are ready. */
    virtual void PostInitializeComponents() override;

private:
    void ApplyGridMaterial(const UScriptStruct& Type, const void* Values);
    void BindRandomizeButton();

    TWeakObjectPtr<ASampleLevelButton> BoundRandomizeButton;
};

/** Applies the current authored style to blocks, fog, decal, and landscape. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleLevelVisuals : public AActor
{
    GENERATED_BODY()

public:
    /** Makes the placed visual controller available to every network client. */
    ASampleLevelVisuals();

    /** Saves the requested index and applies the style selected within the authored array bounds. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Level Visuals")
    void SetLevelStyle(int32 Index);

    /** Refreshes all presentation consumers from the current style. */
    UFUNCTION(BlueprintCallable, Category = "Sample|Level Visuals")
    void UpdateVisuals();

    /** Looks up an authored grid group. The returned values remain owned by the style array. */
    const void* FindGridSettings(FName Group, const UScriptStruct*& OutType) const;

    /** Replicates the authoritative selection, including to clients joining after a style change. */
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    /** Applies the selected style when the placed actor is constructed or edited. */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Reapplies the style after the level and its presentation actors enter play. */
    virtual void BeginPlay() override;

private:
    /** Mirrors the Blueprint-authored StyleIndex once the server enters play. */
    UPROPERTY(ReplicatedUsing = OnRep_StyleIndex)
    int32 ReplicatedStyleIndex = INDEX_NONE;

    /** Copies a received server selection into the original Blueprint field and refreshes presentation. */
    UFUNCTION()
    void OnRep_StyleIndex();

    const void* FindStyle(const UScriptStruct*& OutType) const;
};
