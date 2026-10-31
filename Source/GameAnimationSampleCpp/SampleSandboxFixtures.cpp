#include "SampleSandboxFixtures.h"

#include "Components/MeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename TObject>
    TObject* ReadFixtureObject(const AActor& Owner, FName Name)
    {
        const auto* Property = FindFProperty<FObjectPropertyBase>(Owner.GetClass(), Name);
        return Property ? Cast<TObject>(Property->GetObjectPropertyValue_InContainer(&Owner)) : nullptr;
    }

    FText ReadFixtureText(const AActor& Owner, FName Name)
    {
        const auto* Property = FindFProperty<FTextProperty>(Owner.GetClass(), Name);
        return Property ? Property->GetPropertyValue_InContainer(&Owner) : FText::GetEmpty();
    }

    FVector ReadFixtureColor(const AActor& Owner)
    {
        const auto* Property = FindFProperty<FStructProperty>(Owner.GetClass(), TEXT("Color"));
        if (Property && Property->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const auto& Color = *Property->ContainerPtrToValuePtr<FLinearColor>(&Owner);
            return FVector(Color.R, Color.G, Color.B);
        }
        return FVector::ZeroVector;
    }

    FVector ReadFixturePlateScale(const AActor& Owner)
    {
        const auto* Property = FindFProperty<FNumericProperty>(Owner.GetClass(), TEXT("PlateScale"));
        const double Size = Property && Property->IsFloatingPoint()
            ? Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Owner)) : 1.0;
        return FVector(Size, Size, 0.02);
    }

    void UpdateFixtureColor(const AActor& Owner, FName Component, const FVector& Color)
    {
        if (auto* Mesh = ReadFixtureObject<UMeshComponent>(Owner, Component))
        {
            Mesh->SetVectorParameterValueOnMaterials(TEXT("Base Color"), Color);
        }
    }

    const FTransform* ReadBlockTransform(const AActor& Owner, FName Name)
    {
        const auto* Field = FindFProperty<FStructProperty>(Owner.GetClass(), Name);
        return Field && Field->Struct == TBaseStructure<FTransform>::Get()
            ? Field->ContainerPtrToValuePtr<FTransform>(&Owner) : nullptr;
    }

    FVector SampleBlockVector(const FVector& Min, const FVector& Max)
    {
        const double X = FMath::FRandRange(Min.X, Max.X);
        const double Y = FMath::FRandRange(Min.Y, Max.Y);
        const double Z = FMath::FRandRange(Min.Z, Max.Z);
        return FVector(X, Y, Z);
    }

    template <typename T>
    T ReadStyleValue(const UStruct& Type, const void* Data, FName Name, const T& Default)
    {
        const auto* Field = FindFProperty<FStructProperty>(&Type, Name);
        return Field && Field->Struct == TBaseStructure<T>::Get() ? *Field->ContainerPtrToValuePtr<T>(Data) : Default;
    }

    float ReadStyleNumber(const UStruct& Type, const void* Data, FName Name)
    {
        const auto* Field = FindFProperty<FNumericProperty>(&Type, Name);
        return Field && Field->IsFloatingPoint()
            ? Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(Data)) : 0.0f;
    }

    struct FSampleGridMaterialParameters
    {
        FLinearColor GridColor;
        FLinearColor SurfaceColor;
        FVector GridSizes;
        float Specularity;
    };

    FSampleGridMaterialParameters ReadGridMaterialParameters(const UScriptStruct& Type, const void* Values)
    {
        return {
            ReadStyleValue(Type, Values, TEXT("GridColor_2_93C9D1544CE9BC2796D83F9E713DAB23"), FLinearColor::Black),
            ReadStyleValue(Type, Values, TEXT("SurfaceColor_4_9B668B134E34B8A2B41BB98E51273395"), FLinearColor::Black),
            ReadStyleValue(Type, Values, TEXT("GridSizes_16_782DDA9B42440FAB466A829B7C2E5482"), FVector::ZeroVector),
            ReadStyleNumber(Type, Values, TEXT("Specularity_21_1B077FB34A0BA551F2D806BF119B8EA8"))
        };
    }

    FMulticastScriptDelegate* FindFixtureButtonDelegate(ASampleLevelButton& Button)
    {
        const auto* Field = FindFProperty<FMulticastInlineDelegateProperty>(Button.GetClass(), TEXT("ButtonPressed"));
        return Field && Field->SignatureFunction && Field->SignatureFunction->NumParms == 0
            ? Field->ContainerPtrToValuePtr<FMulticastScriptDelegate>(&Button) : nullptr;
    }
}

