#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "SampleMoverCharacter.h"
#include "SampleGameplayAnimNotifyStates.generated.h"

class UCharacterMoverComponent;

/**
 * Queues the Mover mode changes authored by BP_NotifyState_OverrideMovementMode.
 * The imported subclass owns the serialized OverrideMovementMode and EndingMovementMode
 * name fields, including per-montage overrides. Notify instances hold no playback state.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleOverrideMovementModeAnimNotifyState : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    /**
     * Queues the authored override for the next Mover simulation frame.
     * @param MeshComp Mesh owned by SandboxCharacter_Mover_C or a subclass, supplying its authored CharacterMover field.
     * @param Animation Source animation supplied by the engine.
     * @param TotalDuration Authored duration in seconds.
     * @param EventReference Context of the animation event.
     */
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    /**
     * Queues the authored ending mode only if the simulated mode still equals the override.
     * Normal completion and interruption use the same rule. Missing owners or components
     * and owners outside the authored SandboxCharacter_Mover_C hierarchy are ignored.
     * No previous mode is captured, and no network-role filter is applied.
     * @param MeshComp Mesh ending the notify state.
     * @param Animation Source animation supplied by the engine.
     * @param EventReference Context of the animation event.
     */
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};

/** Transient native pawn fixture with the same component field as the authored Mover pawn. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class ASampleNotifyMoverFixture : public ASampleMoverCharacter
{
    GENERATED_BODY()

public:
    ASampleNotifyMoverFixture();

    /** Transient fixture-owned Mover component used to exercise the authored notify class guard. */
    UPROPERTY()
    TObjectPtr<UCharacterMoverComponent> CharacterMover;
};

/**
 * Applies the all-channel collision response authored by BP_NotifyState_OverrideCapsuleCollision.
 * The imported subclass retains its New Response enum and ResetOnEnd boolean, including
 * per-montage values. No component references or mutable playback state are retained.
 */
UCLASS(Abstract, Blueprintable)
class GAMEANIMATIONSAMPLECPP_API USampleOverrideCapsuleCollisionAnimNotifyState : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    /**
     * Sets all collision channels on the mesh owner's primitive root to the authored response.
     * @param MeshComp Mesh whose owner supplies the root component.
     * @param Animation Source animation supplied by the engine.
     * @param TotalDuration Authored duration in seconds.
     * @param EventReference Context of the animation event.
     */
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    /**
     * Restores the CharacterCapsule profile and updates overlaps if ResetOnEnd is true.
     * Missing or non-primitive roots are ignored. Interruption follows the same rule as
     * normal completion, including the intentional option to leave the override active.
     * @param MeshComp Mesh ending the notify state.
     * @param Animation Source animation supplied by the engine.
     * @param EventReference Context of the animation event.
     */
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
