#include "SampleAnimationNotifyMigrationLibrary.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
    bool CopySetting(const UObject& Source, const FProperty& SourceProperty,
        UObject& Destination, const FProperty& DestinationProperty)
    {
        const void* SourceValue = SourceProperty.ContainerPtrToValuePtr<void>(&Source);
        void* DestinationValue = DestinationProperty.ContainerPtrToValuePtr<void>(&Destination);
        if (SourceProperty.SameType(&DestinationProperty))
        {
            DestinationProperty.CopyCompleteValue(DestinationValue, SourceValue);
            return true;
        }

        // Blueprint and native enums can use different reflected storage for the same enum.
        const auto* SourceEnum = CastField<FEnumProperty>(&SourceProperty);
        const auto* DestinationByte = CastField<FByteProperty>(&DestinationProperty);
        if (SourceEnum && DestinationByte && SourceEnum->GetEnum() == DestinationByte->Enum)
        {
            const int64 Value = SourceEnum->GetUnderlyingProperty()->GetSignedIntPropertyValue(SourceValue);
            if (Value >= 0 && Value <= MAX_uint8 && DestinationByte->Enum->IsValidEnumValue(Value))
            {
                DestinationByte->SetPropertyValue(DestinationValue, static_cast<uint8>(Value));
                return true;
            }
        }
        return false;
    }

    bool CopySettings(const UObject& Source, UObject& Destination, const UClass& NotifyBase,
        const TMap<FName, FName>& FieldRenames)
    {
        for (TFieldIterator<FProperty> Property(&NotifyBase); Property; ++Property)
        {
            if (Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Transient))
            {
                CopySetting(Source, **Property, Destination, **Property);
            }
        }
        for (const auto& [OldName, NewName] : FieldRenames)
        {
            const FProperty* OldField = FindFProperty<FProperty>(Source.GetClass(), OldName);
            const FProperty* NewField = FindFProperty<FProperty>(Destination.GetClass(), NewName);
            if (!OldField || !NewField || !CopySetting(Source, *OldField, Destination, *NewField))
            {
                UE_LOG(LogTemp, Error, TEXT("Cannot preserve notify setting %s as %s on %s"),
                    *OldName.ToString(), *NewName.ToString(), *Source.GetPathName());
                return false;
            }
        }
        return true;
    }
}

int32 USampleAnimationNotifyMigrationLibrary::ReplaceAnimationNotifyInstances(UAnimSequenceBase* Animation,
    UClass* LegacyClass, const TMap<FName, FName>& FieldRenames, bool bValidateOnly)
{
    UClass* NativeClass = LegacyClass ? LegacyClass->GetSuperClass() : nullptr;
    if (!Animation || !NativeClass || LegacyClass->HasAnyClassFlags(CLASS_Native) ||
        !NativeClass->HasAnyClassFlags(CLASS_Native) || NativeClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return INDEX_NONE;
    }
    const bool bNotifyState = NativeClass->IsChildOf(UAnimNotifyState::StaticClass());
    const UClass* NotifyBase = bNotifyState ? UAnimNotifyState::StaticClass() : UAnimNotify::StaticClass();
    if (NativeClass == NotifyBase || !NativeClass->IsChildOf(NotifyBase))
    {
        return INDEX_NONE;
    }
    for (TFieldIterator<FProperty> Property(LegacyClass, EFieldIterationFlags::None); Property; ++Property)
    {
        if (!FieldRenames.Contains(Property->GetFName()))
        {
            return INDEX_NONE;
        }
    }

    TArray<TPair<int32, UObject*>> Replacements;
    for (int32 Index = 0; Index < Animation->Notifies.Num(); ++Index)
    {
        const FAnimNotifyEvent& Event = Animation->Notifies[Index];
        const UObject* Legacy = bNotifyState ? static_cast<const UObject*>(Event.NotifyStateClass.Get()) : Event.Notify.Get();
        if (!Legacy || Legacy->GetClass() != LegacyClass)
        {
            continue;
        }
        UObject* Outer = bValidateOnly ? static_cast<UObject*>(GetTransientPackage()) : Animation;
        UObject* Replacement = NewObject<UObject>(Outer, NativeClass, NAME_None, bValidateOnly ? RF_Transient : RF_Transactional);
        if (!CopySettings(*Legacy, *Replacement, *NotifyBase, FieldRenames))
        {
            return INDEX_NONE;
        }
        Replacements.Emplace(Index, Replacement);
    }

    if (!bValidateOnly && !Replacements.IsEmpty())
    {
        Animation->Modify();
        for (const auto& [Index, Replacement] : Replacements)
        {
            FAnimNotifyEvent& Event = Animation->Notifies[Index];
            if (bNotifyState)
            {
                Event.NotifyStateClass = CastChecked<UAnimNotifyState>(Replacement);
            }
            else
            {
                Event.Notify = CastChecked<UAnimNotify>(Replacement);
            }
        }
        Animation->RefreshCacheData();
        Animation->MarkPackageDirty();
    }
    return Replacements.Num();
}
