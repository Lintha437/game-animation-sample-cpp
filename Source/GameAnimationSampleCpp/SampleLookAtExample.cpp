#include "SampleLookAtExample.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpectatorPawn.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
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
    PrimaryActorTick.bCanEverTick = true;
}

void ASampleLookAtZone::BeginPlay()
{
    Super::BeginPlay();
    const auto* Meshes = FindFProperty<FArrayProperty>(GetClass(), TEXT("Meshes For Mouse Over"));
    const auto* Mesh = Meshes ? CastField<FObjectPropertyBase>(Meshes->Inner) : nullptr;
    if (!Mesh)
    {
        return;
    }
    FScriptArrayHelper Components(Meshes, Meshes->ContainerPtrToValuePtr<void>(this));
    for (int32 Index = 0; Index < Components.Num(); ++Index)
    {
        if (auto* Primitive = Cast<UPrimitiveComponent>(Mesh->GetObjectPropertyValue(Components.GetRawPtr(Index))))
        {
            Primitive->OnBeginCursorOver.AddUniqueDynamic(this, &ASampleLookAtZone::OnZoneHoverBegin);
            Primitive->OnEndCursorOver.AddUniqueDynamic(this, &ASampleLookAtZone::OnZoneHoverEnd);
            Primitive->OnClicked.AddUniqueDynamic(this, &ASampleLookAtZone::OnZoneClicked);
        }
    }
}

void ASampleLookAtZone::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CallLookAtCallback(*this, TEXT("Set Parallel To Camera & Show Tooltip"));
    if (AActor* Gizmo = ReadLookAtActor(*this, TEXT("Gizmo Obj")); IsValid(Gizmo))
    {
        Gizmo->SetActorLocation(GetTargetGizmoPosition());
    }
}

void ASampleLookAtZone::OnZoneHoverBegin(UPrimitiveComponent* Component)
{
    ApplyZoneColor(FLinearColor::White);
    WriteLookAtFlag(*this, TEXT("Is Mouse Over"), true);
}

void ASampleLookAtZone::OnZoneHoverEnd(UPrimitiveComponent* Component)
{
    WriteLookAtFlag(*this, TEXT("Is Mouse Over"), false);
    RefreshZoneColor();
}

void ASampleLookAtZone::OnZoneClicked(UPrimitiveComponent* Component, FKey Button)
{
    SelectLookAtZone();
}

void ASampleLookAtZone::RefreshZoneColor()
{
    const FName ColorName = ReadLookAtFlag(*this, TEXT("Is Selected")) ? TEXT("Color Green") : TEXT("Color Gray");
    const auto* Color = FindFProperty<FStructProperty>(GetClass(), ColorName);
    if (Color && Color->Struct == TBaseStructure<FLinearColor>::Get())
    {
        ApplyZoneColor(*Color->ContainerPtrToValuePtr<FLinearColor>(this));
    }
}

void ASampleLookAtZone::ApplyZoneColor(const FLinearColor& Color)
{
    for (const FName Name : {FName(TEXT("Border Dynamic Material")), FName(TEXT("Text Dynamic Material"))})
    {
        const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), Name);
        auto* Material = Field ? Cast<UMaterialInstanceDynamic>(Field->GetObjectPropertyValue_InContainer(this)) : nullptr;
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Diffuse"), Color);
        }
    }
}

FVector ASampleLookAtZone::GetTargetGizmoPosition() const
{
    const auto* Offset = FindFProperty<FStructProperty>(GetClass(), TEXT("Gizmo Offset"));
    const FVector PositionOffset = Offset && Offset->Struct == TBaseStructure<FVector>::Get()
        ? *Offset->ContainerPtrToValuePtr<FVector>(this) : FVector::ZeroVector;
    const AActor* Pawn = ReadLookAtActor(*this, TEXT("Assigned Pawn"));
    return (IsValid(Pawn) ? Pawn->GetActorLocation() : FVector::ZeroVector) + PositionOffset;
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
    RefreshZoneColor();

    auto* AssignedPawn = Cast<APawn>(ReadLookAtActor(*this, TEXT("Assigned Pawn")));
    auto* Controller = Cast<ASampleLookAtController>(World->GetFirstPlayerController());
    AActor* Gizmo = nullptr;
    if (Controller && !ReadLookAtFlag(*Controller, TEXT("Look At Player")))
    {
        if (UClass* GizmoClass = TargetGizmoClass.LoadSynchronous())
        {
            Gizmo = World->SpawnActor<AActor>(GizmoClass, FTransform(GetActorRotation(), GetTargetGizmoPosition()));
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
    RefreshZoneColor();
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

ASampleLookAtGizmo::ASampleLookAtGizmo()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.01f;
}

void ASampleLookAtGizmo::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("geo"));
    if (auto* Mesh = Field ? Cast<UPrimitiveComponent>(Field->GetObjectPropertyValue_InContainer(this)) : nullptr)
    {
        WriteLookAtObject(*this, TEXT("Border Dynamic Material"), Mesh->CreateDynamicMaterialInstance(0));
        UpdateGizmoColor(TEXT("Color Gray"));
    }
}

