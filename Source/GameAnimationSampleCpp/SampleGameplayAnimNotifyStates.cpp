#include "SampleGameplayAnimNotifyStates.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/EngineTypes.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace
{
    UCharacterMoverComponent* ResolveAuthoredMover(const USkeletalMeshComponent* MeshComp)
    {
        const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
        // The authored cast accepts this generated class and its descendants. An owner
        // already holds its class hierarchy alive, so no asset load is necessary here.
        for (const UClass* Class = Owner ? Owner->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
        {
            if (Class->GetPathName() == TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"))
            {
                const auto* Property = FindFProperty<FObjectPropertyBase>(Class, TEXT("CharacterMover"));
                return Property ? Cast<UCharacterMoverComponent>(Property->GetObjectPropertyValue_InContainer(Owner)) : nullptr;
            }
        }
        return nullptr;
    }

    UPrimitiveComponent* ResolvePrimitiveRoot(const USkeletalMeshComponent* MeshComp)
    {
        const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
        return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
    }

    bool ReadCollisionResponse(const UObject& Notify, ECollisionResponse& Response)
    {
        const FProperty* Property = FindFProperty<FProperty>(Notify.GetClass(), TEXT("New Response"));
        int64 Value = INDEX_NONE;
        if (const auto* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum == StaticEnum<ECollisionResponse>())
        {
            Value = Byte->GetPropertyValue_InContainer(&Notify);
        }
        else if (const auto* Enum = CastField<FEnumProperty>(Property); Enum && Enum->GetEnum() == StaticEnum<ECollisionResponse>())
        {
            Value = Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(&Notify));
        }

        if (!ensureMsgf(Value >= ECR_Ignore && Value <= ECR_Block,
            TEXT("%s requires a valid ECollisionResponse property named New Response"), *Notify.GetClass()->GetPathName()))
        {
            return false;
        }
        Response = static_cast<ECollisionResponse>(Value);
        return true;
    }
}

ASampleNotifyMoverFixture::ASampleNotifyMoverFixture()
{
    auto* Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    SetRootComponent(Capsule);
    auto* Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Capsule);
    CharacterMover = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("CharacterMover"));
}

void USampleOverrideMovementModeAnimNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (UCharacterMoverComponent* Mover = ResolveAuthoredMover(MeshComp))
    {
        const auto* Override = FindFProperty<FNameProperty>(GetClass(), TEXT("OverrideMovementMode"));
        if (ensureMsgf(Override, TEXT("%s requires a name property named OverrideMovementMode"), *GetClass()->GetPathName()))
        {
            Mover->QueueNextMode(Override->GetPropertyValue_InContainer(this), false);
        }
    }
}

void USampleOverrideMovementModeAnimNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (UCharacterMoverComponent* Mover = ResolveAuthoredMover(MeshComp))
    {
        const auto* Override = FindFProperty<FNameProperty>(GetClass(), TEXT("OverrideMovementMode"));
        const auto* Ending = FindFProperty<FNameProperty>(GetClass(), TEXT("EndingMovementMode"));
        if (ensureMsgf(Override && Ending, TEXT("%s requires both authored movement mode name properties"), *GetClass()->GetPathName()) &&
            Mover->GetMovementModeName() == Override->GetPropertyValue_InContainer(this))
        {
            Mover->QueueNextMode(Ending->GetPropertyValue_InContainer(this), false);
        }
    }
}

void USampleOverrideCapsuleCollisionAnimNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (UPrimitiveComponent* Root = ResolvePrimitiveRoot(MeshComp))
    {
        ECollisionResponse Response;
        if (ReadCollisionResponse(*this, Response))
        {
            Root->SetCollisionResponseToAllChannels(Response);
        }
    }
}

void USampleOverrideCapsuleCollisionAnimNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    const auto* Reset = FindFProperty<FBoolProperty>(GetClass(), TEXT("ResetOnEnd"));
    if (ensureMsgf(Reset, TEXT("%s requires a boolean property named ResetOnEnd"), *GetClass()->GetPathName()) &&
        Reset->GetPropertyValue_InContainer(this))
    {
        if (UPrimitiveComponent* Root = ResolvePrimitiveRoot(MeshComp))
        {
            Root->SetCollisionProfileName(TEXT("CharacterCapsule"), true);
        }
    }
}
