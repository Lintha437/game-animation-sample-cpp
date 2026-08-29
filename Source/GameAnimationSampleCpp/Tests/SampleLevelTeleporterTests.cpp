#include "../SampleLevelTeleporter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleLevelTeleporterMigrationTest,
    "GameAnimationSampleCpp.Migration.LevelTeleporter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleLevelTeleporterMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleLevelTeleporter>(nullptr,
        TEXT("/Game/Levels/LevelPrototyping/Teleporter_Level.Teleporter_Level_C"));
    if (!TestNotNull(TEXT("Migrated portal class"), Class))
    {
        return false;
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    World->InitializeActorsForPlay(FURL());

    ASampleLevelTeleporter* Portal = World->SpawnActor<ASampleLevelTeleporter>(Class);
    AActor* Entrant = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Portal actor"), Portal) || !TestNotNull(TEXT("Entering actor"), Entrant))
    {
        return false;
    }

    const FObjectPropertyBase* TriggerProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("Trigger"));
    const FObjectPropertyBase* LabelProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("DestinationName"));
    const FTextProperty* DestinationProperty = FindFProperty<FTextProperty>(Class, TEXT("Destination"));
    if (!TestNotNull(TEXT("Trigger property"), TriggerProperty) ||
        !TestNotNull(TEXT("Label property"), LabelProperty) ||
        !TestNotNull(TEXT("Destination property"), DestinationProperty))
    {
        return false;
    }

    UPrimitiveComponent* Trigger = Cast<UPrimitiveComponent>(TriggerProperty->GetObjectPropertyValue_InContainer(Portal));
    const UTextRenderComponent* Label = Cast<UTextRenderComponent>(LabelProperty->GetObjectPropertyValue_InContainer(Portal));
    if (!TestNotNull(TEXT("Authored trigger"), Trigger) || !TestNotNull(TEXT("Authored label"), Label))
    {
        return false;
    }

    const UFunction* UpdateName = Class->FindFunctionByName(TEXT("UpdateName"));
    TestTrue(TEXT("Label function has no Blueprint bytecode"), UpdateName && UpdateName->Script.IsEmpty());
    Portal->DispatchBeginPlay();

    DestinationProperty->SetPropertyValue_InContainer(Portal, FText::FromString(TEXT("MissingMapForTest")));
    Trigger->OnComponentBeginOverlap.Broadcast(Trigger, Entrant, nullptr, 0, false, FHitResult());
    TestTrue(TEXT("Missing destination queues no travel"), World->NextURL.IsEmpty());

    DestinationProperty->SetPropertyValue_InContainer(Portal, FText::FromString(TEXT("NPCLevel")));
    Portal->UpdateName();
    TestEqual(TEXT("Label uses the assigned destination"), Label->Text.ToString(), FString(TEXT("To\r\nNPCLevel")));

    Trigger->OnComponentBeginOverlap.Broadcast(Trigger, nullptr, nullptr, 0, false, FHitResult());
    TestTrue(TEXT("Missing entrant queues no travel"), World->NextURL.IsEmpty());

    Trigger->OnComponentBeginOverlap.Broadcast(Trigger, Entrant, nullptr, 0, false, FHitResult());
    TestEqual(TEXT("Authority queues the listen destination"), World->NextURL, FString(TEXT("/Game/Levels/NPCLevel?listen")));
    return true;
}

#endif
