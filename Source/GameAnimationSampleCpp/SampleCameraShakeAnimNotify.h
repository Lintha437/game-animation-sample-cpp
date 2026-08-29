#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "SampleCameraShakeAnimNotify.generated.h"

/**
 * Emits the sandbox's positional camera shake when an animation reaches this notify.
 *
 * The existing notify subclass retains its authored Shake class and serialized
 * animation references. One native dispatch uses the skeletal mesh owner's position.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleCameraShakeAnimNotify : public UAnimNotify
{
    GENERATED_BODY()

public:
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
