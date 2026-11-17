#include "Animation/SampleGameplayAnimNotifyStates.h"

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
        int64 Value = Response;
        if (const auto* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum == StaticEnum<ECollisionResponse>())
        {
            Value = Byte->GetPropertyValue_InContainer(&Notify);
        }
        else if (const auto* Enum = CastField<FEnumProperty>(Property); Enum && Enum->GetEnum() == StaticEnum<ECollisionResponse>())
        {
            Value = Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(&Notify));
        }
        else if (Property)
        {
            return false;
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
        Mover->QueueNextMode(Override ? Override->GetPropertyValue_InContainer(this) : ModeOnBegin, false);
    }
}

void USampleOverrideMovementModeAnimNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    if (UCharacterMoverComponent* Mover = ResolveAuthoredMover(MeshComp))
    {
        const auto* Override = FindFProperty<FNameProperty>(GetClass(), TEXT("OverrideMovementMode"));
        const auto* Ending = FindFProperty<FNameProperty>(GetClass(), TEXT("EndingMovementMode"));
        if (Mover->GetMovementModeName() == (Override ? Override->GetPropertyValue_InContainer(this) : ModeOnBegin))
        {
            Mover->QueueNextMode(Ending ? Ending->GetPropertyValue_InContainer(this) : ModeOnEnd, false);
        }
    }
}

void USampleOverrideCapsuleCollisionAnimNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (UPrimitiveComponent* Root = ResolvePrimitiveRoot(MeshComp))
    {
        ECollisionResponse EffectiveResponse = Response;
        if (ReadCollisionResponse(*this, EffectiveResponse))
        {
            Root->SetCollisionResponseToAllChannels(EffectiveResponse);
        }
    }
}

void USampleOverrideCapsuleCollisionAnimNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    const auto* Reset = FindFProperty<FBoolProperty>(GetClass(), TEXT("ResetOnEnd"));
    if (Reset ? Reset->GetPropertyValue_InContainer(this) : bRestoreCapsuleProfile)
    {
        if (UPrimitiveComponent* Root = ResolvePrimitiveRoot(MeshComp))
        {
            Root->SetCollisionProfileName(TEXT("CharacterCapsule"), true);
        }
    }
}
