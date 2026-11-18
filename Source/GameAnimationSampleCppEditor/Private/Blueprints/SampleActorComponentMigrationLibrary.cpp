#include "Blueprints/SampleActorComponentMigrationLibrary.h"

#include "Animation/SampleAnimationTickComponent.h"
#include "Characters/SampleVisualOverrideComponent.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/Level.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
    UClass* ResolveNativeActorComponentClass(const UClass* Class)
    {
        if (!Class || Class->HasAnyClassFlags(CLASS_Native))
        {
            return nullptr;
        }
        const FString Path = Class->GetPathName();
        if (Path == TEXT("/Game/Blueprints/AC_PreCMCTick.AC_PreCMCTick_C") ||
            Path == TEXT("/Game/Blueprints/AC_PostABPTick.AC_PostABPTick_C"))
        {
            return USampleAnimationTickComponent::StaticClass();
        }
        if (Path == TEXT("/Game/Blueprints/AC_VisualOverrideManager.AC_VisualOverrideManager_C"))
        {
            return USampleVisualOverrideComponent::StaticClass();
        }
        return nullptr;
    }

    void CopyComponentTemplateSettings(UActorComponent& Source, UActorComponent& Destination)
    {
        UEngine::CopyPropertiesForUnrelatedObjects(&Source, &Destination);
        Destination.PrimaryComponentTick.bCanEverTick = Source.PrimaryComponentTick.bCanEverTick;
        Destination.PrimaryComponentTick.bStartWithTickEnabled = Source.PrimaryComponentTick.bStartWithTickEnabled;
        Destination.PrimaryComponentTick.bAllowTickOnDedicatedServer = Source.PrimaryComponentTick.bAllowTickOnDedicatedServer;
        Destination.PrimaryComponentTick.bTickEvenWhenPaused = Source.PrimaryComponentTick.bTickEvenWhenPaused;
        Destination.PrimaryComponentTick.TickGroup = Source.PrimaryComponentTick.TickGroup;
        Destination.PrimaryComponentTick.EndTickGroup = Source.PrimaryComponentTick.EndTickGroup;
        Destination.PrimaryComponentTick.TickInterval = Source.PrimaryComponentTick.TickInterval;
        Destination.bAutoActivate = Source.bAutoActivate;
        Destination.SetIsReplicated(Source.GetIsReplicated());
        Destination.ComponentTags = Source.ComponentTags;

        if (auto* VisualOverride = Cast<USampleVisualOverrideComponent>(&Destination))
        {
            if (const auto* Field = FindFProperty<FClassProperty>(Source.GetClass(), TEXT("VisualOverride")))
            {
                if (UClass* Selected = Cast<UClass>(Field->GetObjectPropertyValue_InContainer(&Source)))
                {
                    VisualOverride->SelectedVisualOverride = Selected;
                }
            }
        }
    }

    bool ActorHasLegacyComponent(const AActor& Actor)
    {
        for (const UActorComponent* Component : Actor.GetComponents())
        {
            if (Component && ResolveNativeActorComponentClass(Component->GetClass()) != nullptr)
            {
                return true;
            }
        }
        return false;
    }
}

int32 USampleActorComponentMigrationLibrary::ReplaceActorComponentClassesInBlueprint(
    UBlueprint* Blueprint, bool bValidateOnly)
{
    if (!Blueprint)
    {
        return INDEX_NONE;
    }

    if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
    {
        TArray<UActorComponent*> Overrides;
        Handler->GetAllTemplates(Overrides, false);
        for (UActorComponent* Component : Overrides)
        {
            if (Component && ResolveNativeActorComponentClass(Component->GetClass()) != nullptr)
            {
                return INDEX_NONE;
            }
        }
    }

    int32 Replacements = 0;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node)
            {
                continue;
            }
            const UClass* ExistingClass = Node->ComponentTemplate
                ? Node->ComponentTemplate->GetClass()
                : Node->ComponentClass.Get();
            UClass* NativeClass = ResolveNativeActorComponentClass(ExistingClass);
            if (!NativeClass)
            {
                continue;
            }
            ++Replacements;
            if (bValidateOnly)
            {
                continue;
            }

            Node->Modify();
            UActorComponent* OldTemplate = Node->ComponentTemplate;
            if (OldTemplate)
            {
                UObject* Outer = OldTemplate->GetOuter()
                    ? OldTemplate->GetOuter()
                    : (Blueprint->GeneratedClass ? static_cast<UObject*>(Blueprint->GeneratedClass)
                        : static_cast<UObject*>(Blueprint));
                const FName TemplateName = OldTemplate->GetFName();
                OldTemplate->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
                auto* Replacement = NewObject<UActorComponent>(Outer, NativeClass, TemplateName,
                    RF_Public | RF_ArchetypeObject | RF_Transactional);
                if (!Replacement)
                {
                    return INDEX_NONE;
                }
                CopyComponentTemplateSettings(*OldTemplate, *Replacement);
                Node->ComponentTemplate = Replacement;
            }
            Node->ComponentClass = NativeClass;
        }
    }

    if (!bValidateOnly && Replacements > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    return Replacements;
}

int32 USampleActorComponentMigrationLibrary::RefreshLegacyComponentActorsInLevel(UWorld* World)
{
    if (!World || !World->PersistentLevel)
    {
        return INDEX_NONE;
    }

    int32 Refreshed = 0;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor) || !ActorHasLegacyComponent(*Actor))
        {
            continue;
        }
        Actor->Modify();
        Actor->RerunConstructionScripts();
        if (ActorHasLegacyComponent(*Actor))
        {
            return INDEX_NONE;
        }
        Actor->MarkPackageDirty();
        ++Refreshed;
    }
    return Refreshed;
}