ASampleTeleporterDestination* ASampleTeleporterSender::GetDestination() const
{
    return ReadFixtureObject<ASampleTeleporterDestination>(*this, TEXT("Destination"));
}

void ASampleTeleporterSender::UpdateName()
{
    if (auto* Label = ReadFixtureObject<UTextRenderComponent>(*this, TEXT("DestinationName")))
    {
        const auto* Destination = GetDestination();
        Label->SetText(IsValid(Destination)
            ? FText::Format(NSLOCTEXT("SampleTeleporter", "DestinationLabel", "To\r\n{0}"),
                ReadFixtureText(*Destination, TEXT("DestinationName")))
            : NSLOCTEXT("SampleTeleporter", "MissingDestination", "No\r\nDestination"));
    }
}

void ASampleTeleporterSender::UpdateColor()
{
    if (const auto* Destination = GetDestination(); IsValid(Destination))
    {
        const FVector Color = ReadFixtureColor(*Destination);
        UpdateFixtureColor(*this, TEXT("Plate"), Color);
        UpdateFixtureColor(*this, TEXT("Pointer"), Color);
    }
}

void ASampleTeleporterSender::UpdateRotation()
{
    if (const auto* Destination = GetDestination(); IsValid(Destination))
    {
        const double Yaw = (Destination->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
        SetActorRotation(FRotator(0.0, Yaw, 0.0));
    }
}

void ASampleTeleporterSender::UpdateScale()
{
    if (const auto* Destination = GetDestination(); IsValid(Destination))
    {
        if (auto* Plate = ReadFixtureObject<USceneComponent>(*this, TEXT("Plate")))
        {
            Plate->SetWorldScale3D(ReadFixturePlateScale(*Destination));
        }
    }
}

void ASampleTeleporterSender::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateName();
    UpdateColor();
    UpdateRotation();
    UpdateScale();
}

void ASampleTeleporterSender::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (auto* Trigger = ReadFixtureObject<UPrimitiveComponent>(*this, TEXT("Trigger")))
    {
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASampleTeleporterSender::OnTriggerEntered);
    }
}

void ASampleTeleporterSender::OnTriggerEntered(UPrimitiveComponent* Trigger, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
    const auto* Destination = GetDestination();
    if (!IsValid(OtherActor) || !IsValid(Destination))
    {
        return;
    }
    if (const auto* Arrival = ReadFixtureObject<USceneComponent>(*Destination, TEXT("TeleportPoint")))
    {
        OtherActor->TeleportTo(Arrival->GetComponentLocation(), OtherActor->GetActorRotation());
    }
}

void ASampleTeleporterDestination::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (auto* Label = ReadFixtureObject<UTextRenderComponent>(*this, TEXT("Name")))
    {
        Label->SetText(ReadFixtureText(*this, TEXT("DestinationName")));
    }
    UpdateFixtureColor(*this, TEXT("Plate"), ReadFixtureColor(*this));
    if (auto* Plate = ReadFixtureObject<USceneComponent>(*this, TEXT("Plate")))
    {
        Plate->SetWorldScale3D(ReadFixturePlateScale(*this));
    }
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ASampleTeleporterSender> Sender(World); Sender; ++Sender)
        {
            if (Sender->GetDestination() == this)
            {
                Sender->UpdateName();
                Sender->UpdateColor();
                Sender->UpdateRotation();
                Sender->UpdateScale();
            }
        }
    }
}

void ASampleTargetDummy::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (auto* Trigger = ReadFixtureObject<UPrimitiveComponent>(*this, TEXT("Trigger")))
    {
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASampleTargetDummy::OnTriggerEntered);
        Trigger->OnComponentEndOverlap.AddUniqueDynamic(this, &ASampleTargetDummy::OnTriggerExited);
    }
}

void ASampleTargetDummy::OnTriggerEntered(UPrimitiveComponent* Trigger, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
    NotifyTargetReceiver(OtherActor, TEXT("AddTarget"));
}

