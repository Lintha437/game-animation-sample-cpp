#include "Characters/SampleMoverCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "MoverDataModelTypes.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSampleMoverDebugDrawTest, "GameAnimationSampleCpp.Migration.MoverDebugDraw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleMoverDebugDrawTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());

    UClass* Class = LoadClass<ASampleMoverCharacter>(nullptr,
        TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
    const FVector Location(100, 150, 200);
    const FRotator Rotation(0, 25, 0);
    auto* Pawn = Class ? World->SpawnActor<ASampleMoverCharacter>(Class, Location, Rotation) : nullptr;
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* LineBatch = World->GetLineBatcher(UWorld::ELineBatcherType::World);
    auto* Mover = Pawn ? Pawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
    auto* Capsule = Pawn ? Pawn->FindComponentByClass<UCapsuleComponent>() : nullptr;
    UFunction* Draw = Pawn ? Pawn->FindFunction(TEXT("DebugDraws")) : nullptr;
    const auto* Input = Pawn ? FindFProperty<FStructProperty>(Pawn->GetClass(), TEXT("MoverCustomInputs_PreSim")) : nullptr;
    const auto* Mode = Input ? FindFProperty<FByteProperty>(Input->Struct,
        TEXT("RotationMode_11_A4BE1D7943913AA41D3A5BB9A6A7B0A9")) : nullptr;
    const auto* Sector = Input ? FindFProperty<FByteProperty>(Input->Struct,
        TEXT("MovementDirection_2_19B243824010A37C0D54A7A28A5C6EFA")) : nullptr;
    const auto* Offset = Input ? FindFProperty<FNumericProperty>(Input->Struct,
        TEXT("RotationOffset_14_9438753848B6A37A4F76569318195C60")) : nullptr;
    const auto* Angle = Pawn ? FindFProperty<FNumericProperty>(Pawn->GetClass(), TEXT("DebugAngle")) : nullptr;
    auto* Shapes = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.DrawCharacterDebugShapes"));
    auto* Style = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.StrafeStyle"));
    auto* AimStyle = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.AimStyle"));
    const bool bFixture = TestNotNull(TEXT("Pawn"), Pawn) & TestNotNull(TEXT("Controller"), Controller) &
        TestNotNull(TEXT("Line batch"), LineBatch) & TestNotNull(TEXT("Mover"), Mover) &
        TestNotNull(TEXT("Capsule"), Capsule) & TestNotNull(TEXT("DebugDraws"), Draw) &
        TestNotNull(TEXT("Rotation mode"), Mode) & TestNotNull(TEXT("Direction sector"), Sector) &
        TestTrue(TEXT("Rotation offset"), Offset && Offset->IsFloatingPoint()) &
        TestTrue(TEXT("Debug angle"), Angle && Angle->IsFloatingPoint()) &
        TestNotNull(TEXT("Shapes CVar"), Shapes) & TestNotNull(TEXT("Strafe style CVar"), Style) &
        TestNotNull(TEXT("Aim style CVar"), AimStyle);
    if (!bFixture)
    {
        return false;
    }
    const int32 OriginalShapes = Shapes->GetInt();
    const int32 OriginalStyle = Style->GetInt();
    const int32 OriginalAimStyle = AimStyle->GetInt();
    const auto ShapesPriority = static_cast<EConsoleVariableFlags>(Shapes->GetFlags() & ECVF_SetByMask);
    const auto StylePriority = static_cast<EConsoleVariableFlags>(Style->GetFlags() & ECVF_SetByMask);
    const auto AimPriority = static_cast<EConsoleVariableFlags>(AimStyle->GetFlags() & ECVF_SetByMask);
    ON_SCOPE_EXIT
    {
        Shapes->Set(OriginalShapes, ShapesPriority);
        Style->Set(OriginalStyle, StylePriority);
        AimStyle->Set(OriginalAimStyle, AimPriority);
    };

    Shapes->Set(1, ShapesPriority);
    LineBatch->Flush();
    Pawn->ProcessEvent(Draw, nullptr);
    TestTrue(TEXT("Unpossessed pawn leaves the draw buffer empty"), LineBatch->BatchedLines.IsEmpty());

    Controller->Player = NewObject<ULocalPlayer>(GEngine);
    Pawn->SetController(Controller);
    if (!TestTrue(TEXT("The local player owns the debug pawn"), Pawn->IsLocallyControlled()))
    {
        return false;
    }
    Shapes->Set(0, ShapesPriority);
    Pawn->ProcessEvent(Draw, nullptr);
    TestTrue(TEXT("Disabled debug drawing leaves the draw buffer empty"), LineBatch->BatchedLines.IsEmpty());

    Shapes->Set(1, ShapesPriority);
    Style->Set(0, StylePriority);
    auto* InputData = Input->ContainerPtrToValuePtr<void>(Pawn);
    const auto Capture = [this, Pawn, Draw, LineBatch](const FString& Label)
    {
        LineBatch->Flush();
        Pawn->ProcessEvent(Draw, nullptr);
        TestFalse(Label + TEXT(" submits debug lines"), LineBatch->BatchedLines.IsEmpty());
        FString Geometry;
        for (const FBatchedLine& Line : LineBatch->BatchedLines)
        {
            Geometry += FString::Printf(TEXT("%.3f,%.3f,%.3f;%.3f,%.3f,%.3f;%08x;%.3f;%u\n"),
                Line.Start.X, Line.Start.Y, Line.Start.Z, Line.End.X, Line.End.Y, Line.End.Z,
                Line.Color.ToFColor(false).DWColor(), Line.Thickness, static_cast<uint32>(Line.DepthPriority));
        }
        const FTCHARToUTF8 Bytes(*Geometry);
        FSHAHash Hash;
        FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash.Hash);
        AddInfo(FString::Printf(TEXT("Mover debug geometry %s: %d %s"), *Label,
            LineBatch->BatchedLines.Num(), *Hash.ToString()));
        return Hash.ToString();
    };
    Mode->SetPropertyValue_InContainer(InputData, 0);
    Offset->SetFloatingPointPropertyValue(Offset->ContainerPtrToValuePtr<void>(InputData), 0.0);
    const FString IdleGeometry = Capture(TEXT("idle"));
    FMoverSyncState State = Mover->GetSyncState();
    State.MovementMode = TEXT("Walking");
    auto& Sync = State.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
    Sync.SetTransforms_WorldSpace(Location, Rotation, FVector::ZeroVector, FVector::ZeroVector);
    Sync.MoveDirectionIntent = FVector(0, 0.5, 0);
    Mover->OnSimulationStateInitialized(State, FMoverAuxStateContext());
    Angle->SetFloatingPointPropertyValue(Angle->ContainerPtrToValuePtr<void>(Pawn), 42.0);
    const FString IntentGeometry = Capture(TEXT("movement intent"));
    TestNotEqual(TEXT("Movement intent adds its direction and angle"), IntentGeometry, IdleGeometry);

    Mode->SetPropertyValue_InContainer(InputData, 1);
    Offset->SetFloatingPointPropertyValue(Offset->ContainerPtrToValuePtr<void>(InputData), -30.0);
    for (uint8 Direction = 0; Direction < 6; ++Direction)
    {
        Sector->SetPropertyValue_InContainer(InputData, Direction);
        Capture(FString::Printf(TEXT("strafe sector %u"), Direction));
    }

    UFunction* Query = Pawn->FindFunction(TEXT("Get_MovementDirectionThresholds"));
    const auto* Result = Query ? CastField<FStructProperty>(Query->GetReturnProperty()) : nullptr;
    if (!TestTrue(TEXT("Threshold query returns its authored struct"), Result &&
        Result->Struct->GetFName() == TEXT("S_MovementDirectionThresholds")))
    {
        return false;
    }
    const FName Fields[] = {
        TEXT("FL_3_747C69604F939E7EFF347D97BBD8A811"), TEXT("FR_5_790F2C4742CB53A5585DBFAF660F9CF0"),
        TEXT("BL_7_630ADC5A4B6C3AF9DA182E816EDB311B"), TEXT("BR_9_67DF35F84900050330F770A709E84CF7")
    };
    for (uint8 RotationMode = 0; RotationMode < 3; ++RotationMode)
    {
        Mode->SetPropertyValue_InContainer(InputData, RotationMode);
        for (int32 SelectedStyle = 0; SelectedStyle < 3; ++SelectedStyle)
        {
            const int32 OtherStyle = (SelectedStyle + 1) % 3;
            Style->Set(RotationMode == 2 ? OtherStyle : SelectedStyle, StylePriority);
            AimStyle->Set(RotationMode == 2 ? SelectedStyle : OtherStyle, AimPriority);
            for (uint8 Direction = 0; Direction < 6; ++Direction)
            {
                Sector->SetPropertyValue_InContainer(InputData, Direction);
                const double ExpectedBackward = SelectedStyle == 2 ? 180.0
                    : (Direction == 1 || (SelectedStyle == 0 && Direction == 0)) ? 120.0 : 140.0;
                const double ExpectedForward = SelectedStyle == 0 ? (Direction <= 1 ? 60.0 : 40.0) : ExpectedBackward;
                const double Values[] = {-ExpectedForward, ExpectedForward, -ExpectedBackward, ExpectedBackward};
                FStructOnScope Args(Query);
                Pawn->ProcessEvent(Query, Args.GetStructMemory());
                const auto* Output = Result->ContainerPtrToValuePtr<void>(Args.GetStructMemory());
                for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fields); ++Index)
                {
                    const auto* Field = FindFProperty<FNumericProperty>(Result->Struct, Fields[Index]);
                    if (TestTrue(Fields[Index].ToString(), Field && Field->IsFloatingPoint()))
                    {
                        TestEqual(FString::Printf(TEXT("%s in rotation %u, style %d, sector %u"),
                            *Fields[Index].ToString(), RotationMode, SelectedStyle, Direction),
                            Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(Output)), Values[Index]);
                    }
                }
            }
        }
    }
    return true;
}

#endif
