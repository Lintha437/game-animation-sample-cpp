#include "Characters/SampleCmcCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleCmcCharacterMigrationTest,
    "GameAnimationSampleCpp.Migration.CmcCrouchSpeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleCmcCharacterMigrationTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<ASampleCmcCharacter>(nullptr,
        TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
    if (!TestNotNull(TEXT("Migrated CMC class"), Class))
    {
        return false;
    }

    const UFunction* CrouchSpeed = Class->FindFunctionByName(TEXT("CalculateMaxCrouchSpeed"));
    TestTrue(TEXT("Crouch speed resolves to native code"),
        CrouchSpeed && CrouchSpeed->HasAnyFunctionFlags(FUNC_Native) && CrouchSpeed->Script.IsEmpty());
    const UFunction* JumpGate = Class->FindFunctionByName(TEXT("CanPerformJumpAction"));
    TestTrue(TEXT("The authored montage gate still executes Blueprint code"),
        JumpGate && !JumpGate->HasAnyFunctionFlags(FUNC_Native) && !JumpGate->Script.IsEmpty());

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    ASampleCmcCharacter* Character = World->SpawnActor<ASampleCmcCharacter>(Class);
    if (!TestNotNull(TEXT("CMC character"), Character))
    {
        return false;
    }

    const FStructProperty* Speeds = FindFProperty<FStructProperty>(Class, TEXT("CrouchSpeeds"));
    const FObjectPropertyBase* CurveProperty = FindFProperty<FObjectPropertyBase>(Class, TEXT("StrafeSpeedMapCurve"));
    if (!TestNotNull(TEXT("Authored speeds property"), Speeds) ||
        !TestNotNull(TEXT("Authored curve property"), CurveProperty) ||
        !TestTrue(TEXT("Speed property is a vector"), Speeds->Struct == TBaseStructure<FVector>::Get()))
    {
        return false;
    }
    *Speeds->ContainerPtrToValuePtr<FVector>(Character) = FVector(300.0, 200.0, 100.0);

    UCurveFloat* Curve = NewObject<UCurveFloat>(Character);
    Curve->FloatCurve.AddKey(0.0f, 0.0f);
    Curve->FloatCurve.AddKey(90.0f, 1.0f);
    Curve->FloatCurve.AddKey(180.0f, 2.0f);
    CurveProperty->SetObjectPropertyValue_InContainer(Character, Curve);

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->Velocity = FVector(1.0, 0.0, 0.0);
    TestEqual(TEXT("Forward speed"), Character->CalculateMaxCrouchSpeed(), 300.0);
    Movement->Velocity = FVector(0.0, 1.0, 0.0);
    TestEqual(TEXT("Strafe speed"), Character->CalculateMaxCrouchSpeed(), 200.0);
    Movement->Velocity = FVector(-1.0, 0.0, 0.0);
    TestEqual(TEXT("Backward speed"), Character->CalculateMaxCrouchSpeed(), 100.0);
    Movement->Velocity = FVector(-1.0, -1.0, 0.0);
    TestEqual(TEXT("Backward diagonal interpolates the authored speeds"), Character->CalculateMaxCrouchSpeed(), 150.0);
    Movement->bOrientRotationToMovement = true;
    TestEqual(TEXT("Orient-to-movement uses forward speed"), Character->CalculateMaxCrouchSpeed(), 300.0);

    return true;
}

#endif