void ASampleTargetDummy::OnTriggerExited(UPrimitiveComponent* Trigger, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
    int32 BodyIndex)
{
    NotifyTargetReceiver(OtherActor, TEXT("RemoveTarget"));
}

void ASampleTargetDummy::NotifyTargetReceiver(AActor* OtherActor, FName FunctionName)
{
    const UClass* Interface = FindObject<UClass>(nullptr,
        TEXT("/Game/Blueprints/BPI_SandboxCharacter_Pawn.BPI_SandboxCharacter_Pawn_C"));
    if (!IsValid(OtherActor) || !Interface || !OtherActor->GetClass()->ImplementsInterface(Interface))
    {
        return;
    }
    UFunction* Function = OtherActor->FindFunction(FunctionName);
    if (Function && Function->NumParms == 1)
    {
        for (TFieldIterator<FObjectPropertyBase> Field(Function); Field; ++Field)
        {
            if (Field->HasAnyPropertyFlags(CPF_Parm) && IsA(Field->PropertyClass))
            {
                FStructOnScope Parameters(Function);
                Field->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), this);
                OtherActor->ProcessEvent(Function, Parameters.GetStructMemory());
                return;
            }
        }
    }
}

void ASampleLevelButton::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (auto* Label = ReadFixtureObject<UTextRenderComponent>(*this, TEXT("Name")))
    {
        Label->SetText(ReadFixtureText(*this, TEXT("ButtonName")));
        const auto* TextColor = FindFProperty<FStructProperty>(GetClass(), TEXT("TextColor"));
        if (TextColor && TextColor->Struct == TBaseStructure<FLinearColor>::Get())
        {
            Label->SetTextRenderColor(TextColor->ContainerPtrToValuePtr<FLinearColor>(this)->ToFColor(false));
        }
    }
    UpdateFixtureColor(*this, TEXT("Plate"), ReadFixtureColor(*this));
    if (auto* Plate = ReadFixtureObject<USceneComponent>(*this, TEXT("Plate")))
    {
        Plate->SetWorldScale3D(ReadFixturePlateScale(*this));
    }
}

void ASampleLevelButton::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (auto* Trigger = ReadFixtureObject<UPrimitiveComponent>(*this, TEXT("Trigger")))
    {
        Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ASampleLevelButton::OnTriggerEntered);
    }
}

void ASampleLevelButton::OnTriggerEntered(UPrimitiveComponent* Trigger, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
    constexpr double Cooldown = 0.2;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextOverlapPressTime)
    {
        NextOverlapPressTime = Now + Cooldown;
        SimulatePress();
    }
}

void ASampleLevelButton::SimulatePress()
{
    const auto* Event = FindFProperty<FMulticastDelegateProperty>(GetClass(), TEXT("ButtonPressed"));
    if (Event && Event->SignatureFunction && Event->SignatureFunction->NumParms == 0)
    {
        if (const auto* Delegate = Event->GetMulticastDelegate(Event->ContainerPtrToValuePtr<void>(this)))
        {
            Delegate->ProcessDelegate<UObject>(nullptr);
        }
    }

    const auto* Enabled = FindFProperty<FBoolProperty>(GetClass(), TEXT("ExecuteConsoleCommand"));
    const auto* Command = FindFProperty<FStrProperty>(GetClass(), TEXT("ConsoleCommand"));
    if (IsValid(this) && Enabled && Enabled->GetPropertyValue_InContainer(this) && Command)
    {
        const FString Value = Command->GetPropertyValue_InContainer(this);
        if (!Value.IsEmpty())
        {
            UKismetSystemLibrary::ExecuteConsoleCommand(this, Value, nullptr);
        }
    }
}

