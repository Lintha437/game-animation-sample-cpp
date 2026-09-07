#include "SampleTraversalClearance.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace
{
    bool IsTraversableBlock(const AActor* Actor)
    {
        static constexpr TCHAR BlockClass[] =
            TEXT("/Game/Levels/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable_C");
        for (const UClass* Class = Actor ? Actor->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
        {
            if (Class->GetPathName() == BlockClass)
            {
                return true;
            }
        }
        return false;
    }

    FVector ClearanceOffset(float Radius, float HalfHeight, const FVector& Normal)
    {
        constexpr float Padding = 2.0f;
        return Normal * (Radius + Padding) + FVector::UpVector * (HalfHeight + Padding);
    }
}

FSampleTraversalClearance FSampleTraversalClearanceQuery::Query(AActor& Pawn, const FSampleTraversalApproach& Approach,
    float Radius, float HalfHeight, EDrawDebugTrace::Type DebugDraw, int32 DebugLevel, float DebugDuration)
{
    FSampleTraversalClearance Result;
    const FVector Origin = Pawn.GetActorLocation();
    const FVector Start = Origin + Approach.StartOffset;
    const FVector End = Start + Approach.Direction * Approach.Distance + Approach.EndOffset;
    FHitResult FacingHit;
    UKismetSystemLibrary::CapsuleTraceSingle(&Pawn, Start, End, Approach.Radius, Approach.HalfHeight,
        TraceTypeQuery3, false, {}, DebugLevel >= 2 ? DebugDraw : EDrawDebugTrace::None, FacingHit, true,
        FLinearColor::Black, FLinearColor::Black, DebugDuration);
    const AActor* Block = FacingHit.GetActor();
    if (!FacingHit.bBlockingHit || !IsTraversableBlock(Block))
    {
        return Result;
    }

    Result.Obstacle = FacingHit.GetComponent();
    Result.Ledges = USampleTraversalLedgeLibrary::QueryLedges(*Block, FacingHit.ImpactPoint, Origin);
    if (!Result.Ledges.Front)
    {
        return Result;
    }
    const FVector Front = Result.Ledges.Front->GetLocation();
    if (DebugLevel >= 1)
    {
        UKismetSystemLibrary::DrawDebugSphere(&Pawn, Front, 10.0f, 12, FLinearColor::Green, DebugDuration, 1.0f);
        if (Result.Ledges.Back)
        {
            UKismetSystemLibrary::DrawDebugSphere(&Pawn, Result.Ledges.Back->GetLocation(), 10.0f, 12,
                FLinearColor(0.0f, 0.639368f, 1.0f), DebugDuration, 1.0f);
        }
    }

    const auto Sweep = [&](const FVector& From, const FVector& To, FHitResult& Hit)
    {
        return UKismetSystemLibrary::CapsuleTraceSingle(&Pawn, From, To, Radius, HalfHeight, TraceTypeQuery1,
            false, {}, DebugLevel >= 3 ? DebugDraw : EDrawDebugTrace::None, Hit, true,
            FLinearColor::Red, FLinearColor::Green, DebugDuration);
    };
    const FVector FrontCenter = Front + ClearanceOffset(Radius, HalfHeight, Result.Ledges.Front->GetUnitAxis(EAxis::Z));
    FHitResult ApproachHit;
    Sweep(Origin, FrontCenter, ApproachHit);
    if (ApproachHit.bBlockingHit || ApproachHit.bStartPenetrating)
    {
        return Result;
    }

    Result.bCanReachFront = true;
    Result.Height = FMath::Abs(Origin.Z - HalfHeight - Front.Z);
    // The reference also sweeps when an obstacle has no mapped opposite.
    const FVector Back = Result.Ledges.Back ? Result.Ledges.Back->GetLocation() : FVector::ZeroVector;
    const FVector BackNormal = Result.Ledges.Back ? Result.Ledges.Back->GetUnitAxis(EAxis::Z) : FVector::ZeroVector;
    const FVector BackCenter = Back + ClearanceOffset(Radius, HalfHeight, BackNormal);
    FHitResult TopHit;
    if (Sweep(FrontCenter, BackCenter, TopHit))
    {
        Result.Depth = FVector::Dist2D(Front, TopHit.ImpactPoint);
        return Result;
    }

    Result.bCanCrossTop = true;
    Result.Depth = FVector::Dist2D(Front, Back);
    constexpr float FloorReach = 50.0f;
    const FVector FloorEnd = Back + BackNormal * (Radius + 2.0f) - FVector::UpVector * FloorReach;
    FHitResult FloorHit;
    Sweep(BackCenter, FloorEnd, FloorHit);
    if (FloorHit.bBlockingHit)
    {
        Result.BackFloor = FloorHit.ImpactPoint;
        Result.BackHeight = FMath::Abs(FloorHit.ImpactPoint.Z - Back.Z);
    }
    return Result;
}
