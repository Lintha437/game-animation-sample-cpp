#include "SampleLookAtExample.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpectatorPawn.h"
#include "UObject/StructOnScope.h"
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

    bool ReadLookAtFlag(const UObject& Owner, FName Name)
    {
        const auto* Field = FindFProperty<FBoolProperty>(Owner.GetClass(), Name);
        return Field && Field->GetPropertyValue_InContainer(&Owner);
    }

    void CallLookAtCallback(UObject& Owner, FName Name)
    {
        UFunction* Callback = Owner.FindFunction(Name);
        if (Callback && Callback->NumParms == 0)
        {
            Owner.ProcessEvent(Callback, nullptr);
        }
    }

    void ReportLookAtZone(APlayerController& Controller, APawn* Pawn, const FString& Type, AActor* Gizmo)
    {
        UFunction* Callback = Controller.FindFunction(TEXT("Zone Was Selected"));
        if (!Callback)
        {
            return;
        }
        const auto* PawnParameter = FindFProperty<FObjectPropertyBase>(Callback, TEXT("Active  Pawn"));
        const auto* TypeParameter = FindFProperty<FStrProperty>(Callback, TEXT("Type"));
        const auto* GizmoParameter = FindFProperty<FObjectPropertyBase>(Callback, TEXT("Gizmo Onj"));
        if (PawnParameter && TypeParameter && GizmoParameter && Callback->NumParms == 3 &&
            (!Pawn || Pawn->IsA(PawnParameter->PropertyClass)) && (!Gizmo || Gizmo->IsA(GizmoParameter->PropertyClass)))
        {
            FStructOnScope Arguments(Callback);
            void* Data = Arguments.GetStructMemory();
            PawnParameter->SetObjectPropertyValue_InContainer(Data, Pawn);
            TypeParameter->SetPropertyValue_InContainer(Data, Type);
            GizmoParameter->SetObjectPropertyValue_InContainer(Data, Gizmo);
            Controller.ProcessEvent(Callback, Data);
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
            CallLookAtCallback(**Zone, TEXT("Deselect Zone"));
        }
    }
}

ASampleLookAtZone::ASampleLookAtZone()
    : TargetGizmoClass(FSoftObjectPath(TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Props/BP_LookAtPOI_TargetGizmo.BP_LookAtPOI_TargetGizmo_C")))
{
}

void ASampleLookAtZone::SelectLookAtZone()
{
    UWorld* World = GetWorld();
    if (!World || IsValid(ReadLookAtActor(*this, TEXT("Gizmo Obj"))))
    {
        return;
    }

    if (const auto* Event = FindFProperty<FMulticastDelegateProperty>(GetClass(), TEXT("Dispatch On Mouse Clicked"));
        Event && Event->SignatureFunction && Event->SignatureFunction->NumParms == 0)
    {
        if (const auto* Delegate = Event->GetMulticastDelegate(Event->ContainerPtrToValuePtr<void>(this)))
        {
            Delegate->ProcessDelegate<UObject>(nullptr);
        }
    }
    if (!IsValid(this))
    {
        return;
    }
    WriteLookAtFlag(*this, TEXT("Is Selected"), true);
    CallLookAtCallback(*this, TEXT("Set Color"));

    auto* AssignedPawn = Cast<APawn>(ReadLookAtActor(*this, TEXT("Assigned Pawn")));
    auto* Controller = Cast<ASampleLookAtController>(World->GetFirstPlayerController());
    AActor* Gizmo = nullptr;
    if (Controller && !ReadLookAtFlag(*Controller, TEXT("Look At Player")))
    {
        if (UClass* GizmoClass = TargetGizmoClass.LoadSynchronous())
        {
            const auto* Offset = FindFProperty<FStructProperty>(GetClass(), TEXT("Gizmo Offset"));
            const FVector LocalOffset = Offset && Offset->Struct == TBaseStructure<FVector>::Get()
                ? *Offset->ContainerPtrToValuePtr<FVector>(this) : FVector::ZeroVector;
            const FVector Location = (IsValid(AssignedPawn) ? AssignedPawn->GetActorLocation() : FVector::ZeroVector) + LocalOffset;
            Gizmo = World->SpawnActor<AActor>(GizmoClass, FTransform(GetActorRotation(), Location));
            if (IsValid(Gizmo))
            {
                Gizmo->SetActorScale3D(FVector(4.0));
                WriteLookAtObject(*this, TEXT("Gizmo Obj"), Gizmo);
            }
        }
    }

    for (TActorIterator<ASampleLookAtZone> Zone(World); Zone; ++Zone)
    {
        if (*Zone != this)
        {
            Zone->DeselectLookAtZone();
        }
    }

    const auto* ZoneType = FindFProperty<FStrProperty>(GetClass(), TEXT("Zone Type"));
    if (Controller && ZoneType)
    {
        ReportLookAtZone(*Controller, AssignedPawn, ZoneType->GetPropertyValue_InContainer(this), Gizmo);
    }
}

void ASampleLookAtZone::DeselectLookAtZone()
{
    WriteLookAtFlag(*this, TEXT("Is Selected"), false);
    CallLookAtCallback(*this, TEXT("Set Color"));
    if (AActor* Gizmo = ReadLookAtActor(*this, TEXT("Gizmo Obj")); IsValid(Gizmo))
    {
        Gizmo->Destroy();
        WriteLookAtObject(*this, TEXT("Gizmo Obj"), nullptr);
    }
    if (AActor* Pawn = ReadLookAtActor(*this, TEXT("Assigned Pawn")); IsValid(Pawn))
    {
        WriteLookAtFlag(*Pawn, TEXT("LookAtPOI Enabled"), false);
    }
}
