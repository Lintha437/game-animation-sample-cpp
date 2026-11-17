#include "Animation/SampleAnimationTickComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
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
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    const TCHAR* ComponentPaths[] = {
        TEXT("/Game/Blueprints/AC_PreCMCTick.AC_PreCMCTick_C"),
        TEXT("/Game/Blueprints/AC_PostABPTick.AC_PostABPTick_C")
    };

    for (const TCHAR* Path : ComponentPaths)
    {
        UClass* Class = LoadClass<USampleAnimationTickComponent>(nullptr, Path);
        if (!TestNotNull(Path, Class))
        {
            return false;
        }
        const UFunction* BlueprintTick = Class->FindFunctionByName(TEXT("ReceiveTick"));
        TestTrue(TEXT("Tick has no Blueprint bytecode"),
            BlueprintTick && BlueprintTick->Script.IsEmpty());

        AActor* Owner = World->SpawnActor<AActor>();
        if (!TestNotNull(TEXT("Component owner"), Owner))
        {
            return false;
        }

        USampleAnimationTickComponent* Component = NewObject<USampleAnimationTickComponent>(Owner, Class);
        Component->RegisterComponent();
        USceneComponent* Listener = NewObject<USceneComponent>(Owner);
        Listener->Activate(true);
        const FMulticastInlineDelegateProperty* Property =
            FindFProperty<FMulticastInlineDelegateProperty>(Class, TEXT("Tick"));
        if (!TestNotNull(TEXT("Serialized Tick delegate"), Property))
        {
            return false;
        }

        FScriptDelegate Callback;
        Callback.BindUFunction(Listener, GET_FUNCTION_NAME_CHECKED(UActorComponent, Deactivate));
        void* DelegateValue = Property->ContainerPtrToValuePtr<void>(Component);
        Property->AddDelegate(Callback, Component, DelegateValue);
        TestTrue(TEXT("Listener bound to serialized dispatcher"),
            Property->GetMulticastDelegate(DelegateValue)->IsBound());

        TestTrue(TEXT("Listener starts active"), Listener->IsActive());
        TestTrue(TEXT("Component can be scheduled for tick"), Component->PrimaryComponentTick.bCanEverTick);
        Component->TickComponent(1.0f / 60.0f, LEVELTICK_All, &Component->PrimaryComponentTick);
        TestFalse(TEXT("Native tick ran the bound delegate"), Listener->IsActive());
    }
    return true;
}

#endif
