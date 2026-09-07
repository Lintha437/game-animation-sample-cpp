#include "SampleTraversalComponent.h"

#include "Components/CapsuleComponent.h"
#include "SampleTraversalClearance.h"
#include "UObject/UnrealType.h"

namespace
{
    struct FBlueprintStructure
    {
        const UScriptStruct* Type;
        void* Data;

        static FBlueprintStructure Read(FFrame& Stack)
        {
            Stack.MostRecentProperty = nullptr;
            Stack.MostRecentPropertyAddress = nullptr;
            Stack.StepCompiledIn<FStructProperty>(nullptr);
            const auto* Property = CastField<FStructProperty>(Stack.MostRecentProperty);
            return {Property ? Property->Struct.Get() : nullptr, Stack.MostRecentPropertyAddress};
        }

        FVector Vector(FName Name) const
        {
            const auto* Property = FindFProperty<FStructProperty>(Type, Name);
            return Property && Property->Struct == TBaseStructure<FVector>::Get()
                ? *Property->ContainerPtrToValuePtr<const FVector>(Data) : FVector::ZeroVector;
        }

        double Number(FName Name) const
        {
            const auto* Property = FindFProperty<FNumericProperty>(Type, Name);
            return Property && Property->IsFloatingPoint()
                ? Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Data)) : 0.0;
        }

        void Set(FName Name, const FVector& Value) const
        {
            const auto* Property = FindFProperty<FStructProperty>(Type, Name);
            if (Property && Property->Struct == TBaseStructure<FVector>::Get())
            {
                *Property->ContainerPtrToValuePtr<FVector>(Data) = Value;
            }
        }

        void Set(FName Name, double Value) const
        {
            const auto* Property = FindFProperty<FNumericProperty>(Type, Name);
            if (Property && Property->IsFloatingPoint())
            {
                Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Data), Value);
            }
        }

        void Set(FName Name, bool Value) const
        {
            if (const auto* Property = FindFProperty<FBoolProperty>(Type, Name))
            {
                Property->SetPropertyValue_InContainer(Data, Value);
            }
        }
    };

    const UCapsuleComponent* GetTraversalCapsule(const USampleTraversalComponent& Component)
    {
        const auto* Character = FindFProperty<FStructProperty>(Component.GetClass(), TEXT("CharacterProperties"));
        if (!Character)
        {
            return nullptr;
        }
        const auto* Capsule = FindFProperty<FObjectPropertyBase>(Character->Struct,
            TEXT("Capsule_21_D1F3797D47A5FB49C3DFAE8FAB15AFCC"));
        return Capsule ? Cast<UCapsuleComponent>(Capsule->GetObjectPropertyValue_InContainer(
            Character->ContainerPtrToValuePtr<void>(&Component))) : nullptr;
    }

    void WriteResult(const FBlueprintStructure& Output, const FSampleTraversalClearance& Result)
    {
        if (Result.Obstacle.IsValid())
        {
            if (const auto* Hit = FindFProperty<FObjectPropertyBase>(Output.Type,
                TEXT("HitComponent_68_EBCBFE6F45F585869177C0B80CF2CBF3")))
            {
                Hit->SetObjectPropertyValue_InContainer(Output.Data, Result.Obstacle.Get());
            }
        }
        Output.Set(TEXT("HasFrontLedge_34_6C547E244F41599C4D38E1909503CCBA"), Result.bCanReachFront);
        if (Result.Ledges.Front)
        {
            Output.Set(TEXT("FrontLedgeLocation_50_52B8B8A84F10B0E0A9F287972F2C54D8"), Result.Ledges.Front->GetLocation());
            Output.Set(TEXT("FrontLedgeNormal_51_3C4F46204EAF10E382D4D9A8E26ACB5A"), Result.Ledges.Front->GetUnitAxis(EAxis::Z));
        }
        Output.Set(TEXT("HasBackLedge_41_8550E97541A4EDBCA44DB0B16F1B51C4"),
            Result.Ledges.Back.IsSet() && (!Result.bCanReachFront || Result.bCanCrossTop));
        if (Result.Ledges.Back)
        {
            Output.Set(TEXT("BackLedgeLocation_55_EB7453EF477E18645DAD9F8950BCF44F"), Result.Ledges.Back->GetLocation());
            Output.Set(TEXT("BackLedgeNormal_56_F05B9B4A4046704A49754F80A8C7BCE3"), Result.Ledges.Back->GetUnitAxis(EAxis::Z));
        }
        if (Result.bCanReachFront)
        {
            Output.Set(TEXT("ObstacleHeight_63_0A6EB46D4C92C5D9173661B88701D23F"), Result.Height);
            Output.Set(TEXT("ObstacleDepth_17_34DBDE1142FC8B90DA8440A77C6664CB"), Result.Depth);
            Output.Set(TEXT("HasBackFloor_25_AC42C42F4DD2BFF32A30BDAB394529DE"), Result.BackFloor.IsSet());
            if (Result.BackFloor)
            {
                Output.Set(TEXT("BackFloorLocation_58_0BB660F246B00F3211984E9AC5FE310D"), Result.BackFloor.GetValue());
                Output.Set(TEXT("BackLedgeHeight_64_FA78930E475E1B89F0CCC5BC6F6043A3"), Result.BackHeight);
            }
        }
    }
}

