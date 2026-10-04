#pragma once

#include "GameFramework/Actor.h"
#include "SampleSandboxFixtures.generated.h"

class ASampleTeleporterDestination;

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
    virtual void OnConstruction(const FTransform& Transform) override;
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
};
