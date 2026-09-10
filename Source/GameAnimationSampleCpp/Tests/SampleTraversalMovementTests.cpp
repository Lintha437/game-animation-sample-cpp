#include "../SampleTraversalComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

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

#endif