bool USampleTraversalComponent::EvaluateTraversalClearance(int32&, TEnumAsByte<EDrawDebugTrace::Type>, int32&)
{
    checkNoEntry();
    return false;
}

DEFINE_FUNCTION(USampleTraversalComponent::execEvaluateTraversalClearance)
{
    const FBlueprintStructure Input = FBlueprintStructure::Read(Stack);
    P_GET_PROPERTY(FByteProperty, DebugType);
    const FBlueprintStructure Output = FBlueprintStructure::Read(Stack);
    P_FINISH;

    P_NATIVE_BEGIN;
    bool& bAccepted = *static_cast<bool*>(RESULT_PARAM);
    bAccepted = false;
    AActor* Pawn = P_THIS->GetOwner();
    const UCapsuleComponent* Capsule = GetTraversalCapsule(*P_THIS);
    if (Pawn && IsValid(Capsule) && Input.Data && Input.Type && Input.Type->GetFName() == TEXT("S_TraversalCheckInputs") &&
        Output.Data && Output.Type && Output.Type->GetFName() == TEXT("S_TraversalCheckResult"))
    {
        FSampleTraversalApproach Approach;
        Approach.Direction = Input.Vector(TEXT("TraceForwardDirection_5_84E06003419F502C44036B9424B39047"));
        Approach.StartOffset = Input.Vector(TEXT("TraceOriginOffset_7_2757E2FF43A7611D7F46EE9AAA15F8C0"));
        Approach.EndOffset = Input.Vector(TEXT("TraceEndOffset_15_35E488F2478DA8D4D7E69FB93494661B"));
        Approach.Distance = Input.Number(TEXT("TraceForwardDistance_2_07F489F3465D510BFC123689985564DC"));
        Approach.Radius = Input.Number(TEXT("TraceRadius_10_FF044CA546FFD12BAA4D4E93460A273A"));
        Approach.HalfHeight = Input.Number(TEXT("TraceHalfHeight_12_019A16DE47BB8535C3C0CF8AA6E355EB"));
        const FSampleTraversalClearance Result = FSampleTraversalClearanceQuery::Query(*Pawn, Approach,
            Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight(),
            static_cast<EDrawDebugTrace::Type>(DebugType),
            UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.Traversal.DrawDebugLevel")),
            UKismetSystemLibrary::GetConsoleVariableFloatValue(TEXT("DDCvar.Traversal.DrawDebugDuration")));
        WriteResult(Output, Result);
        bAccepted = Result.bCanReachFront;
    }
    P_NATIVE_END;
}
