#include "SampleSandboxFixtures.h"

#include "Components/MeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
