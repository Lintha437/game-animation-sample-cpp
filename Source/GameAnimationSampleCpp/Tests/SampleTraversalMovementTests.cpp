#include "../SampleTraversalComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "MoverDataModelTypes.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleTraversalWarpGeometryTest,
    "GameAnimationSampleCpp.Migration.TraversalWarpGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleTraversalWarpGeometryTest::RunTest(const FString& Parameters)
{
    const FVector BackLedge(100.0, 200.0, 300.0);
    const FVector BackNormal(0.6, 0.8, 0.0);
    const FVector BackFloor(40.0, 50.0, 60.0);
    const FVector Expected(130.0, 240.0, 60.0);
    FVector Location = FVector::ZeroVector;
    TestTrue(TEXT("Valid animation distances resolve the floor target"),
        USampleTraversalComponent::ResolveTraversalBackFloorLocation(
            BackLedge, BackNormal, BackFloor, 70.0, 120.0, Location));
    TestTrue(TEXT("The ledge and curve delta determine XY and the floor determines Z"), Location.Equals(Expected));
    TestTrue(TEXT("Reversed curve distance order produces the same target"),
        USampleTraversalComponent::ResolveTraversalBackFloorLocation(
            BackLedge, BackNormal, BackFloor, 120.0, 70.0, Location) && Location.Equals(Expected));
    TestFalse(TEXT("Non-finite samples are rejected"),
        USampleTraversalComponent::ResolveTraversalBackFloorLocation(
            BackLedge, BackNormal, BackFloor, std::numeric_limits<double>::infinity(), 70.0, Location));
    TestTrue(TEXT("A failed query leaves its output unchanged"), Location.Equals(Expected));
    const double MaxDistance = std::numeric_limits<double>::max();
    TestFalse(TEXT("Overflowing sample deltas are rejected"),
        USampleTraversalComponent::ResolveTraversalBackFloorLocation(
            BackLedge, BackNormal, BackFloor, MaxDistance, -MaxDistance, Location));
    TestTrue(TEXT("Overflow leaves the previous output unchanged"), Location.Equals(Expected));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleTraversalMovementMigrationTest,
    "GameAnimationSampleCpp.Migration.TraversalMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleTraversalMovementMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USampleTraversalComponent>(
        nullptr, TEXT("/Game/Blueprints/AC_TraversalLogic.AC_TraversalLogic_C"));
    if (!TestNotNull(TEXT("Saved traversal component class"), Class))
    {
        return false;
    }
    for (FName Hook : {FName(TEXT("SetMovementMode_CMC")), FName(TEXT("GetEndMovementMode_CMC")),
                      FName(TEXT("SetReplicationBehavior"))})
    {
        const UFunction* Function = Class->FindFunctionByName(Hook);
        TestTrue(FString::Printf(TEXT("%s resolves to the native parent"), *Hook.ToString()),
            Function && Function->GetOuter() == USampleTraversalComponent::StaticClass());
    }
    for (FName Hook : {FName(TEXT("PerformTraversalAction_CMC")), FName(TEXT("PerformTraversalAction_Mover")),
                      FName(TEXT("SetWarpTargets"))})
    {
        const UFunction* Function = Class->FindFunctionByName(Hook);
        TestTrue(FString::Printf(TEXT("%s retains its authored implementation"), *Hook.ToString()),
            Function && !Function->Script.IsEmpty());
    }

    const UFunction* ServerRequest = Class->FindFunctionByName(TEXT("PerformTraversalAction_Server"));
    const UFunction* Multicast = Class->FindFunctionByName(TEXT("PerformTraversalAction_Clients"));
    TestTrue(TEXT("The native adapter preserves the typed server RPC"),
        ServerRequest && ServerRequest->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer));
    TestTrue(TEXT("The native adapter preserves the typed multicast RPC"),
        Multicast && Multicast->HasAllFunctionFlags(FUNC_Net | FUNC_NetMulticast));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    ACharacter* Character = World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("CMC owner"), Character))
    {
        return false;
    }
    USampleTraversalComponent* Traversal = NewObject<USampleTraversalComponent>(Character, Class);
    Character->AddInstanceComponent(Traversal);
    Traversal->RegisterComponent();
    TestTrue(TEXT("A pawn without local control receives the multicast"), Traversal->ShouldReceiveTraversalReplay());
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }
    Controller->SetAsLocalPlayerController();
    Controller->Possess(Character);
    TestFalse(TEXT("The locally controlled pawn skips its own multicast"), Traversal->ShouldReceiveTraversalReplay());
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Traversal->SetMovementMode_CMC(MOVE_Flying);
    TestEqual(TEXT("Starting traversal selects flying"), Movement->MovementMode.GetValue(), MOVE_Flying);

    const FStructProperty* Result = FindFProperty<FStructProperty>(Class, TEXT("TraversalResult"));
    if (!TestNotNull(TEXT("Authored traversal result"), Result))
    {
        return false;
    }
    void* ResultData = Result->ContainerPtrToValuePtr<void>(Traversal);
    const FProperty* Action = Result->Struct->FindPropertyByName(TEXT("ActionType_2_96B748004D5339073D1AB49946EAF925"));
    const FEnumProperty* Enum = CastField<FEnumProperty>(Action);
    const FByteProperty* Byte = CastField<FByteProperty>(Action);
    const UEnum* Definition = Enum ? Enum->GetEnum() : Byte ? Byte->Enum.Get() : nullptr;
    if (!TestNotNull(TEXT("Authored traversal action enum"), Definition))
    {
        return false;
    }

    const EMovementMode ExpectedModes[] = {MOVE_Walking, MOVE_Falling, MOVE_Walking, MOVE_Walking};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedModes); ++Index)
    {
        const FString Entry = FString::Printf(TEXT("NewEnumerator%d"), Index);
        const int64 Value = Definition->GetValueByNameString(Entry);
        if (!TestTrue(FString::Printf(TEXT("%s exists"), *Entry), Value != INDEX_NONE))
        {
            return false;
        }
        if (Enum)
        {
            Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(ResultData), Value);
        }
        else
        {
            Byte->SetPropertyValue_InContainer(ResultData, static_cast<uint8>(Value));
        }
        TestEqual(FString::Printf(TEXT("%s selects the reference exit mode"), *Entry),
            Traversal->GetEndMovementMode_CMC(), ExpectedModes[Index]);
    }
    Traversal->SetMovementMode_CMC(Traversal->GetEndMovementMode_CMC());
    TestEqual(TEXT("Finishing traversal restores walking"), Movement->MovementMode.GetValue(), MOVE_Walking);

    Traversal->SetReplicationBehavior(true);
    TestTrue(TEXT("CMC traversal disables server movement correction"),
        Movement->bIgnoreClientMovementErrorChecksAndCorrection);
    TestTrue(TEXT("CMC traversal accepts the client position"),
        Movement->bServerAcceptClientAuthoritativePosition);
    Traversal->SetReplicationBehavior(false);
    TestFalse(TEXT("CMC teardown restores movement correction"),
        Movement->bIgnoreClientMovementErrorChecksAndCorrection);
    TestFalse(TEXT("CMC teardown stops accepting authoritative client positions"),
        Movement->bServerAcceptClientAuthoritativePosition);

    Traversal->BeginCmcTraversalMode();
    TestEqual(TEXT("The native start enters CMC flying"), Movement->MovementMode.GetValue(), MOVE_Flying);
    TestTrue(TEXT("The native start enables the correction window"), Movement->bIgnoreClientMovementErrorChecksAndCorrection);
    Traversal->FinishCmcTraversalMode();
    TestEqual(TEXT("The native finish returns to the selected CMC mode"), Movement->MovementMode.GetValue(), MOVE_Walking);
    TestTrue(TEXT("The native finish retains the correction grace period"), Movement->bIgnoreClientMovementErrorChecksAndCorrection);
    World->GetTimerManager().ClearAllTimersForObject(Traversal);
    Traversal->SetReplicationBehavior(false);

    Controller->UnPossess();
    Character->SetAutonomousProxy(true);
    TestEqual(TEXT("The authority receives autonomous moves"), Character->GetRemoteRole(), ROLE_AutonomousProxy);
    Traversal->BeginCmcTraversalMode();
    Traversal->FinishCmcTraversalMode();
    TestEqual(TEXT("Authority waits for the owning client's exit mode"), Movement->MovementMode.GetValue(), MOVE_Flying);
    TestTrue(TEXT("Authority still accepts the owner's move"), Movement->bServerAcceptClientAuthoritativePosition);
    World->GetTimerManager().ClearAllTimersForObject(Traversal);
    Traversal->SetReplicationBehavior(false);
    Character->SetAutonomousProxy(false);

    const FBoolProperty* Mover = FindFProperty<FBoolProperty>(Class, TEXT("IsMoverCharacter"));
    if (!TestNotNull(TEXT("Authored Mover selector"), Mover))
    {
        return false;
    }
    Mover->SetPropertyValue_InContainer(Traversal, true);
    Traversal->SetReplicationBehavior(true);
    TestFalse(TEXT("Mover selection leaves CMC correction alone"),
        Movement->bIgnoreClientMovementErrorChecksAndCorrection);
    TestFalse(TEXT("Mover selection leaves the CMC client-position flag alone"),
        Movement->bServerAcceptClientAuthoritativePosition);

    const FBoolProperty* Activity = FindFProperty<FBoolProperty>(Class, TEXT("DoingTraversalAction"));
    if (!TestNotNull(TEXT("Authored traversal activity observed by the pawn"), Activity))
    {
        return false;
    }
    Traversal->SetTraversalActionActive(true);
    TestTrue(TEXT("The native start publishes traversal activity"), Activity->GetPropertyValue_InContainer(Traversal));
    Traversal->SetTraversalActionActive(false);
    TestFalse(TEXT("The native finish clears traversal activity"), Activity->GetPropertyValue_InContainer(Traversal));

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    UBoxComponent* FirstObstacle = NewObject<UBoxComponent>(Character);
    UBoxComponent* SecondObstacle = NewObject<UBoxComponent>(Character);
    Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    Capsule->IgnoreComponentWhenMoving(SecondObstacle, true);
    Traversal->BeginTraversalCollision(Capsule, FirstObstacle, Mesh);
    Traversal->BeginTraversalCollision(Capsule, FirstObstacle, Mesh);
    TestTrue(TEXT("Mover temporarily ignores the swept obstacle"), Capsule->GetMoveIgnoreComponents().Contains(FirstObstacle));
    TestEqual(TEXT("Mover mesh ignores WorldDynamic during the action"), Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Ignore);
    Traversal->BeginTraversalCollision(Capsule, SecondObstacle);
    TestFalse(TEXT("Retargeting releases the previous obstacle"), Capsule->GetMoveIgnoreComponents().Contains(FirstObstacle));
    TestEqual(TEXT("Changing to the CMC path restores the mesh response"), Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Overlap);
    Traversal->EndTraversalCollision();
    Traversal->EndTraversalCollision();
    TestTrue(TEXT("Cleanup preserves an ignore entry established by the host"),
        Capsule->GetMoveIgnoreComponents().Contains(SecondObstacle));

    Traversal->BeginTraversalCollision(Capsule, FirstObstacle, Mesh);
    Traversal->EndTraversalCollision();
    TestFalse(TEXT("Action finish clears the capsule's traversal ignore"), Capsule->GetMoveIgnoreComponents().Contains(FirstObstacle));
    TestEqual(TEXT("Action finish restores the previous response"), Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic), ECR_Overlap);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleTraversalCharacterStateTest,
    "GameAnimationSampleCpp.Migration.TraversalCharacterState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleTraversalCharacterStateTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    const auto Spawn = [World](const TCHAR* ClassPath)
    {
        UClass* Class = LoadClass<APawn>(nullptr, ClassPath);
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return Class ? World->SpawnActor<APawn>(Class, FTransform::Identity, SpawnParams) : nullptr;
    };
    const auto ReadPawnObject = [](const APawn& Pawn, FName Name)
    {
        const auto* Field = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), Name);
        return Field ? Field->GetObjectPropertyValue_InContainer(&Pawn) : nullptr;
    };
    const auto Verify = [this, &ReadPawnObject](APawn& Pawn, UObject* Capsule, UObject* Mesh,
        uint8 Mode, uint8 Gait, double Speed)
    {
        UFunction* Query = Pawn.FindFunction(TEXT("Get_PropertiesForTraversal"));
        const auto* Result = Query ? CastField<FStructProperty>(Query->GetReturnProperty()) : nullptr;
        if (!TestTrue(TEXT("Existing Blueprint interface returns its authored struct"),
            Result && Result->Struct->GetFName() == TEXT("S_CharacterPropertiesForTraversal")))
        {
            return;
        }
        FStructOnScope Args(Query);
        Pawn.ProcessEvent(Query, Args.GetStructMemory());
        const void* Data = Result->ContainerPtrToValuePtr<void>(Args.GetStructMemory());
        const auto CheckObject = [this, Data, Result](FName Name, UObject* Expected)
        {
            const auto* Field = FindFProperty<FObjectPropertyBase>(Result->Struct, Name);
            TestTrue(Name.ToString(), Field && Expected && Field->GetObjectPropertyValue_InContainer(Data) == Expected);
        };
        CheckObject(TEXT("Capsule_21_D1F3797D47A5FB49C3DFAE8FAB15AFCC"), Capsule);
        CheckObject(TEXT("Mesh_15_D47797BD4F40417B966E3BB7E0AC62D3"), Mesh);
        CheckObject(TEXT("MotionWarping_18_C9F4AD1440C92128F649A7BA8B49094B"),
            ReadPawnObject(Pawn, TEXT("MotionWarping")));
        const auto* ModeField = FindFProperty<FByteProperty>(Result->Struct, TEXT("MovementMode_22_F39395024EEBA400FBE9FB8AE0EF7350"));
        const auto* GaitField = FindFProperty<FByteProperty>(Result->Struct, TEXT("Gait_12_9093F6E14192D37D2F0223B94E64FF71"));
        const auto* SpeedField = FindFProperty<FDoubleProperty>(Result->Struct, TEXT("Speed_26_C6C3500C4C3030A343948F80DFD60AEE"));
        TestTrue(TEXT("Chooser movement mode"), ModeField && ModeField->GetPropertyValue_InContainer(Data) == Mode);
        TestTrue(TEXT("Chooser gait"), GaitField && GaitField->GetPropertyValue_InContainer(Data) == Gait);
        TestTrue(TEXT("Chooser horizontal speed"), SpeedField && FMath::IsNearlyEqual(SpeedField->GetPropertyValue_InContainer(Data), Speed));
    };

    auto* Cmc = Cast<ACharacter>(Spawn(TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C")));
    const auto* CmcGait = Cmc ? FindFProperty<FByteProperty>(Cmc->GetClass(), TEXT("Gait")) : nullptr;
    if (!TestTrue(TEXT("CMC pawn and gait"), Cmc && CmcGait))
    {
        return false;
    }
    auto* CmcMovement = Cmc->GetCharacterMovement();
    for (const EMovementMode Mode : {MOVE_None, MOVE_Walking, MOVE_NavWalking, MOVE_Falling, MOVE_Swimming, MOVE_Flying, MOVE_Custom})
    {
        // Assign the snapshot directly so NavWalking does not require a navigation system.
        CmcMovement->MovementMode = Mode;
        for (uint8 Gait = 0; Gait < 3; ++Gait)
        {
            CmcGait->SetPropertyValue_InContainer(Cmc, Gait);
            CmcMovement->Velocity = FVector(30.0, 40.0, -800.0) * Gait;
            Verify(*Cmc, Cmc->GetCapsuleComponent(), Cmc->GetMesh(), Mode <= MOVE_NavWalking ? 0 : 1, Gait, 50.0 * Gait);
        }
    }

    APawn* Mover = Spawn(TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
    auto* CharacterMover = Mover ? Mover->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
    const auto* Pre = Mover ? FindFProperty<FStructProperty>(Mover->GetClass(), TEXT("MoverCustomInputs_PreSim")) : nullptr;
    const auto* Post = Mover ? FindFProperty<FStructProperty>(Mover->GetClass(), TEXT("MoverCustomInputs_PostSim")) : nullptr;
    const auto* Gait = Post ? FindFProperty<FByteProperty>(Post->Struct, TEXT("Gait_5_13BB29FB446593C2412547A541C4C00B")) : nullptr;
    if (!TestTrue(TEXT("Mover pawn and compatible pre- and post-simulation gait"), CharacterMover && Pre && Post && Gait &&
        Pre->SameType(Post)))
    {
        return false;
    }

    const auto* DirectionalInput = FindFProperty<FStructProperty>(Mover->GetClass(), TEXT("MoverDefaultInputs_PostSim"));
    if (!TestTrue(TEXT("Finalized movement input"), DirectionalInput && DirectionalInput->Struct == FCharacterDefaultInputs::StaticStruct()))
    {
        return false;
    }
    DirectionalInput->ContainerPtrToValuePtr<FCharacterDefaultInputs>(Mover)->SetMoveInput(
        EMoveInputType::DirectionalIntent, FVector::RightVector * 0.5);
    UFunction* ApproachQuery = Mover->FindFunction(TEXT("GetTraversalCheckInputs"));
    const auto* Approach = ApproachQuery ? CastField<FStructProperty>(ApproachQuery->GetReturnProperty()) : nullptr;
    if (!TestTrue(TEXT("Existing Mover query returns its authored sweep"), Approach &&
        Approach->Struct->GetFName() == TEXT("S_TraversalCheckInputs")))
    {
        return false;
    }

    const FVector Velocity(300, 400, -800);
    for (const FName Mode : {FName(TEXT("Walking")), FName(TEXT("Falling"))})
    {
        const bool bOnGround = Mode == TEXT("Walking");
        FMoverSyncState State = CharacterMover->GetSyncState();
        State.MovementMode = Mode;
        State.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>().SetTransforms_WorldSpace(
            FVector::ZeroVector, FRotator::ZeroRotator, Velocity, FVector::ZeroVector);
        CharacterMover->OnSimulationStateInitialized(State, FMoverAuxStateContext());
        for (uint8 Value = 0; Value < 3; ++Value)
        {
            Gait->SetPropertyValue_InContainer(Pre->ContainerPtrToValuePtr<void>(Mover), (Value + 1) % 3);
            Gait->SetPropertyValue_InContainer(Post->ContainerPtrToValuePtr<void>(Mover), Value);
            Verify(*Mover, ReadPawnObject(*Mover, TEXT("Capsule")), ReadPawnObject(*Mover, TEXT("SkeletalMesh")),
                bOnGround ? 0 : 1, Value, 500.0);
        }

        FStructOnScope Args(ApproachQuery);
        Mover->ProcessEvent(ApproachQuery, Args.GetStructMemory());
        const void* Data = Approach->ContainerPtrToValuePtr<void>(Args.GetStructMemory());
        const auto CheckNumber = [this, Approach, Data](FName Name, double Expected)
        {
            const auto* Field = FindFProperty<FDoubleProperty>(Approach->Struct, Name);
            TestTrue(Name.ToString(), Field && FMath::IsNearlyEqual(Field->GetPropertyValue_InContainer(Data), Expected));
        };
        const auto CheckVector = [this, Approach, Data](FName Name, const FVector& Expected)
        {
            const auto* Field = FindFProperty<FStructProperty>(Approach->Struct, Name);
            TestTrue(Name.ToString(), Field && Field->Struct == TBaseStructure<FVector>::Get() &&
                Field->ContainerPtrToValuePtr<FVector>(Data)->Equals(Expected));
        };
        CheckNumber(TEXT("TraceForwardDistance_2_07F489F3465D510BFC123689985564DC"), bOnGround ? 255.0 : 75.0);
        CheckNumber(TEXT("TraceRadius_10_FF044CA546FFD12BAA4D4E93460A273A"), 30.0);
        CheckNumber(TEXT("TraceHalfHeight_12_019A16DE47BB8535C3C0CF8AA6E355EB"), bOnGround ? 60.0 : 86.0);
        CheckVector(TEXT("TraceForwardDirection_5_84E06003419F502C44036B9424B39047"),
            bOnGround ? Velocity.GetSafeNormal() : FVector::RightVector);
        CheckVector(TEXT("TraceOriginOffset_7_2757E2FF43A7611D7F46EE9AAA15F8C0"), FVector::ZeroVector);
        CheckVector(TEXT("TraceEndOffset_15_35E488F2478DA8D4D7E69FB93494661B"),
            bOnGround ? FVector::ZeroVector : FVector(0, 0, 50));
    }
    return true;
}

#endif
