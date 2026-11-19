#pragma once

#include "Characters/SampleMoverCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Utilities/SamplePropertyAccess.h"

namespace SampleMoverInternal
{
    inline constexpr uint8 MoverGaitWalk = 0;
    inline constexpr uint8 MoverGaitRun = 1;
    inline constexpr uint8 MoverGaitSprint = 2;

    inline constexpr uint8 FaceMovement = 0;
    inline constexpr uint8 StrafeRotationMode = 1;
    inline constexpr uint8 AimRotationMode = 2;

    inline const FName MoverGaitField(TEXT("Gait_5_13BB29FB446593C2412547A541C4C00B"));
    inline const FName MoverPlayerInputState(TEXT("PlayerInputState"));
    inline const FName MoverSprintIntent(TEXT("WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936"));
    inline const FName MoverWalkIntent(TEXT("WantsToWalk_3_78963B154975E3BDE244A3BE438473E3"));
    inline const FName MoverStrafeIntent(TEXT("WantsToStrafe_5_64E842184F7691CD179DFCBA7158A4D9"));
    inline const FName MoverAimIntent(TEXT("WantsToAim_7_3A4AA0AA4DE529A3B213E68BECD246F8"));
    inline const FName MoverCrouchIntent(TEXT("WantsToCrouch_9_47D0791142F36BB93FFD9EA12F96F2B9"));
    inline const FName MovementSectorField(TEXT("MovementDirection_2_19B243824010A37C0D54A7A28A5C6EFA"));
    inline const FName DirectionalOffsetField(TEXT("RotationOffset_14_9438753848B6A37A4F76569318195C60"));

    inline const FName RagdollMode(TEXT("Ragdoll"));
    inline const FName RagdollTransformField(TEXT("RagdollTransform_33_5FE542414D593629259855B867DEA655"));
    inline const FName DefaultRagdollProfile(TEXT("DefaultPhysicsProfile"));
    inline const FName KinematicRagdollProfile(TEXT("Kinematic"));
    inline const FName PhysicalAnimationProfile(TEXT("PhysicalAnimation"));
    inline const FName CapsuleProfile(TEXT("CharacterCapsule"));
    inline const FName MoverJumpEdge(TEXT("Jump_JustPressed"));

    enum class EAuthoredMovementMode : uint8
    {
        Grounded,
        Airborne,
        Sliding,
        Traversing,
        Ragdoll,
        Flying,
    };

    inline USkeletalMeshComponent* GetRagdollMesh(const ASampleMoverCharacter& Pawn)
    {
        const auto* Mesh = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), TEXT("SkeletalMesh"));
        return Mesh ? Cast<USkeletalMeshComponent>(Mesh->GetObjectPropertyValue_InContainer(&Pawn)) : nullptr;
    }

    inline UCapsuleComponent* GetRagdollCapsule(const ASampleMoverCharacter& Pawn)
    {
        const auto* Capsule = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), TEXT("Capsule"));
        return Capsule ? Cast<UCapsuleComponent>(Capsule->GetObjectPropertyValue_InContainer(&Pawn)) : nullptr;
    }

    inline const FTransform* FindRagdollTransform(const FStructProperty* Snapshot, const ASampleMoverCharacter& Pawn)
    {
        const auto* Field = Snapshot ? FindFProperty<FStructProperty>(Snapshot->Struct, RagdollTransformField) : nullptr;
        return Field && Field->Struct == TBaseStructure<FTransform>::Get()
            ? Field->ContainerPtrToValuePtr<FTransform>(Snapshot->ContainerPtrToValuePtr<void>(&Pawn))
            : nullptr;
    }
}
