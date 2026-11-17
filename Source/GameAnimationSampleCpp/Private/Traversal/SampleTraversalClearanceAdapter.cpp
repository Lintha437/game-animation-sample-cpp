#include "Traversal/SampleTraversalComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Characters/SampleCmcCharacter.h"
#include "Characters/SampleMoverCharacter.h"
#include "Traversal/SampleTraversalClearance.h"
#include "UObject/StructOnScope.h"
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

        void SetEnum(FName Name, uint8 Value) const
        {
            const auto* Property = Type->FindPropertyByName(Name);
            if (const auto* Enum = CastField<FEnumProperty>(Property))
            {
                Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(Data),
                    static_cast<uint64>(Value));
            }
            else if (const auto* Byte = CastField<FByteProperty>(Property))
            {
                Byte->SetPropertyValue_InContainer(Data, Value);
            }
        }

        void SetObject(FName Name, UObject* Value) const
        {
            if (const auto* Field = FindFProperty<FObjectPropertyBase>(Type, Name);
                Field && (!Value || Value->IsA(Field->PropertyClass)))
            {
                Field->SetObjectPropertyValue_InContainer(Data, Value);
            }
        }
    };

    UObject* ReadPawnObject(const APawn& Pawn, FName Name)
    {
        const auto* Field = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), Name);
        return Field ? Field->GetObjectPropertyValue_InContainer(&Pawn) : nullptr;
    }

    void WriteCharacterProperties(const FBlueprintStructure& Output, const APawn& Pawn, UObject* Capsule,
        UObject* Mesh, uint8 MovementMode, uint8 Gait, double Speed)
    {
        Output.SetObject(TEXT("Capsule_21_D1F3797D47A5FB49C3DFAE8FAB15AFCC"), Capsule);
        Output.SetObject(TEXT("Mesh_15_D47797BD4F40417B966E3BB7E0AC62D3"), Mesh);
        Output.SetObject(TEXT("MotionWarping_18_C9F4AD1440C92128F649A7BA8B49094B"),
            ReadPawnObject(Pawn, TEXT("MotionWarping")));
        Output.SetEnum(TEXT("MovementMode_22_F39395024EEBA400FBE9FB8AE0EF7350"), MovementMode);
        Output.SetEnum(TEXT("Gait_12_9093F6E14192D37D2F0223B94E64FF71"), Gait);
        Output.Set(TEXT("Speed_26_C6C3500C4C3030A343948F80DFD60AEE"), Speed);
    }

    void SampleCharacterProperties(const FBlueprintStructure& Output, const APawn* Pawn)
    {
        if (const auto* Cmc = Cast<ASampleCmcCharacter>(Pawn))
        {
            const auto* Movement = Cmc->GetCharacterMovement();
            const EMovementMode Mode = Movement ? Movement->MovementMode.GetValue() : MOVE_None;
            // These are the serialized OnGround and InAir values of the authored enum.
            constexpr uint8 OnGround = 0;
            constexpr uint8 InAir = 1;
            const bool bGroundMode = Mode == MOVE_None || Mode == MOVE_Walking || Mode == MOVE_NavWalking;
            WriteCharacterProperties(Output, *Cmc, Cmc->GetCapsuleComponent(), Cmc->GetMesh(),
                bGroundMode ? OnGround : InAir, Cmc->GetAuthoredGaitValue(), Movement ? Movement->Velocity.Size2D() : 0.0);
        }
        else if (const auto* Mover = Cast<ASampleMoverCharacter>(Pawn))
        {
            const auto* Movement = Cast<UCharacterMoverComponent>(ReadPawnObject(*Mover, TEXT("CharacterMover")));
            if (IsValid(Movement))
            {
                WriteCharacterProperties(Output, *Mover, ReadPawnObject(*Mover, TEXT("Capsule")),
                    ReadPawnObject(*Mover, TEXT("SkeletalMesh")), Mover->ResolveMovementModeValue(),
                    Mover->GetAuthoredGaitValue(), Movement->GetVelocity().Size2D());
            }
        }
    }

    void WriteApproach(const FBlueprintStructure& Output, const FSampleTraversalApproach& Approach)
    {
        Output.Set(TEXT("TraceForwardDirection_5_84E06003419F502C44036B9424B39047"), Approach.Direction);
        Output.Set(TEXT("TraceOriginOffset_7_2757E2FF43A7611D7F46EE9AAA15F8C0"), Approach.StartOffset);
        Output.Set(TEXT("TraceEndOffset_15_35E488F2478DA8D4D7E69FB93494661B"), Approach.EndOffset);
        Output.Set(TEXT("TraceForwardDistance_2_07F489F3465D510BFC123689985564DC"), Approach.Distance);
        Output.Set(TEXT("TraceRadius_10_FF044CA546FFD12BAA4D4E93460A273A"), static_cast<double>(Approach.Radius));
        Output.Set(TEXT("TraceHalfHeight_12_019A16DE47BB8535C3C0CF8AA6E355EB"), static_cast<double>(Approach.HalfHeight));
    }

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

    void ResetGeometry(const FBlueprintStructure& Output)
    {
        Output.SetObject(TEXT("HitComponent_68_EBCBFE6F45F585869177C0B80CF2CBF3"), nullptr);
        Output.Set(TEXT("HasFrontLedge_34_6C547E244F41599C4D38E1909503CCBA"), false);
        Output.Set(TEXT("HasBackLedge_41_8550E97541A4EDBCA44DB0B16F1B51C4"), false);
        Output.Set(TEXT("HasBackFloor_25_AC42C42F4DD2BFF32A30BDAB394529DE"), false);
        Output.Set(TEXT("FrontLedgeLocation_50_52B8B8A84F10B0E0A9F287972F2C54D8"), FVector::ZeroVector);
        Output.Set(TEXT("FrontLedgeNormal_51_3C4F46204EAF10E382D4D9A8E26ACB5A"), FVector::ZeroVector);
        Output.Set(TEXT("BackLedgeLocation_55_EB7453EF477E18645DAD9F8950BCF44F"), FVector::ZeroVector);
        Output.Set(TEXT("BackLedgeNormal_56_F05B9B4A4046704A49754F80A8C7BCE3"), FVector::ZeroVector);
        Output.Set(TEXT("BackFloorLocation_58_0BB660F246B00F3211984E9AC5FE310D"), FVector::ZeroVector);
        Output.Set(TEXT("ObstacleHeight_63_0A6EB46D4C92C5D9173661B88701D23F"), 0.0);
        Output.Set(TEXT("ObstacleDepth_17_34DBDE1142FC8B90DA8440A77C6664CB"), 0.0);
        Output.Set(TEXT("BackLedgeHeight_64_FA78930E475E1B89F0CCC5BC6F6043A3"), 0.0);
    }

    void WriteResult(const FBlueprintStructure& Output, const FSampleTraversalClearance& Result)
    {
        Output.SetObject(TEXT("HitComponent_68_EBCBFE6F45F585869177C0B80CF2CBF3"), Result.Obstacle.Get());
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

void USampleTraversalComponent::BuildImportedTraversalInputs(const APawn*, int32&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(USampleTraversalComponent::execBuildImportedTraversalInputs)
{
    P_GET_OBJECT(APawn, Pawn);
    const FBlueprintStructure Output = FBlueprintStructure::Read(Stack);
    P_FINISH;

    P_NATIVE_BEGIN;
    if (Output.Data && Output.Type && Output.Type->GetFName() == TEXT("S_TraversalCheckInputs"))
    {
        Output.Type->ClearScriptStruct(Output.Data);
        if (const auto* Cmc = Cast<ASampleCmcCharacter>(Pawn); Cmc && Cmc->GetCharacterMovement())
        {
            WriteApproach(Output, Cmc->BuildTraversalApproach());
        }
        else if (const auto* Mover = Cast<ASampleMoverCharacter>(Pawn))
        {
            FSampleTraversalApproach Approach;
            if (Mover->TryBuildTraversalApproach(Approach))
            {
                WriteApproach(Output, Approach);
            }
        }
    }
    P_NATIVE_END;
}

void USampleTraversalComponent::BuildImportedTraversalProperties(const APawn*, int32&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(USampleTraversalComponent::execBuildImportedTraversalProperties)
{
    P_GET_OBJECT(APawn, Pawn);
    const FBlueprintStructure Output = FBlueprintStructure::Read(Stack);
    P_FINISH;

    P_NATIVE_BEGIN;
    if (Output.Data && Output.Type && Output.Type->GetFName() == TEXT("S_CharacterPropertiesForTraversal"))
    {
        Output.Type->ClearScriptStruct(Output.Data);
        SampleCharacterProperties(Output, Pawn);
    }
    P_NATIVE_END;
}

bool USampleTraversalComponent::TryAuthoredTraversal(const FSampleTraversalApproach& Approach, EDrawDebugTrace::Type DebugType)
{
    UFunction* Function = FindFunction(TEXT("TryTraversalAction"));
    const auto* Inputs = Function ? FindFProperty<FStructProperty>(Function, TEXT("Inputs")) : nullptr;
    const auto* CheckFailed = Function ? FindFProperty<FBoolProperty>(Function, TEXT("TraversalCheckFailed")) : nullptr;
    const auto* MontageFailed = Function ? FindFProperty<FBoolProperty>(Function, TEXT("MontageSelectionFailed")) : nullptr;
    if (!Inputs || Inputs->Struct->GetFName() != TEXT("S_TraversalCheckInputs") || !CheckFailed || !MontageFailed)
    {
        return false;
    }

    FStructOnScope Args(Function);
    uint8* Data = Args.GetStructMemory();
    const FBlueprintStructure Probe{Inputs->Struct.Get(), Inputs->ContainerPtrToValuePtr<void>(Data)};
    WriteApproach(Probe, Approach);
    if (const auto* Debug = FindFProperty<FByteProperty>(Function, TEXT("DebugType")))
    {
        Debug->SetPropertyValue_InContainer(Data, static_cast<uint8>(DebugType));
    }
    else if (const auto* DebugEnum = FindFProperty<FEnumProperty>(Function, TEXT("DebugType")))
    {
        DebugEnum->GetUnderlyingProperty()->SetIntPropertyValue(DebugEnum->ContainerPtrToValuePtr<void>(Data),
            static_cast<uint64>(DebugType));
    }
    const uint64 PreviousDispatch = TraversalDispatchSerial;
    ProcessEvent(Function, Data);
    return !CheckFailed->GetPropertyValue_InContainer(Data) && !MontageFailed->GetPropertyValue_InContainer(Data) &&
        TraversalDispatchSerial != PreviousDispatch;
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
    const bool bValidOutput = Output.Data && Output.Type &&
        Output.Type->GetFName() == TEXT("S_TraversalCheckResult");
    if (bValidOutput)
    {
        ResetGeometry(Output);
    }
    AActor* Pawn = P_THIS->GetOwner();
    const UCapsuleComponent* Capsule = GetTraversalCapsule(*P_THIS);
    if (Pawn && IsValid(Capsule) && Input.Data && Input.Type && Input.Type->GetFName() == TEXT("S_TraversalCheckInputs") &&
        bValidOutput)
    {
        FSampleTraversalApproach Approach;
        Approach.Direction = Input.Vector(TEXT("TraceForwardDirection_5_84E06003419F502C44036B9424B39047"));
        Approach.StartOffset = Input.Vector(TEXT("TraceOriginOffset_7_2757E2FF43A7611D7F46EE9AAA15F8C0"));
        Approach.EndOffset = Input.Vector(TEXT("TraceEndOffset_15_35E488F2478DA8D4D7E69FB93494661B"));
        Approach.Distance = Input.Number(TEXT("TraceForwardDistance_2_07F489F3465D510BFC123689985564DC"));
        Approach.Radius = Input.Number(TEXT("TraceRadius_10_FF044CA546FFD12BAA4D4E93460A273A"));
        Approach.HalfHeight = Input.Number(TEXT("TraceHalfHeight_12_019A16DE47BB8535C3C0CF8AA6E355EB"));
        int32 DebugLevel;
        float DebugDuration;
        P_THIS->ReadTraversalDebugSettings(DebugLevel, DebugDuration);
        const FSampleTraversalClearance Result = FSampleTraversalClearanceQuery::Query(*Pawn, Approach,
            Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight(),
            static_cast<EDrawDebugTrace::Type>(DebugType),
            DebugLevel, DebugDuration);
        WriteResult(Output, Result);
        bAccepted = Result.bCanReachFront;
    }
    P_NATIVE_END;
}
