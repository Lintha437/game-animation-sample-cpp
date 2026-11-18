#include "Characters/SampleVisualOverrideComponent.h"

#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataDrivenCVars/DataDrivenCVars.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Sandbox/SampleSandboxGameMode.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr TCHAR SampleVisualSelectionVariable[] = TEXT("DDCvar.VisualOverride");
}

USampleVisualOverrideComponent::USampleVisualOverrideComponent()
{
    bAutoActivate = false;
    SetIsReplicatedByDefault(false);
}

void USampleVisualOverrideComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(USampleVisualOverrideComponent, SelectedVisualOverride, Params);
}

void USampleVisualOverrideComponent::OnRep_SelectedVisualOverride()
{
    ApplyVisualOverride();
}

void USampleVisualOverrideComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetWorld() && GetWorld()->GetAuthGameMode<ASampleSandboxGameMode>())
    {
        if (auto* Variables = GEngine ? GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>() : nullptr)
        {
            Variables->OnDataDrivenCVarDelegate.AddUniqueDynamic(this, &USampleVisualOverrideComponent::HandleVisualSelectionChanged);
        }
        HandleVisualSelectionChanged(SampleVisualSelectionVariable);
    }
}

void USampleVisualOverrideComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* Variables = GEngine ? GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>() : nullptr)
    {
        Variables->OnDataDrivenCVarDelegate.RemoveDynamic(this, &USampleVisualOverrideComponent::HandleVisualSelectionChanged);
    }
    if (PendingVisualLoad)
    {
        PendingVisualLoad->CancelHandle();
        PendingVisualLoad.Reset();
    }
    PendingVisualClass.Reset();
    Super::EndPlay(EndPlayReason);
}

void USampleVisualOverrideComponent::HandleVisualSelectionChanged(FString VariableName)
{
    if (VariableName != SampleVisualSelectionVariable)
    {
        return;
    }
    if (PendingVisualLoad)
    {
        PendingVisualLoad->CancelHandle();
        PendingVisualLoad.Reset();
    }
    const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASampleSandboxGameMode>() : nullptr;
    const TSoftClassPtr<AActor> Choice = Mode ? Mode->GetSelectedVisualOverride() : TSoftClassPtr<AActor>();
    PendingVisualClass = Choice.ToSoftObjectPath();
    if (PendingVisualClass.IsNull() || Choice.IsValid())
    {
        SetVisualOverride(Choice.Get());
        return;
    }
    PendingVisualLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(PendingVisualClass,
        FStreamableDelegate::CreateUObject(this, &USampleVisualOverrideComponent::CompleteVisualLoad, PendingVisualClass));
}

void USampleVisualOverrideComponent::CompleteVisualLoad(FSoftObjectPath RequestedClass)
{
    if (PendingVisualClass == RequestedClass)
    {
        SetVisualOverride(Cast<UClass>(RequestedClass.ResolveObject()));
        PendingVisualLoad.Reset();
    }
}

void USampleVisualOverrideComponent::SetVisualOverride(UClass* Selected)
{
    if (Selected && (!Selected->IsChildOf(AActor::StaticClass()) || Selected->HasAnyClassFlags(CLASS_Abstract)))
    {
        Selected = nullptr;
    }

    if (const auto* Property = FindFProperty<FClassProperty>(GetClass(), TEXT("VisualOverride")))
    {
        Property->SetObjectPropertyValue_InContainer(this, Selected);
        if (Property->HasAnyPropertyFlags(CPF_Net))
        {
            MARK_PROPERTY_DIRTY_UNSAFE(this, Property->RepIndex);
        }
    }
    else
    {
        SelectedVisualOverride = Selected;
        MARK_PROPERTY_DIRTY_FROM_NAME(USampleVisualOverrideComponent, SelectedVisualOverride, this);
    }
    ApplyVisualOverride();
}

void USampleVisualOverrideComponent::ApplyVisualOverride()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    UClass* Selected = SelectedVisualOverride.Get();
    if (const auto* Property = FindFProperty<FClassProperty>(GetClass(), TEXT("VisualOverride")))
    {
        Selected = Cast<UClass>(Property->GetObjectPropertyValue_InContainer(this));
    }

    TInlineComponentArray<UChildActorComponent*> Children(Owner);
    for (UChildActorComponent* Child : Children)
    {
        if (Child->ComponentHasTag(TEXT("VisualOverride")))
        {
            Child->SetChildActorClass(Selected);
            if (auto* SourceMesh = Cast<USkeletalMeshComponent>(Child->GetAttachParent()))
            {
                SourceMesh->SetVisibility(Selected == nullptr);
            }
        }
    }
}
