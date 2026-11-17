#include "Sandbox/SampleMultiplayerSpawnGameMode.h"
#include "Sandbox/SampleSandboxFixtures.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogSampleMultiplayerSpawn, Log, All);

namespace
{
    /** Returns whether the proposed capsule overlaps another possessed player. */
    bool OverlapsPlayer(const UWorld& World, const AController* NewPlayer, const FVector& Location,
        float Radius, float HalfHeight)
    {
        for (TActorIterator<APlayerController> It(&World); It; ++It)
        {
            const APlayerController* Controller = *It;
            const APawn* Other = Controller == NewPlayer ? nullptr : Controller->GetPawn();
            if (!IsValid(Other))
            {
                continue;
            }

            float OtherRadius = 0.0f;
            float OtherHalfHeight = 0.0f;
            Other->GetSimpleCollisionCylinder(OtherRadius, OtherHalfHeight);
            const FVector OtherLocation = Other->GetActorLocation();
            const float MinDistance = Radius + OtherRadius + 20.0f;
            if (FVector::DistSquared2D(Location, OtherLocation) < FMath::Square(MinDistance) &&
                FMath::Abs(Location.Z - OtherLocation.Z) < HalfHeight + OtherHalfHeight + 10.0f)
            {
                return true;
            }
        }
        return false;
    }

    /** Projects an offset onto walkable geometry, including movable sample floors. */
    bool ProjectToFloor(const UWorld& World, const FVector& Start, float HalfHeight, FVector& Result)
    {
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        const FVector TraceStart(Start.X, Start.Y, Start.Z + FMath::Max(200.0f, HalfHeight * 2.0f));
        const FVector TraceEnd(Start.X, Start.Y, Start.Z - 3000.0f);
        FHitResult Hit;
        if (!World.LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, Objects) || Hit.ImpactNormal.Z < 0.7f)
        {
            return false;
        }
        Result = FVector(Start.X, Start.Y, Hit.ImpactPoint.Z + HalfHeight + 2.0f);
        return true;
    }

    /** Avoids joining directly on buttons whose overlap changes the shared level presentation. */
    bool OverlapsLevelButton(const UWorld& World, const FVector& Location, float Radius, float HalfHeight)
    {
        // A query-only trigger can ignore the query channel used by overlap sweeps.
        // Check its actual world-space bounds so spawn clearance follows authored dimensions.
        for (TActorIterator<ASampleLevelButton> Button(&World); Button; ++Button)
        {
            TArray<UBoxComponent*> Boxes;
            Button->GetComponents<UBoxComponent>(Boxes);
            for (const UBoxComponent* Box : Boxes)
            {
                if (Box->GetFName() == TEXT("Trigger") &&
                    Box->Bounds.GetBox().ExpandBy(FVector(Radius + 10.0f, Radius + 10.0f, HalfHeight + 10.0f))
                        .IsInsideOrOn(Location))
                {
                    return true;
                }
            }
        }
        return false;
    }
}

APawn* ASampleMultiplayerSpawnGameMode::SpawnDefaultPawnAtTransform_Implementation(
    AController* NewPlayer, const FTransform& SpawnTransform)
{
    UWorld* World = GetWorld();
    UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
    const APawn* PawnDefaults = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
    const UCapsuleComponent* Capsule = PawnDefaults ? PawnDefaults->FindComponentByClass<UCapsuleComponent>() : nullptr;
    // Blueprint-added components are instantiated on actors, but may be absent from their CDO.
    // A previously spawned pawn supplies the authoritative shape for subsequent joins.
    if (World && PawnClass && !Capsule)
    {
        for (TActorIterator<APawn> It(World); It; ++It)
        {
            if (It->IsA(PawnClass))
            {
                Capsule = It->FindComponentByClass<UCapsuleComponent>();
                if (Capsule)
                {
                    break;
                }
            }
        }
    }
    if (!World || !PawnDefaults || !Capsule || !Capsule->IsCollisionEnabled())
    {
        return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
    }

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FRotator Rotation = SpawnTransform.GetRotation().Rotator();
    const auto IsClear = [&](const FVector& Location)
    {
        return !OverlapsPlayer(*World, NewPlayer, Location, Radius, HalfHeight) &&
            !OverlapsLevelButton(*World, Location, Radius, HalfHeight) &&
            !World->EncroachingBlockingGeometry(PawnDefaults, Location, Rotation);
    };

    const FVector Start = SpawnTransform.GetLocation();
    if (IsClear(Start))
    {
        return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
    }

    const float Spacing = FMath::Max(180.0f, Radius * 2.0f + 40.0f);
    int32 PlayerCount = 0;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ++PlayerCount;
    }
    const int32 MaxRing = FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(static_cast<float>(PlayerCount + 1))) + 8, 8, 128);

    for (int32 Ring = 1; Ring <= MaxRing; ++Ring)
    {
        for (int32 X = -Ring; X <= Ring; ++X)
        {
            for (int32 Y = -Ring; Y <= Ring; ++Y)
            {
                if (FMath::Max(FMath::Abs(X), FMath::Abs(Y)) != Ring)
                {
                    continue;
                }

                const FVector Offset(Spacing * X, Spacing * Y, 0.0f);
                FVector Candidate;
                if (!ProjectToFloor(*World, Start + Offset, HalfHeight, Candidate) || !IsClear(Candidate))
                {
                    continue;
                }

                FTransform Placement = SpawnTransform;
                Placement.SetLocation(Candidate);
                return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Placement);
            }
        }
    }

    UE_LOG(LogSampleMultiplayerSpawn, Warning,
        TEXT("No unoccupied ground near %s for %s (%d active controllers)"),
        *Start.ToString(), *GetNameSafe(NewPlayer), PlayerCount);
    return nullptr;
}
