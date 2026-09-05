#include "SampleTraversalComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
    UCharacterMovementComponent* FindCharacterMovement(const UActorComponent& Component)
    {
        const ACharacter* Character = Cast<ACharacter>(Component.GetOwner());
        return Character ? Character->GetCharacterMovement() : nullptr;
    }

    bool UsesAirborneExit(const USampleTraversalComponent& Component)
    {
        static const FName ResultName(TEXT("TraversalResult"));
        static const FName ActionName(TEXT("ActionType_2_96B748004D5339073D1AB49946EAF925"));
        constexpr TCHAR AirborneAction[] = TEXT("NewEnumerator1");

        const FStructProperty* Result = FindFProperty<FStructProperty>(Component.GetClass(), ResultName);
        if (!Result)
        {
            return false;
        }
        const void* Data = Result->ContainerPtrToValuePtr<void>(&Component);
        const FProperty* Action = Result->Struct->FindPropertyByName(ActionName);
        if (const FEnumProperty* Enum = CastField<FEnumProperty>(Action))
        {
            const int64 Airborne = Enum->GetEnum()->GetValueByNameString(AirborneAction);
            return Airborne != INDEX_NONE && Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                Enum->ContainerPtrToValuePtr<void>(Data)) == Airborne;
        }
        if (const FByteProperty* Byte = CastField<FByteProperty>(Action); Byte && Byte->Enum)
        {
            return Byte->GetPropertyValue_InContainer(Data) == Byte->Enum->GetValueByNameString(AirborneAction);
        }
        return false;
    }
}

void USampleTraversalComponent::SetMovementMode_CMC(EMovementMode CMCMovementMode)
{
    if (UCharacterMovementComponent* Movement = FindCharacterMovement(*this))
    {
        Movement->SetMovementMode(CMCMovementMode);
    }
}

EMovementMode USampleTraversalComponent::GetEndMovementMode_CMC() const
{
    return UsesAirborneExit(*this) ? MOVE_Falling : MOVE_Walking;
}

void USampleTraversalComponent::SetReplicationBehavior(bool ClientAuthoritative)
{
    static const FName MoverFlag(TEXT("IsMoverCharacter"));
    const FBoolProperty* IsMover = FindFProperty<FBoolProperty>(GetClass(), MoverFlag);
    if (IsMover && IsMover->GetPropertyValue_InContainer(this))
    {
        return;
    }
    if (UCharacterMovementComponent* Movement = FindCharacterMovement(*this))
    {
        Movement->bIgnoreClientMovementErrorChecksAndCorrection = ClientAuthoritative;
        Movement->bServerAcceptClientAuthoritativePosition = ClientAuthoritative;
    }
}
