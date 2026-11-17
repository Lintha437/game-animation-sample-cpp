#include "../SampleSmartObjectInteractionComponent.h"
#include "../SampleSmartObjectTasks.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSmartObjectClaimArbitrationTest,
    "GameAnimationSampleCpp.Migration.SmartObjectClaimArbitration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSmartObjectClaimArbitrationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();

    auto* Subsystem = USmartObjectSubsystem::GetCurrent(World);
    AActor* Player = World->SpawnActor<AActor>();
    AActor* OtherUser = World->SpawnActor<AActor>();
    if (!TestTrue(TEXT("Authoritative runtime and users"), Subsystem && Player && OtherUser))
    {
        return false;
    }

    auto* Definition = NewObject<USmartObjectDefinition>(World);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Definition->DebugAddSlot().BehaviorDefinitions.Add(
            NewObject<UGameplayInteractionSmartObjectBehaviorDefinition>(Definition));
    }
    if (!TestTrue(TEXT("Two-slot definition validates"), Definition->Validate()))
    {
        return false;
    }

    const auto AddObject = [&]() -> USmartObjectComponent*
    {
        AActor* Actor = World->SpawnActor<AActor>();
        if (!Actor)
        {
            return nullptr;
        }
        auto* Component = NewObject<USmartObjectComponent>(Actor);
        Actor->SetRootComponent(Component);
        Actor->AddInstanceComponent(Component);
        Component->SetDefinition(Definition);
        Component->RegisterComponent();
        if (!Component->GetRegisteredHandle().IsValid() && !Subsystem->RegisterSmartObject(Component))
        {
            return nullptr;
        }
        return Component;
    };
    USmartObjectComponent* Target = AddObject();
    USmartObjectComponent* Unrelated = AddObject();
    if (!TestTrue(TEXT("Two separate Smart Objects are registered"), Target && Unrelated))
    {
        return false;
    }
    TArray<FSmartObjectSlotHandle> Slots;
    TArray<FSmartObjectSlotHandle> UnrelatedSlots;
    Subsystem->GetAllSlots(Target->GetRegisteredHandle(), Slots);
    Subsystem->GetAllSlots(Unrelated->GetRegisteredHandle(), UnrelatedSlots);
    if (!TestTrue(TEXT("Both objects have two runtime slots"), Slots.Num() == 2 && UnrelatedSlots.Num() == 2))
    {
        return false;
    }

    UClass* TaskClass = LoadClass<USampleClaimSmartObjectTask>(nullptr,
        TEXT("/Game/Blueprints/SmartObjects/TasksAndConditions/STT_ClaimSlot.STT_ClaimSlot_C"));
    UClass* ComponentClass = LoadClass<USampleSmartObjectInteractionComponent>(nullptr,
        TEXT("/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation.AC_SmartObjectAnimation_C"));
    if (!TestTrue(TEXT("Imported task and component preserve their native parents"), TaskClass && ComponentClass))
    {
        return false;
    }
    auto* Task = NewObject<USampleClaimSmartObjectTask>(Player, TaskClass);
    auto* Interaction = NewObject<USampleSmartObjectInteractionComponent>(Player, ComponentClass);
    Player->AddInstanceComponent(Interaction);
    Interaction->RegisterComponent();
    ON_SCOPE_EXIT { Task->ReleasePendingClaim(false); };

    const auto Claim = [&](const FSmartObjectSlotHandle& Slot)
    {
        return USampleClaimSmartObjectTask::ArbitrateClaim(*Subsystem, Slot, *Player, Target->GetOwner());
    };
    const auto Preferred = Claim(Slots[0]);
    if (!TestTrue(TEXT("Free selected slot is preferred"), Preferred.IsValid() && Preferred.SlotHandle == Slots[0]))
    {
        return false;
    }
    if (!TestTrue(TEXT("Pending native claim is registered"), Task->TrackPendingClaim(*Subsystem, Preferred)))
    {
        return false;
    }
    TestTrue(TEXT("Aborting the pending claim releases it"), Task->ReleasePendingClaim(false));
    TestTrue(TEXT("Abort leaves the selected slot free"), Subsystem->GetSlotState(Slots[0]) == ESmartObjectSlotState::Free);

    const auto Blocker = Subsystem->MarkSlotAsClaimed(Slots[0], ESmartObjectClaimPriority::Low,
        FConstStructView::Make(FSmartObjectActorUserData(OtherUser)));
    if (!TestTrue(TEXT("Low-priority competing claim"), Blocker.IsValid()))
    {
        return false;
    }
    ON_SCOPE_EXIT { Subsystem->MarkSlotAsFree(Blocker); };
    const auto Fallback = Claim(Slots[0]);
    if (!TestTrue(TEXT("Contention falls back to the other slot on the same object"),
        Fallback.IsValid() && Fallback.SlotHandle == Slots[1]))
    {
        return false;
    }
    if (!TestTrue(TEXT("Fallback claim is tracked"), Task->TrackPendingClaim(*Subsystem, Fallback)))
    {
        return false;
    }
    TestFalse(TEXT("Both slots claimed causes failure instead of priority override or cross-object search"), Claim(Slots[0]).IsValid());
    TestTrue(TEXT("Unrelated object remains free"), Subsystem->GetSlotState(UnrelatedSlots[0]) == ESmartObjectSlotState::Free);

    const auto Replacement = Subsystem->MarkSlotAsClaimed(Slots[1], ESmartObjectClaimPriority::High,
        FConstStructView::Make(FSmartObjectActorUserData(OtherUser)));
    if (!TestTrue(TEXT("External higher-priority claim revokes the pending fallback"), Replacement.IsValid()))
    {
        return false;
    }
    ON_SCOPE_EXIT { Subsystem->MarkSlotAsFree(Replacement); };
    TestTrue(TEXT("Revoked cleanup is idempotent"), Task->ReleasePendingClaim(false) && Task->ReleasePendingClaim(false));
    TestTrue(TEXT("Replacement remains claimed"), Subsystem->GetSlotState(Slots[1]) == ESmartObjectSlotState::Claimed);
    bool bReplacementDataIntact = false;
    Subsystem->ReadSlotData(Slots[1], [&](FConstSmartObjectSlotView View)
    {
        const auto* UserData = View.GetUserData().GetPtr<const FSmartObjectActorUserData>();
        bReplacementDataIntact = UserData && UserData->UserActor.Get() == OtherUser;
    });
    TestTrue(TEXT("Stale cleanup preserves the replacement user's data"), bReplacementDataIntact);

    const auto Transfer = USampleClaimSmartObjectTask::ArbitrateClaim(*Subsystem, UnrelatedSlots[0], *Player, Unrelated->GetOwner());
    if (!TestTrue(TEXT("Transfer fixture claims a free slot"), Transfer.IsValid()) ||
        !TestTrue(TEXT("Transfer fixture is tracked"), Task->TrackPendingClaim(*Subsystem, Transfer)))
    {
        return false;
    }
    Task->ForgetPendingClaim();
    TestTrue(TEXT("Native cleanup after ownership transfer is harmless"), Task->ReleasePendingClaim(false));
    TestTrue(TEXT("Transferred slot is still claimed"), Subsystem->GetSlotState(Transfer.SlotHandle) == ESmartObjectSlotState::Claimed);
    TestTrue(TEXT("Transferred owner can release the slot"), Subsystem->MarkSlotAsFree(Transfer));

    const auto* AuthoredActivity = FindFProperty<FBoolProperty>(ComponentClass, TEXT("PlayerIsInteracting"));
    if (!TestNotNull(TEXT("Authored activity flag"), AuthoredActivity))
    {
        return false;
    }
    Interaction->PendingPlayerInteractionTarget = Target->GetOwner();
    Interaction->PendingPlayerInteractionSlot = Slots[0];
    Interaction->PlayerInteractionRequestId = 1;
    Interaction->SetPlayerInteractionState(true);
    TestTrue(TEXT("Wrong target and slot do not match an accepted request"),
        Interaction->GetPlayerInteractionRequestId(Unrelated->GetOwner(), Slots[0]) == 0 &&
        Interaction->GetPlayerInteractionRequestId(Target->GetOwner(), Slots[1]) == 0);
    const uint64 RequestId = Interaction->GetPlayerInteractionRequestId(Target->GetOwner(), Slots[0]);
    TestTrue(TEXT("Exact request has a nonzero identity"), RequestId != 0);
    ++Interaction->PlayerInteractionRequestId;
    Interaction->ResetPlayerInteractionAfterFailedClaim(RequestId);
    TestTrue(TEXT("Unmatched rejection preserves the accepted activity"), Interaction->bPlayerInteractionActive);
    Interaction->ResetPlayerInteractionAfterFailedClaim(
        Interaction->GetPlayerInteractionRequestId(Target->GetOwner(), Slots[0]));
    TestFalse(TEXT("Matched rejection clears native activity"), Interaction->bPlayerInteractionActive);
    TestFalse(TEXT("Matched rejection clears authored activity"), AuthoredActivity->GetPropertyValue_InContainer(Interaction));
    TestFalse(TEXT("Matched rejection forgets request identity"), Interaction->PendingPlayerInteractionSlot.IsValid());
    return true;
}

#endif
