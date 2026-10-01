#pragma once

#include "GameFramework/Actor.h"
#include "SampleSandboxObstacles.generated.h"

/** Binds the imported zone component to the sandbox ragdoll gravity policy. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleZeroGravityZone : public AActor
{
    GENERATED_BODY()

protected:
    /** Binds overlaps after the authored component has been instantiated and registered. */
    virtual void PostInitializeComponents() override;

private:
    UFUNCTION()
    void OnZoneEntered(UPrimitiveComponent* Zone, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

    UFUNCTION()
    void OnZoneExited(UPrimitiveComponent* Zone, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);
};

/** Connects a sandbox physics obstacle's authored path controller to its Mover component. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASamplePhysicsObstacle : public AActor
{
    GENERATED_BODY()

protected:
    /** Installs the existing path input producer without creating or replacing components. */
    virtual void BeginPlay() override;
};

/** Applies instance timing settings to the obstacle's authored timeline. */
UCLASS(Blueprintable)
class GAMEANIMATIONSAMPLECPP_API ASampleTimedObstacle : public AActor
{
    GENERATED_BODY()

protected:
    /** Configures the timeline and schedules playback only for instances with automatic start enabled. */
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void StartObstacleTimeline();
};
