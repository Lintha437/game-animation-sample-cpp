#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "SampleCameraShakeAnimNotify.generated.h"

class UCameraShakeBase;

/**
 * Emits the sandbox's positional camera shake when an animation reaches this notify.
 *
 * The animation event owns its shake class. One native dispatch uses the skeletal mesh owner's position.
 */
UCLASS(Blueprintable, meta=(DisplayName="Camera Shake"))
class GAMEANIMATIONSAMPLECPP_API USampleCameraShakeAnimNotify : public UAnimNotify
{
    GENERATED_BODY()

public:
    /** Shake asset to play. An empty setting produces no effect. */
    UPROPERTY(EditAnywhere, Category="Camera")
    TSubclassOf<UCameraShakeBase> ShakeClass;

    /**
     * Starts the configured shake in the owner's world.
     * @param MeshComp Skeletal mesh firing this notify. Missing owners produce no shake.
     * @param Animation Source sequence or montage supplied by the engine.
     * @param EventReference Animation event context supplied by the engine.
     */
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;

    /** Returns the existing label used by authored animation tracks. */
    virtual FString GetNotifyName_Implementation() const override;
};