void ASampleLookAtGizmo::BeginPlay()
{
    Super::BeginPlay();
    const auto* Drawer = FindFProperty<FStructProperty>(GetClass(), TEXT("Drawer"));
    if (Drawer && Drawer->Struct == FDebugDrawer::StaticStruct())
    {
        *Drawer->ContainerPtrToValuePtr<FDebugDrawer>(this) = UDrawDebugLibrary::MakeDebugDrawer(this);
    }
    const auto* Hitbox = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("hitbox"));
    if (auto* Component = Hitbox ? Cast<UPrimitiveComponent>(Hitbox->GetObjectPropertyValue_InContainer(this)) : nullptr)
    {
        Component->OnBeginCursorOver.AddUniqueDynamic(this, &ASampleLookAtGizmo::OnGizmoHoverBegin);
        Component->OnEndCursorOver.AddUniqueDynamic(this, &ASampleLookAtGizmo::OnGizmoHoverEnd);
    }
}

void ASampleLookAtGizmo::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (const auto* Controller = GetWorld()->GetFirstPlayerController())
    {
        float X = 0.0f;
        float Y = 0.0f;
        Controller->GetMousePosition(X, Y);
        UpdateGizmoControls(Controller->GetInputAnalogKeyState(EKeys::LeftShift) == 1.0f, FVector2D(X, Y),
            Controller->GetInputAnalogKeyState(EKeys::MouseWheelAxis));
    }
}

void ASampleLookAtGizmo::OnGizmoHoverBegin(UPrimitiveComponent* Component)
{
    WriteLookAtFlag(*this, TEXT("Is Mouse Over"), true);
}

void ASampleLookAtGizmo::OnGizmoHoverEnd(UPrimitiveComponent* Component)
{
    WriteLookAtFlag(*this, TEXT("Is Mouse Over"), false);
}

void ASampleLookAtGizmo::UpdateGizmoColor(FName ColorName)
{
    const auto* Material = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("Border Dynamic Material"));
    const auto* Color = FindFProperty<FStructProperty>(GetClass(), ColorName);
    auto* DynamicMaterial = Material ? Cast<UMaterialInstanceDynamic>(Material->GetObjectPropertyValue_InContainer(this)) : nullptr;
    if (DynamicMaterial && Color && Color->Struct == TBaseStructure<FLinearColor>::Get())
    {
        DynamicMaterial->SetVectorParameterValue(TEXT("Diffuse"), *Color->ContainerPtrToValuePtr<FLinearColor>(this));
    }
}

void ASampleLookAtGizmo::UpdateGizmoControls(bool bShiftDown, FVector2D Cursor, float Wheel)
{
    const bool bHovered = ReadLookAtFlag(*this, TEXT("Is Mouse Over"));
    const bool bControlled = bShiftDown && (bHovered || ReadLookAtFlag(*this, TEXT("Is Controlled")));
    WriteLookAtFlag(*this, TEXT("Is Controlled"), bControlled);
    CallLookAtCallback(*this, TEXT("Set Parallel To Camera & Show Tooltip"));
    UpdateGizmoColor(bControlled ? TEXT("Color Green") : (bHovered ? TEXT("Color White") : TEXT("Color Gray")));

    const auto* MousePosition = FindFProperty<FStructProperty>(GetClass(), TEXT("Previous Mouse Pos"));
    FVector2D* Previous = MousePosition && MousePosition->Struct == TBaseStructure<FVector2D>::Get()
        ? MousePosition->ContainerPtrToValuePtr<FVector2D>(this) : nullptr;
    if (bControlled)
    {
        const FVector2D Delta = Previous ? Cursor - *Previous : FVector2D::ZeroVector;
        const FRotator Rotation = GetActorRotation();
        SetActorRotation(FRotator(0.0, FRotator::ClampAxis(Rotation.Yaw - Delta.X),
            FRotator::ClampAxis(Rotation.Roll + Delta.Y)));
        if (!FMath::IsNearlyZero(Wheel))
        {
            const auto* Sensitivity = FindFProperty<FNumericProperty>(GetClass(), TEXT("Scale Sensetivity"));
            const double ScaleRange = Sensitivity && Sensitivity->IsFloatingPoint()
                ? Sensitivity->GetFloatingPointPropertyValue(Sensitivity->ContainerPtrToValuePtr<void>(this)) : 0.0;
            const double ScaleDelta = FMath::Clamp(Wheel * 0.1, -1.0, 1.0) * ScaleRange;
            SetActorScale3D((GetActorScale3D() + FVector(ScaleDelta)).GetClampedToSize(0.3, 20.0));
        }
    }
    if (Previous)
    {
        *Previous = Cursor;
    }
}
