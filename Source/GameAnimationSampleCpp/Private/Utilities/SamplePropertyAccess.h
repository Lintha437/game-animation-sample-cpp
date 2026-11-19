#pragma once

#include "CoreMinimal.h"
#include "UObject/EnumProperty.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace SamplePropertyAccess
{
    /** Reads an underlying integer value from an FEnumProperty or FByteProperty pointer. */
    inline int64 ReadEnum(const FProperty& Property, const void* ValuePtr)
    {
        if (const auto* Enum = CastField<FEnumProperty>(&Property))
        {
            return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
        }
        if (const auto* Byte = CastField<FByteProperty>(&Property))
        {
            return Byte->GetPropertyValue(ValuePtr);
        }
        return 0;
    }

    /** Writes an unsigned enum byte into a named FByteProperty or FEnumProperty field. */
    inline void WriteEnumByte(const UStruct* Type, void* Data, FName Name, uint8 Value)
    {
        if (!Type || !Data)
        {
            return;
        }
        if (const auto* Byte = FindFProperty<FByteProperty>(Type, Name))
        {
            Byte->SetPropertyValue_InContainer(Data, Value);
        }
        else if (const auto* Enum = FindFProperty<FEnumProperty>(Type, Name))
        {
            Enum->GetUnderlyingProperty()->SetIntPropertyValue(
                Enum->ContainerPtrToValuePtr<void>(Data), static_cast<uint64>(Value));
        }
    }

    /** Copies compatible enum values between source and target properties. */
    inline void CopyEnum(const FProperty* Target, void* Destination, const FProperty* Source, const void* SourceContainer)
    {
        if (!Target || !Source || !Target->SameType(Source))
        {
            return;
        }
        Target->CopyCompleteValue(
            Target->ContainerPtrToValuePtr<void>(Destination),
            Source->ContainerPtrToValuePtr<void>(SourceContainer));
    }

    /** Reads a floating-point FNumericProperty from a struct/object buffer. */
    inline double ReadNumber(const UStruct* Type, const void* Data, FName Name, double Fallback = 0.0)
    {
        const auto* Number = Type && Data ? FindFProperty<FNumericProperty>(Type, Name) : nullptr;
        return Number && Number->IsFloatingPoint()
            ? Number->GetFloatingPointPropertyValue(Number->ContainerPtrToValuePtr<void>(Data))
            : Fallback;
    }

    /** Writes a floating-point FNumericProperty into a struct/object buffer. */
    inline void WriteNumber(const UStruct* Type, void* Data, FName Name, double Value)
    {
        const auto* Number = Type && Data ? FindFProperty<FNumericProperty>(Type, Name) : nullptr;
        if (Number && Number->IsFloatingPoint())
        {
            Number->SetFloatingPointPropertyValue(Number->ContainerPtrToValuePtr<void>(Data), Value);
        }
    }

    /** Reads a boolean property value from an object or returns bFallback if absent. */
    inline bool ReadBool(const UObject& Object, FName Name, bool bFallback = false)
    {
        const auto* Field = FindFProperty<FBoolProperty>(Object.GetClass(), Name);
        return Field ? Field->GetPropertyValue_InContainer(&Object) : bFallback;
    }

    /** Writes a boolean property value onto an object if present. */
    inline void WriteBool(UObject& Object, FName Name, bool bValue)
    {
        if (const auto* Field = FindFProperty<FBoolProperty>(Object.GetClass(), Name))
        {
            Field->SetPropertyValue_InContainer(&Object, bValue);
        }
    }

    /** Finds a struct property on an object by name. */
    inline const FStructProperty* FindStructProperty(const UObject& Object, FName Name)
    {
        return FindFProperty<FStructProperty>(Object.GetClass(), Name);
    }

    /** Reads a named FVector property on an object. */
    inline FVector ReadVector(const UObject& Object, FName Name)
    {
        const auto* Property = FindStructProperty(Object, Name);
        return Property && Property->Struct == TBaseStructure<FVector>::Get()
            ? *Property->ContainerPtrToValuePtr<FVector>(&Object)
            : FVector::ZeroVector;
    }

    /** Writes a named FVector property on an object. */
    inline void WriteVector(UObject& Object, FName Name, const FVector& Value)
    {
        const auto* Property = FindStructProperty(Object, Name);
        if (Property && Property->Struct == TBaseStructure<FVector>::Get())
        {
            *Property->ContainerPtrToValuePtr<FVector>(&Object) = Value;
        }
    }

    /** Reads a boolean member from a named struct property on an object. */
    inline bool ReadStructBool(const FStructProperty* Snapshot, const UObject& Object, FName FieldName)
    {
        const auto* Property = Snapshot ? FindFProperty<FBoolProperty>(Snapshot->Struct, FieldName) : nullptr;
        return Property && Property->GetPropertyValue_InContainer(Snapshot->ContainerPtrToValuePtr<void>(&Object));
    }

    /** Writes a boolean member inside a named struct property on an object. */
    inline void WriteStructBool(const FStructProperty* Snapshot, UObject& Object, FName FieldName, bool bValue)
    {
        const auto* Property = Snapshot ? FindFProperty<FBoolProperty>(Snapshot->Struct, FieldName) : nullptr;
        if (Property)
        {
            Property->SetPropertyValue_InContainer(Snapshot->ContainerPtrToValuePtr<void>(&Object), bValue);
        }
    }
}
