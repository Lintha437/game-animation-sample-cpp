#include "SampleLookAtExample.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpectatorPawn.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr TCHAR LookAtZoneClassPath[] =
        TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Props/BP_LookAtPOI_BorderObj.BP_LookAtPOI_BorderObj_C");

    const FName DemoPlayerProperty(TEXT("Player Pawn"));

    AActor* ReadLookAtActor(const UObject& Owner, FName Name)
    {
        const auto* Field = FindFProperty<FObjectPropertyBase>(Owner.GetClass(), Name);
        return Field ? Cast<AActor>(Field->GetObjectPropertyValue_InContainer(&Owner)) : nullptr;
    }

    void WriteLookAtObject(UObject& Owner, FName Name, UObject* Value)
    {
        const auto* Field = FindFProperty<FObjectPropertyBase>(Owner.GetClass(), Name);
        if (Field && (!Value || Value->IsA(Field->PropertyClass)))
        {
            Field->SetObjectPropertyValue_InContainer(&Owner, Value);
        }
    }

    void WriteLookAtFlag(UObject& Owner, FName Name, bool bValue)
    {
        if (const auto* Field = FindFProperty<FBoolProperty>(Owner.GetClass(), Name))
        {
            Field->SetPropertyValue_InContainer(&Owner, bValue);
        }
    }
}

ASampleLookAtController::ASampleLookAtController()
    : DemoPlayerClass(FSoftObjectPath(TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C")))
    , DemoWidgetClass(FSoftObjectPath(TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Widget/WB_LookAtPOI__GUI.WB_LookAtPOI__GUI_C")))
{
}

void ASampleLookAtController::BeginPlay()
{
    Super::BeginPlay();
    if (PlayerCameraManager)
    {
        PlayerCameraManager->DefaultFOV = 32.0f;
    }
    if (IsLocalPlayerController())
    {
        if (UClass* WidgetClass = DemoWidgetClass.LoadSynchronous())
        {
            if (auto* Widget = CreateWidget<UUserWidget>(this, WidgetClass))
            {
                Widget->AddToViewport();
                DemoWidget = Widget;
                WriteLookAtObject(*this, TEXT("Level Widget"), Widget);
            }
        }
    }
}

void ASampleLookAtController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* Widget = DemoWidget.Get())
    {
        Widget->RemoveFromParent();
    }
    Super::EndPlay(EndPlayReason);
}

void ASampleLookAtController::EnterLookAtPlayerMode()
{
    UWorld* World = GetWorld();
    if (!World || !HasAuthority())
    {
        return;
    }
    APawn* DemoPlayer = Cast<APawn>(ReadLookAtActor(*this, DemoPlayerProperty));
    if (!IsValid(DemoPlayer))
    {
        UClass* PawnClass = DemoPlayerClass.LoadSynchronous();
        DemoPlayer = PawnClass ? World->SpawnActor<APawn>(PawnClass, FTransform(FVector(0.0, 0.0, 100.0))) : nullptr;
    }
    if (!IsValid(DemoPlayer))
    {
        return;
    }

    if (auto* CurrentSpectator = Cast<ASpectatorPawn>(GetPawn()))
    {
        PreviousSpectator = CurrentSpectator;
    }
    WriteLookAtObject(*this, DemoPlayerProperty, DemoPlayer);
    Possess(DemoPlayer);
    WriteLookAtFlag(*this, TEXT("Look At Player"), true);

    if (UClass* ZoneClass = FindObject<UClass>(nullptr, LookAtZoneClassPath))
    {
        for (TActorIterator<AActor> Zone(World, ZoneClass); Zone; ++Zone)
        {
            if (AActor* Gizmo = ReadLookAtActor(**Zone, TEXT("Gizmo Obj")); IsValid(Gizmo))
            {
                Gizmo->Destroy();
            }
        }
    }
}

void ASampleLookAtController::EnterLookAtSpectatorMode()
{
    UWorld* World = GetWorld();
    if (!World || !HasAuthority())
    {
        return;
    }

    ASpectatorPawn* Spectator = PreviousSpectator.Get();
    if (!IsValid(Spectator))
    {
        TActorIterator<ASpectatorPawn> Available(World);
        Spectator = Available ? *Available : nullptr;
    }
    if (!IsValid(Spectator))
    {
        return;
    }

    APawn* DemoPlayer = Cast<APawn>(ReadLookAtActor(*this, DemoPlayerProperty));
    WriteLookAtFlag(*this, TEXT("Look At Player"), false);
    WriteLookAtFlag(*this, TEXT("Look At Enabled"), false);
    Possess(Spectator);
    if (IsValid(DemoPlayer))
    {
        DemoPlayer->Destroy();
        WriteLookAtObject(*this, DemoPlayerProperty, nullptr);
    }

    if (UClass* ZoneClass = FindObject<UClass>(nullptr, LookAtZoneClassPath))
    {
        for (TActorIterator<AActor> Zone(World, ZoneClass); Zone; ++Zone)
        {
            UFunction* Deselect = Zone->FindFunction(TEXT("Deselect Zone"));
            if (Deselect && Deselect->NumParms == 0)
            {
                Zone->ProcessEvent(Deselect, nullptr);
            }
        }
    }
}