void ASampleLevelBlock::RandomizeBlockOffset()
{
    const auto* Initial = ReadBlockTransform(*this, TEXT("InitialTransform"));
    const auto* Min = ReadBlockTransform(*this, TEXT("MinTransformOffset"));
    const auto* Max = ReadBlockTransform(*this, TEXT("MaxTransformOffset"));
    if (!Initial || !Min || !Max)
    {
        return;
    }

    const FVector Location = SampleBlockVector(Min->GetLocation(), Max->GetLocation());
    const FRotator MinRotation = Min->Rotator();
    const FRotator MaxRotation = Max->Rotator();
    const double Roll = FMath::FRandRange(MinRotation.Roll, MaxRotation.Roll);
    const double Pitch = FMath::FRandRange(MinRotation.Pitch, MaxRotation.Pitch);
    const double Yaw = FMath::FRandRange(MinRotation.Yaw, MaxRotation.Yaw);
    const FVector Scale = Initial->GetScale3D() + SampleBlockVector(Min->GetScale3D(), Max->GetScale3D());
    SetActorTransform(*Initial);
    AddActorLocalTransform(FTransform(FRotator(Pitch, Yaw, Roll), Location, Scale));
    RefreshBlockText();
}

void ASampleLevelBlock::ResetBlockOffset()
{
    if (const auto* Initial = ReadBlockTransform(*this, TEXT("InitialTransform")))
    {
        SetActorTransform(*Initial);
        RefreshBlockText();
    }
}

void ASampleLevelBlock::RefreshBlockText()
{
    auto* Label = ReadFixtureObject<UTextRenderComponent>(*this, TEXT("TextRender"));
    if (!Label)
    {
        return;
    }

    const auto* AutoName = FindFProperty<FBoolProperty>(GetClass(), TEXT("AutoNameFromHeight"));
    if (AutoName && AutoName->GetPropertyValue_InContainer(this))
    {
        const FNumberFormattingOptions Format = FNumberFormattingOptions().SetMaximumFractionalDigits(2);
        Label->SetText(FText::Format(NSLOCTEXT("SampleLevelBlock", "HeightLabel", "{0} M"),
            FText::AsNumber(GetActorScale3D().Z, &Format)));
    }
    else
    {
        Label->SetText(ReadFixtureText(*this, TEXT("Name")));
    }
}

void ASampleLevelBlock::RebuildTraversalLedges()
{
    const auto* Ledges = FindFProperty<FArrayProperty>(GetClass(), TEXT("Ledges"));
    const auto* Opposites = FindFProperty<FMapProperty>(GetClass(), TEXT("OppositeLedges"));
    const auto* Element = Ledges ? CastField<FObjectPropertyBase>(Ledges->Inner) : nullptr;
    const auto* Key = Opposites ? CastField<FObjectPropertyBase>(Opposites->KeyProp) : nullptr;
    const auto* Value = Opposites ? CastField<FObjectPropertyBase>(Opposites->ValueProp) : nullptr;
    if (!Element || !Key || !Value || Element->PropertyClass != USplineComponent::StaticClass() ||
        Key->PropertyClass != Element->PropertyClass || Value->PropertyClass != Element->PropertyClass)
    {
        return;
    }

    USplineComponent* Splines[] = {
        ReadFixtureObject<USplineComponent>(*this, TEXT("Ledge_1")),
        ReadFixtureObject<USplineComponent>(*this, TEXT("Ledge_2")),
        ReadFixtureObject<USplineComponent>(*this, TEXT("Ledge_3")),
        ReadFixtureObject<USplineComponent>(*this, TEXT("Ledge_4")),
    };
    FScriptArrayHelper Array(Ledges, Ledges->ContainerPtrToValuePtr<void>(this));
    Array.EmptyValues();
    for (auto* Spline : Splines)
    {
        Element->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Spline);
    }

    FScriptMapHelper Map(Opposites, Opposites->ContainerPtrToValuePtr<void>(this));
    Map.EmptyValues();
    TMap<USplineComponent*, int32> Entries;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Splines); ++Index)
    {
        USplineComponent* Spline = Splines[Index];
        const int32* Existing = Entries.Find(Spline);
        const int32 Entry = Existing ? *Existing : Map.AddDefaultValue_Invalid_NeedsRehash();
        if (!Existing)
        {
            Entries.Add(Spline, Entry);
            Key->SetObjectPropertyValue(Map.GetKeyPtr(Entry), Spline);
        }
        Value->SetObjectPropertyValue(Map.GetValuePtr(Entry), Splines[Index ^ 1]);
    }
    Map.Rehash();
}

