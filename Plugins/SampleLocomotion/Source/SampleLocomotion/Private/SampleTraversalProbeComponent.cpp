#include "SampleTraversalProbeComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

bool USampleTraversalProbeComponent::FindOpportunity(const UCapsuleComponent* Capsule,
    FVector WorldDirection, FSampleTraversalOpportunity& Opportunity) const
{
    Opportunity = FSampleTraversalOpportunity();
    const UWorld* World = GetWorld();
    WorldDirection.Z = 0.0;
    if (!World || !IsValid(Capsule) || Capsule->GetOwner() != GetOwner() ||
        WorldDirection.ContainsNaN() || !WorldDirection.Normalize() || Reach <= 0.0f ||
        MinimumHeight <= 0.0f || MaximumHeight <= MinimumHeight)
    {
        return false;
    }

    float Radius = 0.0f;
    float HalfHeight = 0.0f;
    Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
    if (Radius <= 0.0f || HalfHeight < Radius)
    {
        return false;
    }

    constexpr float Clearance = 2.0f;
    const FVector Feet = Capsule->GetComponentLocation() - FVector::UpVector * HalfHeight;
    const FVector FacingStart = Feet + FVector::UpVector * MinimumHeight;
    const FVector FacingEnd = FacingStart + WorldDirection * (Radius + Reach);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(SampleTraversal), false, GetOwner());

    FHitResult FacingHit;
    if (!World->LineTraceSingleByChannel(FacingHit, FacingStart, FacingEnd, TraceChannel, Query) ||
        FacingHit.bStartPenetrating || FVector::DotProduct(FacingHit.ImpactNormal, -WorldDirection) < 0.5)
    {
        return false;
    }

    const FVector TopAxis = FVector(FacingHit.ImpactPoint.X, FacingHit.ImpactPoint.Y, Feet.Z) +
        WorldDirection * (Radius + Clearance);
    const FVector TopStart = TopAxis + FVector::UpVector * (MaximumHeight + Clearance);
    const FVector TopEnd = TopAxis + FVector::UpVector * (MinimumHeight - Clearance);
    FHitResult TopHit;
    if (!World->LineTraceSingleByChannel(TopHit, TopStart, TopEnd, TraceChannel, Query) ||
        TopHit.bStartPenetrating || TopHit.ImpactNormal.Z < MinimumTopNormalZ)
    {
        return false;
    }

    const double Height = TopHit.ImpactPoint.Z - Feet.Z;
    if (Height < MinimumHeight || Height > MaximumHeight)
    {
        return false;
    }

    const FVector CandidateCenter = TopHit.ImpactPoint + FVector::UpVector * (HalfHeight + Clearance);
    if (World->OverlapBlockingTestByChannel(CandidateCenter, FQuat::Identity, TraceChannel,
        FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
    {
        return false;
    }

    Opportunity.FrontContact = FacingHit.ImpactPoint;
    Opportunity.FrontNormal = FacingHit.ImpactNormal;
    Opportunity.TopContact = TopHit.ImpactPoint;
    Opportunity.CandidateCapsuleCenter = CandidateCenter;
    Opportunity.Height = Height;
    return true;
}
