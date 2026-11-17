#include "Traversal/SampleTraversalLedgeLibrary.h"
#include "Traversal/SampleTraversalClearance.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleTraversalLedgeMigrationTest,
    "GameAnimationSampleCpp.Migration.TraversalLedges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleTraversalLedgeMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<AActor>(nullptr,
        TEXT("/Game/Levels/LevelPrototyping/LevelBlock_Traversable.LevelBlock_Traversable_C"));
    if (!TestNotNull(TEXT("Saved traversal block class"), Class))
    {
        return false;
    }
    TestNull(TEXT("The Blueprint spline-processing graph was removed"), Class->FindFunctionByName(TEXT("FindLedgeClosestToActor")));
    UFunction* Entry = Class->FindFunctionByName(TEXT("GetLedgeTransforms"));
    if (!TestNotNull(TEXT("Original traversal block signature"), Entry))
    {
        return false;
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    AActor* Block = World->SpawnActor<AActor>(Class);
    if (!TestNotNull(TEXT("Traversal block instance"), Block))
    {
        return false;
    }

    const auto MakeSpline = [Block](double X)
    {
        USplineComponent* Spline = NewObject<USplineComponent>(Block);
        Block->AddInstanceComponent(Spline);
        Spline->ClearSplinePoints(false);
        Spline->AddSplinePoint(FVector(X, 0.0, 0.0), ESplineCoordinateSpace::Local, false);
        Spline->AddSplinePoint(FVector(X, 100.0, 0.0), ESplineCoordinateSpace::Local, true);
        Spline->RegisterComponent();
        return Spline;
    };
    USplineComponent* Front = MakeSpline(0.0);
    USplineComponent* Back = MakeSpline(100.0);
    const FArrayProperty* Ledges = FindFProperty<FArrayProperty>(Class, TEXT("Ledges"));
    const FMapProperty* Opposites = FindFProperty<FMapProperty>(Class, TEXT("OppositeLedges"));
    if (!TestNotNull(TEXT("Authored ledge list"), Ledges) || !TestNotNull(TEXT("Authored opposite map"), Opposites))
    {
        return false;
    }
    FScriptArrayHelper Array(Ledges, Ledges->ContainerPtrToValuePtr<void>(Block));
    Array.EmptyValues();
    const auto* Element = CastFieldChecked<FObjectPropertyBase>(Ledges->Inner);
    Element->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Front);
    Element->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Back);

    FScriptMapHelper Map(Opposites, Opposites->ContainerPtrToValuePtr<void>(Block));
    Map.EmptyValues();
    const int32 Pair = Map.AddDefaultValue_Invalid_NeedsRehash();
    CastFieldChecked<FObjectPropertyBase>(Opposites->KeyProp)->SetObjectPropertyValue(Map.GetKeyPtr(Pair), Front);
    CastFieldChecked<FObjectPropertyBase>(Opposites->ValueProp)->SetObjectPropertyValue(Map.GetValuePtr(Pair), Back);
    Map.Rehash();

    const FVector Hit = FVector::ZeroVector;
    const FVector PawnLocation(-60.0, 0.0, 0.0);
    const FSampleTraversalLedges Result = USampleTraversalLedgeLibrary::QueryLedges(*Block, Hit, PawnLocation);
    TestTrue(TEXT("Front pose clamps the hit away from the corner"),
        Result.Front && Result.Front->GetLocation().Equals(FVector(0.0, 30.0, 0.0), 0.1));
    TestTrue(TEXT("The opposite pose follows the clamped front point"),
        Result.Back && Result.Back->GetLocation().Equals(FVector(100.0, 30.0, 0.0), 0.1));

    FStructOnScope Params(Entry);
    void* Values = Params.GetStructMemory();
    *FindFProperty<FStructProperty>(Entry, TEXT("HitLocation"))->ContainerPtrToValuePtr<FVector>(Values) = Hit;
    *FindFProperty<FStructProperty>(Entry, TEXT("ActorLocation"))->ContainerPtrToValuePtr<FVector>(Values) = PawnLocation;
    const FStructProperty* InOut = FindFProperty<FStructProperty>(Entry, TEXT("TraversalTraceResultInOut"));
    if (!TestNotNull(TEXT("Existing by-reference result"), InOut))
    {
        return false;
    }
    void* Payload = InOut->ContainerPtrToValuePtr<void>(Values);
    const auto* HasFront = FindFProperty<FBoolProperty>(InOut->Struct, TEXT("HasFrontLedge_34_6C547E244F41599C4D38E1909503CCBA"));
    const auto* HasBack = FindFProperty<FBoolProperty>(InOut->Struct, TEXT("HasBackLedge_41_8550E97541A4EDBCA44DB0B16F1B51C4"));
    const auto* FrontLocation = FindFProperty<FStructProperty>(InOut->Struct,
        TEXT("FrontLedgeLocation_50_52B8B8A84F10B0E0A9F287972F2C54D8"));
    const auto* BackFloor = FindFProperty<FBoolProperty>(InOut->Struct,
        TEXT("HasBackFloor_25_AC42C42F4DD2BFF32A30BDAB394529DE"));
    if (!HasFront || !HasBack || !FrontLocation || !BackFloor)
    {
        AddError(TEXT("Required result fields were missing."));
        return false;
    }
    BackFloor->SetPropertyValue_InContainer(Payload, true);
    Block->ProcessEvent(Entry, Values);
    TestTrue(TEXT("The existing Blueprint call writes its front-ledge output"), HasFront->GetPropertyValue_InContainer(Payload));
    TestTrue(TEXT("The existing Blueprint call writes its back-ledge output"), HasBack->GetPropertyValue_InContainer(Payload));
    TestTrue(TEXT("The existing Blueprint call returns the clamped position"),
        FrontLocation->ContainerPtrToValuePtr<FVector>(Payload)->Equals(FVector(0.0, 30.0, 0.0), 0.1));
    TestTrue(TEXT("The call preserves unrelated result fields"), BackFloor->GetPropertyValue_InContainer(Payload));

    TInlineComponentArray<UPrimitiveComponent*> AuthoredCollision(Block);
    for (UPrimitiveComponent* Primitive : AuthoredCollision)
    {
        Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    const auto MakeBox = [Block](const FVector& Center, const FVector& Extent)
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(Block);
        Block->AddInstanceComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetWorldLocation(Center);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
        return Box;
    };
    UBoxComponent* FrontWall = MakeBox(FVector(50.0, 50.0, -55.0), FVector(50.0, 65.0, 55.0));
    MakeBox(FVector(145.0, 50.0, -130.0), FVector(45.0, 65.0, 30.0));
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Front->SetUpVectorAtSplinePoint(Index, -FVector::ForwardVector, ESplineCoordinateSpace::World);
        Back->SetUpVectorAtSplinePoint(Index, FVector::ForwardVector, ESplineCoordinateSpace::World);
    }
    ACharacter* Pawn = World->SpawnActor<ACharacter>(FVector(-115.0, 50.0, -55.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Traversing pawn"), Pawn))
    {
        return false;
    }
    FSampleTraversalApproach Approach;
    Approach.Direction = FVector::ForwardVector;
    Approach.Distance = 200.0;
    Approach.Radius = 12.0f;
    Approach.HalfHeight = 15.0f;
    const auto Query = [&]()
    {
        return FSampleTraversalClearanceQuery::Query(*Pawn, Approach, 20.0f, 60.0f, EDrawDebugTrace::None, 0, 0.0f);
    };
    const FSampleTraversalClearance Clear = Query();
    TestTrue(TEXT("The approach sweep identifies the actual obstacle component"), Clear.Obstacle.Get() == FrontWall);
    TestTrue(TEXT("Clear approach and top"), Clear.bCanReachFront && Clear.bCanCrossTop);
    TestEqual(TEXT("Height is measured from the pawn's feet"), Clear.Height, 115.0, 0.1);
    TestEqual(TEXT("Open top depth is measured between the ledges"), Clear.Depth, 100.0, 0.1);
    TestTrue(TEXT("The floor sweep returns the far landing"), Clear.BackFloor.IsSet());
    TestEqual(TEXT("The far landing height is measured from the back ledge"), Clear.BackHeight, 100.0, 0.2);

    UBoxComponent* Overhead = MakeBox(FVector(55.0, 50.0, 60.0), FVector(5.0, 40.0, 40.0));
    const FSampleTraversalClearance BlockedTop = Query();
    TestTrue(TEXT("A blocked top still permits reaching the front edge"), BlockedTop.bCanReachFront);
    TestFalse(TEXT("An obstruction above the ledge blocks crossing"), BlockedTop.bCanCrossTop);
    TestEqual(TEXT("Blocked top depth ends at the collision"), BlockedTop.Depth, 50.0, 0.5);
    TestFalse(TEXT("A blocked crossing skips the floor sweep"), BlockedTop.BackFloor.IsSet());
    Overhead->SetWorldLocation(FVector(-50.0, 50.0, 85.0));
    TestFalse(TEXT("An obstruction on the approach blocks the front edge"), Query().bCanReachFront);

    Front->SetLocationAtSplinePoint(1, FVector(0.0, 40.0, 0.0), ESplineCoordinateSpace::Local, true);
    TestFalse(TEXT("A short front ledge is rejected"), USampleTraversalLedgeLibrary::QueryLedges(*Block, Hit, PawnLocation).Front.IsSet());
    return true;
}

#endif