void ASampleLevelBlock::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (auto* Mesh = ReadFixtureObject<UPrimitiveComponent>(*this, TEXT("StaticMesh")))
    {
        auto* Base = ReadFixtureObject<UMaterialInterface>(*this, TEXT("BaseMaterial"));
        if (const auto* Field = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("DynamicMaterial")))
        {
            Field->SetObjectPropertyValue_InContainer(this, Mesh->CreateDynamicMaterialInstance(0, Base));
        }
    }

    const auto* UseTheme = FindFProperty<FBoolProperty>(GetClass(), TEXT("UseLevelVisualsColor"));
    if (UseTheme && UseTheme->GetPropertyValue_InContainer(this))
    {
        if (UWorld* World = GetWorld())
        {
            TActorIterator<ASampleLevelVisuals> Visuals(World);
            if (Visuals)
            {
                ApplyLevelStyle(**Visuals);
            }
        }
    }
    else if (const auto* Parameters = FindFProperty<FStructProperty>(GetClass(), TEXT("MaterialParams")))
    {
        ApplyGridMaterial(*Parameters->Struct, Parameters->ContainerPtrToValuePtr<void>(this));
    }

    RefreshBlockText();
    const auto* Baseline = FindFProperty<FStructProperty>(GetClass(), TEXT("InitialTransform"));
    if (Baseline && Baseline->Struct == TBaseStructure<FTransform>::Get())
    {
        *Baseline->ContainerPtrToValuePtr<FTransform>(this) = GetActorTransform();
    }
    BindRandomizeButton();
}

void ASampleLevelBlock::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    BindRandomizeButton();
}

void ASampleLevelBlock::BindRandomizeButton()
{
    auto* Button = ReadFixtureObject<ASampleLevelButton>(*this, TEXT("Randomize Button"));
    FScriptDelegate Handler;
    Handler.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(ASampleLevelBlock, RandomizeBlockOffset));
    if (auto* Previous = BoundRandomizeButton.Get(); Previous && Previous != Button)
    {
        if (auto* Delegate = FindFixtureButtonDelegate(*Previous))
        {
            Delegate->Remove(Handler);
        }
    }
    BoundRandomizeButton = Button;
    if (Button)
    {
        if (auto* Delegate = FindFixtureButtonDelegate(*Button))
        {
            FScriptDelegate LegacyHandler;
            LegacyHandler.BindUFunction(this, TEXT("Randomize Offset"));
            Delegate->Remove(LegacyHandler);
            Delegate->AddUnique(Handler);
        }
    }
}

void ASampleLevelBlock::ApplyLevelStyle(const ASampleLevelVisuals& Visuals)
{
    const auto* UseTheme = FindFProperty<FBoolProperty>(GetClass(), TEXT("UseLevelVisualsColor"));
    const auto* Group = FindFProperty<FNameProperty>(GetClass(), TEXT("ColorGroup"));
    if (UseTheme && UseTheme->GetPropertyValue_InContainer(this) && Group)
    {
        const UScriptStruct* Type = nullptr;
        if (const void* Values = Visuals.FindGridSettings(Group->GetPropertyValue_InContainer(this), Type))
        {
            ApplyGridMaterial(*Type, Values);
        }
    }
}

void ASampleLevelBlock::ApplyGridMaterial(const UScriptStruct& Type, const void* Values)
{
    const auto Parameters = ReadGridMaterialParameters(Type, Values);
    if (auto* Material = ReadFixtureObject<UMaterialInstanceDynamic>(*this, TEXT("DynamicMaterial")))
    {
        Material->SetVectorParameterValue(TEXT("Grid Color"), Parameters.GridColor);
        Material->SetVectorParameterValue(TEXT("Surface Color"), Parameters.SurfaceColor);
        Material->SetScalarParameterValue(TEXT("Grid 1 Size"), Parameters.GridSizes.X);
        Material->SetScalarParameterValue(TEXT("Grid 2 Size"), Parameters.GridSizes.Y);
        Material->SetScalarParameterValue(TEXT("Grid 3 Size"), Parameters.GridSizes.Z);
        Material->SetScalarParameterValue(TEXT("Base Specular"), Parameters.Specularity);
    }
    if (auto* Label = ReadFixtureObject<UTextRenderComponent>(*this, TEXT("TextRender")))
    {
        Label->SetTextRenderColor(Parameters.GridColor.ToFColor(false));
    }
}

