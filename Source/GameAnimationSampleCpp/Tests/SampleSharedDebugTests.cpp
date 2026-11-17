#if WITH_DEV_AUTOMATION_TESTS

#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "../SampleDebugDrawing.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename T>
    const T* DebugField(const UStruct* Struct, FName Name)
    {
        const auto* Field = FindFProperty<T>(Struct, Name);
        checkf(Field, TEXT("Missing debug fixture field %s"), *Name.ToString());
        return Field;
    }

    template <typename T>
    T& DebugInput(FStructOnScope& Args, FName Name)
    {
        return *DebugField<FProperty>(Args.GetStruct(), Name)->ContainerPtrToValuePtr<T>(Args.GetStructMemory());
    }

    void DebugNumber(FStructOnScope& Args, FName Name, double Value)
    {
        const auto* Property = DebugField<FNumericProperty>(Args.GetStruct(), Name);
        Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Args.GetStructMemory()), Value);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSampleSharedDebugDrawingTest, "GameAnimationSampleCpp.Migration.SharedDebugDrawing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSampleSharedDebugDrawingTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient game world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    AActor* Owner = World->SpawnActor<AActor>();
    UClass* Class = LoadClass<UObject>(nullptr,
        TEXT("/Game/Blueprints/Data/BFL_HelpfulFunctions.BFL_HelpfulFunctions_C"));
    if (!TestTrue(TEXT("Imported debug class and world context"), Class && Owner))
    {
        return false;
    }
    UObject* Library = Class->GetDefaultObject();
    UObject* Object = NewObject<UCurveFloat>();
    USceneComponent* Component = NewObject<USceneComponent>(Owner);
    const TArray<UObject*> Objects = {Object, nullptr, Owner, Component, Object, nullptr};
    const TArray<FString> ExpectedNames = {
        Object->GetName(), FString(), Owner->GetActorNameOrLabel(), Component->GetReadableName(),
        Object->GetName(), FString()
    };
    TestTrue(TEXT("Native object names preserve display format, order, duplicates and null entries"),
        USampleDebugDrawing::GetObjectNames(Objects) == ExpectedNames);
    TestTrue(TEXT("Native empty object array yields no names"), USampleDebugDrawing::GetObjectNames({}).IsEmpty());

    UFunction* ObjectNames = Library->FindFunction(TEXT("GetObjectNames"));
    const auto* ObjectInput = ObjectNames ? FindFProperty<FArrayProperty>(ObjectNames, TEXT("Objects")) : nullptr;
    const auto* NameReturn = ObjectNames ? CastField<FArrayProperty>(ObjectNames->GetReturnProperty()) : nullptr;
    if (TestTrue(TEXT("Imported GetObjectNames preserves object array and string array signature"),
        ObjectInput && CastField<FObjectPropertyBase>(ObjectInput->Inner) && NameReturn &&
        CastField<FStrProperty>(NameReturn->Inner)))
    {
        FStructOnScope Args(ObjectNames);
        DebugInput<TArray<UObject*>>(Args, TEXT("Objects")) = Objects;
        Library->ProcessEvent(ObjectNames, Args.GetStructMemory());
        const auto& Names = *NameReturn->ContainerPtrToValuePtr<TArray<FString>>(Args.GetStructMemory());
        TestTrue(TEXT("Imported object names match native display semantics"), Names == ExpectedNames);
        DebugInput<TArray<UObject*>>(Args, TEXT("Objects")).Reset();
        Library->ProcessEvent(ObjectNames, Args.GetStructMemory());
        TestTrue(TEXT("Imported empty object array yields no names"), Names.IsEmpty());
    }

    const TArray<ULineBatchComponent*> Batches = {
        World->GetLineBatcher(UWorld::ELineBatcherType::World),
        World->GetLineBatcher(UWorld::ELineBatcherType::Foreground)
    };
    const auto Capture = [this, Library, Owner, &Batches](FName Name, auto SetArguments)
    {
        UFunction* Function = Library->FindFunction(Name);
        if (!TestNotNull(Name.ToString(), Function))
        {
            return;
        }
        FStructOnScope Args(Function);
        DebugInput<UObject*>(Args, TEXT("__WorldContext")) = Owner;
        SetArguments(Args);
        for (ULineBatchComponent* Batch : Batches)
        {
            Batch->Flush();
        }
        Library->ProcessEvent(Function, Args.GetStructMemory());
        FString Geometry;
        int32 Count = 0;
        for (const ULineBatchComponent* Batch : Batches)
        {
            for (const FBatchedLine& Line : Batch->BatchedLines)
            {
                Geometry += FString::Printf(TEXT("%.3f,%.3f,%.3f;%.3f,%.3f,%.3f;%08x;%.3f;%u\n"),
                    Line.Start.X, Line.Start.Y, Line.Start.Z, Line.End.X, Line.End.Y, Line.End.Z,
                    Line.Color.ToFColor(false).DWColor(), Line.Thickness, static_cast<uint32>(Line.DepthPriority));
                ++Count;
            }
        }
        TestTrue(Name.ToString() + TEXT(" submits scene lines"), Count > 0);
        const FTCHARToUTF8 Bytes(*Geometry);
        FSHAHash Hash;
        FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash.Hash);
        AddInfo(FString::Printf(TEXT("Shared debug geometry %s: %d %s"), *Name.ToString(), Count, *Hash.ToString()));
    };
    const auto PlaceText = [](FStructOnScope& Args)
    {
        DebugInput<FVector>(Args, TEXT("Location")) = FVector(90, 60, 30);
        DebugInput<FRotator>(Args, TEXT("Rotation")) = FRotator(11, 35, 8);
        DebugInput<FVector>(Args, TEXT("Offset")) = FVector(14, 28, 42);
    };

    Capture(TEXT("DrawDebugArrowWithCircle"), [](FStructOnScope& Args)
    {
        DebugInput<bool>(Args, TEXT("DrawCircle")) = true;
        DebugInput<bool>(Args, TEXT("DrawAxis")) = true;
        DebugInput<bool>(Args, TEXT("DrawArrow")) = true;
        DebugInput<FVector>(Args, TEXT("Center")) = FVector(30, 15, 21);
        DebugInput<FVector>(Args, TEXT("Direction")) = FVector(0.6, -0.5, 0.3);
        DebugInput<FVector>(Args, TEXT("Offset")) = FVector(5, 9, 19);
        DebugInput<FLinearColor>(Args, TEXT("Color")) = FLinearColor::Blue;
        DebugInput<int32>(Args, TEXT("Segments")) = 32;
        DebugNumber(Args, TEXT("Radius"), 45);
        DebugNumber(Args, TEXT("Length"), 82);
        DebugNumber(Args, TEXT("Size"), 8);
        DebugNumber(Args, TEXT("Thickness"), 0.25);
    });
    Capture(TEXT("DrawDebugAngleThresholds"), [](FStructOnScope& Args)
    {
        DebugInput<FVector>(Args, TEXT("Center")) = FVector(30, 15, 21);
        DebugInput<FVector>(Args, TEXT("Offset")) = FVector(5, 9, 19);
        DebugInput<FRotator>(Args, TEXT("InRotation")) = FRotator(17, -20, 13);
        DebugInput<TArray<double>>(Args, TEXT("YawAngles")) = {-126, -41, 60, 144};
        DebugInput<FLinearColor>(Args, TEXT("Color")) = FLinearColor::Red;
        DebugNumber(Args, TEXT("StartRadius"), 23);
        DebugNumber(Args, TEXT("EndRadius"), 47);
        DebugNumber(Args, TEXT("Thickness"), 1);
    });
    Capture(TEXT("DebugDraw_BoolStates"), [&PlaceText](FStructOnScope& Args)
    {
        PlaceText(Args);
        DebugInput<TArray<FString>>(Args, TEXT("BoolNames")) = {TEXT("Grounded"), TEXT("Walking"), TEXT("Unspecified")};
        DebugInput<TArray<bool>>(Args, TEXT("BoolValues")) = {true, false, false};
    });
    Capture(TEXT("DebugDraw_StringArray"), [&PlaceText](FStructOnScope& Args)
    {
        PlaceText(Args);
        DebugInput<FString>(Args, TEXT("Label")) = TEXT("History");
        DebugInput<FString>(Args, TEXT("Prefix")) = TEXT("> ");
        DebugInput<TArray<FString>>(Args, TEXT("Strings")) = {TEXT("Stop"), TEXT("Walk"), TEXT("Run")};
        DebugInput<FString>(Args, TEXT("Highlighted String")) = TEXT("wAlK");
        DebugInput<FString>(Args, TEXT("Highlight")) = TEXT(" *");
    });
    Capture(TEXT("DebugDraw_ObjectNameArray"), [&PlaceText, Owner](FStructOnScope& Args)
    {
        PlaceText(Args);
        DebugInput<FString>(Args, TEXT("ArrayLabel")) = TEXT("Actors");
        DebugInput<TArray<UObject*>>(Args, TEXT("Objects")) = {Owner, nullptr};
    });
    Capture(TEXT("DebugDraw_MultiLineGraph"), [&PlaceText](FStructOnScope& Args)
    {
        PlaceText(Args);
        DebugInput<FVector2D>(Args, TEXT("GraphSize")) = FVector2D(47, 31);
        DebugInput<FVector2D>(Args, TEXT("MinValue")) = FVector2D(10, -5);
        DebugInput<FVector2D>(Args, TEXT("MaxValue")) = FVector2D(20, 5);
        DebugInput<FString>(Args, TEXT("Xaxis Label")) = TEXT("Time");
        DebugInput<FString>(Args, TEXT("Yaxis Label")) = TEXT("Speed");
        const auto* Lines = DebugField<FArrayProperty>(Args.GetStruct(), TEXT("Lines"));
        FScriptArrayHelper Entries(Lines, Lines->ContainerPtrToValuePtr<void>(Args.GetStructMemory()));
        const auto* Row = CastFieldChecked<FStructProperty>(Lines->Inner)->Struct.Get();
        const auto* Name = DebugField<FStrProperty>(Row, TEXT("Name_10_3306FBD140B223C953EF4AACC1AC6176"));
        const auto* Color = DebugField<FStructProperty>(Row, TEXT("Color_9_8B0F933E47F28862F20A129A698F0BD0"));
        const auto* Values = DebugField<FArrayProperty>(Row, TEXT("Values_8_8E822734407F401431999D9111E5F344"));
        Entries.AddValues(2);
        for (int32 Index = 0; Index < Entries.Num(); ++Index)
        {
            void* Entry = Entries.GetRawPtr(Index);
            Name->SetPropertyValue_InContainer(Entry, Index == 0 ? TEXT("Owner") : TEXT("Authority"));
            *Color->ContainerPtrToValuePtr<FLinearColor>(Entry) = Index == 0 ? FLinearColor::Blue : FLinearColor::Red;
            *Values->ContainerPtrToValuePtr<TArray<double>>(Entry) = {0.5 * Index, 2.0 + Index, -1.0, 4.0};
        }
    });

    UFunction* History = Library->FindFunction(TEXT("AddToStringHistoryArray"));
    if (TestNotNull(TEXT("Existing string history function"), History))
    {
        FStructOnScope Args(History);
        auto& Values = DebugInput<TArray<FString>>(Args, TEXT("InOutValues"));
        Values = {TEXT("a"), TEXT("b"), TEXT("c"), TEXT("d")};
        DebugInput<FString>(Args, TEXT("NewValue")) = TEXT("new");
        DebugInput<int32>(Args, TEXT("MaxHistoryCount")) = 2;
        Library->ProcessEvent(History, Args.GetStructMemory());
        TestTrue(TEXT("One removal preserves entries beyond the removed index"),
            Values == TArray<FString>{TEXT("new"), TEXT("a"), TEXT("c"), TEXT("d")});
    }
    return true;
}

#endif
