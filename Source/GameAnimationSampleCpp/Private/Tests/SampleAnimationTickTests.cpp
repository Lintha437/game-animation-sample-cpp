#include "Animation/SampleAnimationTickComponent.h"
#include "Characters/SampleCmcCharacter.h"
#include "Characters/SampleMoverCharacter.h"
#include "Characters/SampleVisualOverrideComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleAnimationTickMigrationTest,
    "GameAnimationSampleCpp.Migration.AnimationTickDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleAnimationTickMigrationTest::RunTest(const FString& Parameters)
{
    UClass* TickClass = USampleAnimationTickComponent::StaticClass();
    TestFalse(TEXT("Native tick component is instantiable"), TickClass->HasAnyClassFlags(CLASS_Abstract));

    const auto* TickCdo = GetDefault<USampleAnimationTickComponent>();
    TestTrue(TEXT("Native tick component can ever tick"), TickCdo->PrimaryComponentTick.bCanEverTick);
    TestEqual(TEXT("Native tick group"), static_cast<uint8>(TickCdo->PrimaryComponentTick.TickGroup),
        static_cast<uint8>(TG_DuringPhysics));
    TestEqual(TEXT("Native end tick group"), static_cast<uint8>(TickCdo->PrimaryComponentTick.EndTickGroup),
        static_cast<uint8>(TG_PrePhysics));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());

    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Component owner"), Owner))
    {
        return false;
    }

    USampleAnimationTickComponent* Component = NewObject<USampleAnimationTickComponent>(Owner, TickClass);
    Component->RegisterComponent();
    USceneComponent* Listener = NewObject<USceneComponent>(Owner);
    Listener->Activate(true);

    Component->BindTick(*Listener, GET_FUNCTION_NAME_CHECKED(UActorComponent, Deactivate));
    TestTrue(TEXT("Listener starts active"), Listener->IsActive());
    Component->TickComponent(1.0f / 60.0f, LEVELTICK_All, &Component->PrimaryComponentTick);
    TestFalse(TEXT("Native tick ran the bound callback"), Listener->IsActive());

    const auto* OverrideCdo = GetDefault<USampleVisualOverrideComponent>();
    TestFalse(TEXT("Visual override component matches CDO auto-activate default"), OverrideCdo->bAutoActivate);
    TestFalse(TEXT("Visual override component matches CDO replication default"), OverrideCdo->GetIsReplicated());

    for (const TCHAR* PawnPath : {
        TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"),
        TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"),
        TEXT("/Game/Blueprints/SandboxCharacter_Mover_Ragdoll.SandboxCharacter_Mover_Ragdoll_C")
    })
    {
        UClass* PawnClass = LoadClass<AActor>(nullptr, PawnPath);
        if (!TestNotNull(PawnPath, PawnClass))
        {
            return false;
        }
        AActor* Pawn = World->SpawnActor<AActor>(PawnClass);
        auto* Override = Pawn ? Pawn->FindComponentByClass<USampleVisualOverrideComponent>() : nullptr;
        if (!TestNotNull(*FString::Printf(TEXT("%s visual override component"), PawnPath), Override))
        {
            return false;
        }
        TestTrue(*FString::Printf(TEXT("%s visual override component is native"), PawnPath),
            Override->GetClass()->HasAnyClassFlags(CLASS_Native));

        if (!FString(PawnPath).EndsWith(TEXT("SandboxCharacter_Mover_C")))
        {
            auto* Tick = Pawn->FindComponentByClass<USampleAnimationTickComponent>();
            if (!TestNotNull(*FString::Printf(TEXT("%s tick component"), PawnPath), Tick))
            {
                return false;
            }
            TestTrue(*FString::Printf(TEXT("%s tick component is native"), PawnPath),
                Tick->GetClass()->HasAnyClassFlags(CLASS_Native));
        }
    }

    return true;
}

#endif