const void* ASampleLevelVisuals::FindStyle(const UScriptStruct*& OutType) const
{
    OutType = nullptr;
    const auto* Property = FindFProperty<FArrayProperty>(GetClass(), TEXT("LevelStyles"));
    const auto* Inner = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
    const auto* Index = FindFProperty<FIntProperty>(GetClass(), TEXT("StyleIndex"));
    if (!Inner || !Index)
    {
        return nullptr;
    }
    FScriptArrayHelper Styles(Property, Property->ContainerPtrToValuePtr<void>(this));
    if (Styles.Num() == 0)
    {
        return nullptr;
    }
    OutType = Inner->Struct;
    return Styles.GetRawPtr(FMath::Clamp(Index->GetPropertyValue_InContainer(this), 0, Styles.Num() - 1));
}

const void* ASampleLevelVisuals::FindGridSettings(FName Group, const UScriptStruct*& OutType) const
{
    const UScriptStruct* StyleType = nullptr;
    const void* Style = FindStyle(StyleType);
    const auto* Map = StyleType ? FindFProperty<FMapProperty>(StyleType, TEXT("BlockColors_19_BD7B5F9248A47F4BA4AEE2BCADEEA20F")) : nullptr;
    const auto* Value = Map ? CastField<FStructProperty>(Map->ValueProp) : nullptr;
    if (!Style || !Value || !CastField<FNameProperty>(Map->KeyProp))
    {
        OutType = nullptr;
        return nullptr;
    }
    OutType = Value->Struct;
    FScriptMapHelper Colors(Map, Map->ContainerPtrToValuePtr<void>(Style));
    return Colors.FindValueFromHash(&Group);
}

void ASampleLevelVisuals::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateVisuals();
}

void ASampleLevelVisuals::BeginPlay()
{
    Super::BeginPlay();
    UpdateVisuals();
}

void ASampleLevelVisuals::SetLevelStyle(int32 Index)
{
    if (const auto* Field = FindFProperty<FIntProperty>(GetClass(), TEXT("StyleIndex")))
    {
        Field->SetPropertyValue_InContainer(this, Index);
        UpdateVisuals();
    }
}

void ASampleLevelVisuals::UpdateVisuals()
{
    const UScriptStruct* Type = nullptr;
    const void* Style = FindStyle(Type);
    if (!Style)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ASampleLevelBlock> Block(World); Block; ++Block)
        {
            Block->ApplyLevelStyle(*this);
        }
    }
    if (auto* Fog = ReadFixtureObject<UExponentialHeightFogComponent>(*this, TEXT("ExponentialHeightFog")))
    {
        Fog->SetFogInscatteringColor(ReadStyleValue(*Type, Style,
            TEXT("FogColor_3_DA5FC03848B34AFC1BABBCB1640E14A6"), FLinearColor::Black));
        Fog->SetFogDensity(ReadStyleNumber(*Type, Style, TEXT("FogDensity_22_21D76EA447CB464F8ECFC6B3D18938E0")));
    }
    if (auto* Decal = ReadFixtureObject<UDecalComponent>(*this, TEXT("Decal")))
    {
        Decal->SetDecalColor(ReadStyleValue(*Type, Style,
            TEXT("DecalColor_12_1498C751435B031639098A8BF277CF4D"), FLinearColor::Black));
    }
    if (auto* Landscape = ReadFixtureObject<ALandscapeProxy>(*this, TEXT("Landscape")))
    {
        const UScriptStruct* SettingsType = nullptr;
        const void* Settings = FindGridSettings(TEXT("Landscape"), SettingsType);
        const FStructOnScope DefaultSettings(Settings ? nullptr : SettingsType);
        if (SettingsType)
        {
            const auto Parameters = ReadGridMaterialParameters(*SettingsType,
                Settings ? Settings : DefaultSettings.GetStructMemory());
            Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("Grid Color"), Parameters.GridColor);
            Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("Surface Color"), Parameters.SurfaceColor);
            Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("Grid 1 Size"), Parameters.GridSizes.X);
            Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("Grid 2 Size"), Parameters.GridSizes.Y);
            Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("Grid 3 Size"), Parameters.GridSizes.Z);
            Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("Base Specular"), Parameters.Specularity);
        }
    }
}
