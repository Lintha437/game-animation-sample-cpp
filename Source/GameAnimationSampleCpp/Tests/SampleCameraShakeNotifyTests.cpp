#include "../SampleCameraShakeAnimNotify.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimTypes.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCameraShakeNotifyMigrationTest,
    "GameAnimationSampleCpp.Migration.CameraShakeNotify",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCameraShakeNotifyMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USampleCameraShakeAnimNotify>(nullptr,
        TEXT("/Game/Blueprints/AnimNotifies/BP_AnimNotify_SimpleCameraShake.BP_AnimNotify_SimpleCameraShake_C"));
    if (!TestNotNull(TEXT("Migrated notify class"), Class))
    {
        return false;
    }

    for (const FName FunctionName : {FName(TEXT("GetNotifyName")), FName(TEXT("Received_Notify"))})
    {
        const UFunction* Function = Class->FindFunctionByName(FunctionName);
        TestTrue(*FunctionName.ToString(), Function && Function->Script.IsEmpty());
    }
    USampleCameraShakeAnimNotify* Notify = NewObject<USampleCameraShakeAnimNotify>(GetTransientPackage(), Class);
    TestEqual(TEXT("Native track label"), Notify->GetNotifyName(), FString(TEXT("CameraShake")));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    AActor* Owner = World->SpawnActor<AActor>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Mesh owner"), Owner) ||
        !TestNotNull(TEXT("Player controller"), Controller))
    {
        return false;
    }

    APlayerCameraManager* Manager = Controller->PlayerCameraManager;
    if (!TestNotNull(TEXT("Camera manager"), Manager))
    {
        return false;
    }
    UCameraModifier_CameraShake* Modifier = Cast<UCameraModifier_CameraShake>(
        Manager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
    if (!TestNotNull(TEXT("Camera shake modifier"), Modifier))
    {
        return false;
    }

    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    Owner->SetRootComponent(Mesh);
    Mesh->RegisterComponent();
    FMinimalViewInfo Camera;
    Camera.Location = FVector::ZeroVector;
    Manager->FillCameraCache(Camera);
    const FAnimNotifyEventReference EventReference;
    const auto TestShake = [&](double Distance, float ExpectedScale)
    {
        Manager->StopAllCameraShakes(true);
        Owner->SetActorLocation(FVector(Distance, 0.0, 0.0));
        Notify->Notify(Mesh, nullptr, EventReference);
        TArray<FActiveCameraShakeInfo> ActiveShakes;
        Modifier->GetActiveCameraShakes(ActiveShakes);
        if (TestEqual(TEXT("One shake is dispatched"), ActiveShakes.Num(), 1) &&
            TestNotNull(TEXT("Shake instance"), ActiveShakes[0].ShakeInstance.Get()))
        {
            TestEqual(TEXT("Radial shake scale"), ActiveShakes[0].ShakeInstance->ShakeScale, ExpectedScale);
        }
    };
    TestShake(100.0, 1.0f);
    TestShake(750.0, 0.5f);
    TestShake(2000.0, 0.0f);
    return true;
}

#endif
