#include "SampleTraversalLedgeLibrary.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace
{
    const USplineComponent* FindFrontSpline(const AActor& Block, const FVector& ActorLocation)
    {
        const FArrayProperty* Ledges = FindFProperty<FArrayProperty>(Block.GetClass(), TEXT("Ledges"));
        const FObjectPropertyBase* Element = Ledges ? CastField<FObjectPropertyBase>(Ledges->Inner) : nullptr;
        if (!Element)
        {
            return nullptr;
        }
        FScriptArrayHelper Array(Ledges, Ledges->ContainerPtrToValuePtr<void>(&Block));
        const USplineComponent* Closest = nullptr;
        double ClosestDistance = TNumericLimits<double>::Max();
        for (int32 Index = 0; Index < Array.Num(); ++Index)
        {
            const auto* Spline = Cast<USplineComponent>(Element->GetObjectPropertyValue(Array.GetRawPtr(Index)));
            if (!IsValid(Spline))
            {
                continue;
            }
            const FVector Edge = Spline->FindLocationClosestToWorldLocation(ActorLocation, ESplineCoordinateSpace::World);
            const FVector Normal = Spline->FindUpVectorClosestToWorldLocation(ActorLocation, ESplineCoordinateSpace::World);
            const double Distance = FVector::DistSquared(Edge + Normal * 10.0, ActorLocation);
            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                Closest = Spline;
            }
        }
        return Closest;
    }

    const USplineComponent* FindOppositeSpline(const AActor& Block, const USplineComponent& Front)
    {
        const FMapProperty* Opposites = FindFProperty<FMapProperty>(Block.GetClass(), TEXT("OppositeLedges"));
        const auto* Key = Opposites ? CastField<FObjectPropertyBase>(Opposites->KeyProp) : nullptr;
        const auto* Value = Opposites ? CastField<FObjectPropertyBase>(Opposites->ValueProp) : nullptr;
        if (!Key || !Value)
        {
            return nullptr;
        }
        FScriptMapHelper Map(Opposites, Opposites->ContainerPtrToValuePtr<void>(&Block));
        for (int32 Index = 0; Index < Map.GetMaxIndex(); ++Index)
        {
            if (Map.IsValidIndex(Index) && Key->GetObjectPropertyValue(Map.GetKeyPtr(Index)) == &Front)
            {
                return Cast<USplineComponent>(Value->GetObjectPropertyValue(Map.GetValuePtr(Index)));
            }
        }
        return nullptr;
    }

    void WriteLedge(const UScriptStruct& Type, void* Data, FName FlagName, FName LocationName, FName NormalName,
        const TOptional<FTransform>& Ledge)
    {
        const FBoolProperty* Flag = FindFProperty<FBoolProperty>(&Type, FlagName);
        const FStructProperty* Location = FindFProperty<FStructProperty>(&Type, LocationName);
        const FStructProperty* Normal = FindFProperty<FStructProperty>(&Type, NormalName);
        if (!Flag || !Location || !Normal || Location->Struct != TBaseStructure<FVector>::Get() ||
            Normal->Struct != TBaseStructure<FVector>::Get())
        {
            return;
        }
        Flag->SetPropertyValue_InContainer(Data, Ledge.IsSet());
        if (Ledge)
        {
            *Location->ContainerPtrToValuePtr<FVector>(Data) = Ledge->GetLocation();
            *Normal->ContainerPtrToValuePtr<FVector>(Data) = Ledge->GetUnitAxis(EAxis::Z);
        }
    }
}

FSampleTraversalLedges USampleTraversalLedgeLibrary::QueryLedges(
    const AActor& Block, FVector HitLocation, FVector ActorLocation)
{
    FSampleTraversalLedges Ledges;
    const USplineComponent* Front = FindFrontSpline(Block, ActorLocation);
    const FNumericProperty* Width = FindFProperty<FNumericProperty>(Block.GetClass(), TEXT("MinLedgeWidth"));
    const double Minimum = Width && Width->IsFloatingPoint()
        ? Width->GetFloatingPointPropertyValue(Width->ContainerPtrToValuePtr<void>(&Block))
        : 60.0;
    if (!Front || Front->GetSplineLength() < Minimum)
    {
        return Ledges;
    }
    const float Clearance = static_cast<float>(Minimum * 0.5);
    const FVector AlongSpline = Front->FindLocationClosestToWorldLocation(HitLocation, ESplineCoordinateSpace::Local);
    const float Distance = FMath::Clamp(Front->GetDistanceAlongSplineAtLocation(AlongSpline, ESplineCoordinateSpace::Local),
        Clearance, Front->GetSplineLength() - Clearance);
    Ledges.Front = Front->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World, false);
    if (const USplineComponent* Back = FindOppositeSpline(Block, *Front); IsValid(Back))
    {
        Ledges.Back = Back->FindTransformClosestToWorldLocation(
            Ledges.Front->GetLocation(), ESplineCoordinateSpace::World, false);
    }
    return Ledges;
}

void USampleTraversalLedgeLibrary::UpdateBlockLedges(AActor*, FVector, FVector, int32&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(USampleTraversalLedgeLibrary::execUpdateBlockLedges)
{
    P_GET_OBJECT(AActor, Block);
    P_GET_STRUCT(FVector, HitLocation);
    P_GET_STRUCT(FVector, ActorLocation);
    Stack.StepCompiledIn<FStructProperty>(nullptr);
    void* Data = Stack.MostRecentPropertyAddress;
    const FStructProperty* Result = CastField<FStructProperty>(Stack.MostRecentProperty);
    P_FINISH;

    P_NATIVE_BEGIN;
    if (Block && Data && Result && Result->Struct->GetFName() == TEXT("S_TraversalCheckResult"))
    {
        const FSampleTraversalLedges Ledges = QueryLedges(*Block, HitLocation, ActorLocation);
        WriteLedge(*Result->Struct, Data, TEXT("HasFrontLedge_34_6C547E244F41599C4D38E1909503CCBA"),
            TEXT("FrontLedgeLocation_50_52B8B8A84F10B0E0A9F287972F2C54D8"),
            TEXT("FrontLedgeNormal_51_3C4F46204EAF10E382D4D9A8E26ACB5A"), Ledges.Front);
        if (Ledges.Front)
        {
            WriteLedge(*Result->Struct, Data, TEXT("HasBackLedge_41_8550E97541A4EDBCA44DB0B16F1B51C4"),
                TEXT("BackLedgeLocation_55_EB7453EF477E18645DAD9F8950BCF44F"),
                TEXT("BackLedgeNormal_56_F05B9B4A4046704A49754F80A8C7BCE3"), Ledges.Back);
        }
    }
    P_NATIVE_END;
}
