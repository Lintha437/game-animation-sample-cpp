#include "Characters/SampleMoverModeAssetMigrationLibrary.h"

#include "Characters/SampleMoverModes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MovementMode.h"
#include "MovementModeTransition.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
    UClass* ResolveNativeModeClass(const UClass* Class)
    {
        if (!Class || Class->HasAnyClassFlags(CLASS_Native))
        {
            return nullptr;
        }
        const FString Path = Class->GetPathName();
        if (Path == TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Walking.BP_MovementMode_Walking_C"))
        {
            return USampleMoverWalkingMode::StaticClass();
        }
        if (Path == TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Falling.BP_MovementMode_Falling_C"))
        {
            return USampleMoverFallingMode::StaticClass();
        }
        if (Path == TEXT("/Game/Blueprints/MovementModes/B_MovementMode_Flying.B_MovementMode_Flying_C"))
        {
            return USampleMoverFlyingMode::StaticClass();
        }
        if (Path == TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Slide.BP_MovementMode_Slide_C"))
        {
            return USampleMoverSlidingMode::StaticClass();
        }
        if (Path == TEXT("/Game/Blueprints/MovementModes/BP_MovementMode_Ragdoll.BP_MovementMode_Ragdoll_C"))
        {
            return USampleMoverRagdollMode::StaticClass();
        }
        return nullptr;
    }

    UClass* ResolveNativeTransitionClass(const UClass* Class)
    {
        if (!Class || Class->HasAnyClassFlags(CLASS_Native))
        {
            return nullptr;
        }
        const FString Path = Class->GetPathName();
        if (Path == TEXT("/Game/Blueprints/MovementModes/BP_MovementTransition_ToSlide.BP_MovementTransition_ToSlide_C"))
        {
            return USampleMoverEnterSlideTransition::StaticClass();
        }
        if (Path == TEXT("/Game/Blueprints/MovementModes/BP_MovementTransition_FromSlide.BP_MovementTransition_FromSlide_C"))
        {
            return USampleMoverExitSlideTransition::StaticClass();
        }
        return nullptr;
    }

    void CopySourceNumber(const UObject& Source, FName Name, double& Target)
    {
        if (const auto* Field = FindFProperty<FNumericProperty>(Source.GetClass(), Name);
            Field && Field->IsFloatingPoint())
        {
            Target = Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(&Source));
        }
    }

    void CopySourceBool(const UObject& Source, FName Name, bool& Target)
    {
        if (const auto* Field = FindFProperty<FBoolProperty>(Source.GetClass(), Name))
        {
            Target = Field->GetPropertyValue_InContainer(&Source);
        }
    }

    bool CopyMatchingModeProperties(const UBaseMovementMode& Source, UBaseMovementMode& Destination)
    {
        for (TFieldIterator<FProperty> Property(Destination.GetClass()); Property; ++Property)
        {
            if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient) ||
                Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBaseMovementMode, Transitions))
            {
                continue;
            }
            const FProperty* SourceProperty = FindFProperty<FProperty>(Source.GetClass(), Property->GetFName());
            if (!SourceProperty || !SourceProperty->SameType(*Property))
            {
                continue;
            }
            Property->CopyCompleteValue(
                Property->ContainerPtrToValuePtr<void>(&Destination),
                SourceProperty->ContainerPtrToValuePtr<const void>(&Source));
        }
        if (auto* Walk = Cast<USampleMoverWalkingMode>(&Destination))
        {
            CopySourceNumber(Source, TEXT("WalkSpeed"), Walk->WalkSettings.WalkSpeed);
            CopySourceNumber(Source, TEXT("RunSpeed"), Walk->WalkSettings.RunSpeed);
            CopySourceNumber(Source, TEXT("SprintSpeed"), Walk->WalkSettings.SprintSpeed);
            CopySourceNumber(Source, TEXT("CrouchSpeed"), Walk->WalkSettings.CrouchSpeed);
            CopySourceNumber(Source, TEXT("WalkAcceleration"), Walk->WalkSettings.WalkAcceleration);
            CopySourceNumber(Source, TEXT("RunAcceleration"), Walk->WalkSettings.RunAcceleration);
            CopySourceNumber(Source, TEXT("SprintAcceleration"), Walk->WalkSettings.SprintAcceleration);
            CopySourceNumber(Source, TEXT("StoppingDeceleration"), Walk->WalkSettings.StoppingDeceleration);
            CopySourceNumber(Source, TEXT("GaitChangeDeceleration"), Walk->WalkSettings.GaitChangeDeceleration);
            CopySourceNumber(Source, TEXT("Walk/RunTurnStrength"), Walk->WalkSettings.WalkRunTurnStrength);
            CopySourceNumber(Source, TEXT("WalkRunTurnStrength"), Walk->WalkSettings.WalkRunTurnStrength);
            CopySourceNumber(Source, TEXT("SprintTurnStrength"), Walk->WalkSettings.SprintTurnStrength);
            CopySourceNumber(Source, TEXT("Walk/RunFacingTime"), Walk->WalkSettings.WalkRunFacingTime);
            CopySourceNumber(Source, TEXT("WalkRunFacingTime"), Walk->WalkSettings.WalkRunFacingTime);
            CopySourceNumber(Source, TEXT("SprintFacingTime"), Walk->WalkSettings.SprintFacingTime);
            CopySourceNumber(Source, TEXT("IdleFacingTime"), Walk->WalkSettings.IdleFacingTime);
            CopySourceBool(Source, TEXT("SlopeRelativeVelocity"), Walk->WalkSettings.bSlopeRelativeVelocity);
            CopySourceBool(Source, TEXT("JustLanded"), Walk->bJustLanded);
        }
        if (auto* Slide = Cast<USampleMoverSlidingMode>(&Destination))
        {
            CopySourceNumber(Source, TEXT("ShallowSlopeAngle"), Slide->SlideSettings.ShallowSlopeAngle);
            CopySourceNumber(Source, TEXT("SteepSlopeAngle"), Slide->SlideSettings.SteepSlopeAngle);
            CopySourceNumber(Source, TEXT("InitialBoostTime"), Slide->SlideSettings.InitialBoostTime);
            CopySourceNumber(Source, TEXT("FlatGroundSpeed"), Slide->SlideSettings.FlatGroundSpeed);
            CopySourceNumber(Source, TEXT("ShallowSlopeSpeed"), Slide->SlideSettings.ShallowSlopeSpeed);
            CopySourceNumber(Source, TEXT("SteepSlopeSpeed"), Slide->SlideSettings.SteepSlopeSpeed);
            CopySourceNumber(Source, TEXT("InitialBoostSpeed"), Slide->SlideSettings.InitialBoostSpeed);
            CopySourceNumber(Source, TEXT("InitialBoostAcceleration"), Slide->SlideSettings.InitialBoostAcceleration);
            CopySourceNumber(Source, TEXT("AfterBoostAcceleration"), Slide->SlideSettings.AfterBoostAcceleration);
            CopySourceNumber(Source, TEXT("FlatGroundDeceleration"), Slide->SlideSettings.FlatGroundDeceleration);
            CopySourceNumber(Source, TEXT("SteepSlopeDeceleration"), Slide->SlideSettings.SteepSlopeDeceleration);
            CopySourceBool(Source, TEXT("InitialBoost"), Slide->bInitialBoost);
        }
        return true;
    }

    bool ConvertTransitionsArray(TArray<TObjectPtr<UBaseMovementModeTransition>>& Transitions, UObject* Outer,
        TArrayView<const TObjectPtr<UBaseMovementModeTransition>> NativeDefaults, EObjectFlags Flags,
        bool bValidateOnly, int32& InOutCount)
    {
        bool bModified = false;
        TArray<TObjectPtr<UBaseMovementModeTransition>> Updated;
        Updated.Reserve(Transitions.Num());
        TArray<UBaseMovementModeTransition*> AvailableDefaults;
        for (UBaseMovementModeTransition* DefaultTransition : NativeDefaults)
        {
            if (DefaultTransition)
            {
                AvailableDefaults.Add(DefaultTransition);
            }
        }
        for (UBaseMovementModeTransition* Transition : Transitions)
        {
            if (!Transition)
            {
                Updated.Add(nullptr);
                continue;
            }
            if (UClass* NativeTransition = ResolveNativeTransitionClass(Transition->GetClass()))
            {
                ++InOutCount;
                bModified = true;
                UBaseMovementModeTransition* Replacement = nullptr;
                if (!bValidateOnly)
                {
                    if (UBaseMovementModeTransition** Match = AvailableDefaults.FindByPredicate(
                        [NativeTransition](const UBaseMovementModeTransition* Candidate)
                        {
                            return Candidate && Candidate->GetClass() == NativeTransition;
                        }))
                    {
                        Replacement = *Match;
                        AvailableDefaults.RemoveSingleSwap(Replacement);
                        Replacement->SetFlags(Flags & (RF_Public | RF_ArchetypeObject));
                    }
                }
                if (!Replacement)
                {
                    UObject* TargetOuter = bValidateOnly ? static_cast<UObject*>(GetTransientPackage()) : Outer;
                    Replacement = NewObject<UBaseMovementModeTransition>(TargetOuter, NativeTransition, NAME_None,
                        bValidateOnly ? RF_Transient : Flags);
                }
                if (!Replacement)
                {
                    return false;
                }
                Replacement->bSupportsAsync = Transition->bSupportsAsync;
                Updated.Add(Replacement);
            }
            else
            {
                Updated.Add(Transition);
            }
        }
        if (!bValidateOnly && bModified)
        {
            Transitions = MoveTemp(Updated);
        }
        return true;
    }
}

int32 USampleMoverModeAssetMigrationLibrary::ReplaceMoverModeInstancesOnComponent(
    UCharacterMoverComponent* Mover, bool bValidateOnly)
{
    if (!Mover)
    {
        return INDEX_NONE;
    }

    int32 Replacements = 0;
    const bool bIsArchetype = Mover->HasAnyFlags(RF_ArchetypeObject);
    const EObjectFlags BaseFlags = RF_Transactional
        | (bIsArchetype ? (RF_Public | RF_ArchetypeObject) : (Mover->GetFlags() & RF_Public));
    TMap<FName, TObjectPtr<UBaseMovementMode>> UpdatedModes;

    for (auto& [ModeName, ModeInstance] : Mover->MovementModes)
    {
        if (!ModeInstance)
        {
            UpdatedModes.Add(ModeName, nullptr);
            continue;
        }
        if (UClass* NativeModeClass = ResolveNativeModeClass(ModeInstance->GetClass()))
        {
            ++Replacements;
            UObject* TargetOuter = bValidateOnly ? static_cast<UObject*>(GetTransientPackage()) : static_cast<UObject*>(Mover);
            auto* Replacement = NewObject<UBaseMovementMode>(TargetOuter, NativeModeClass, NAME_None,
                bValidateOnly ? RF_Transient : BaseFlags);
            if (!Replacement || !CopyMatchingModeProperties(*ModeInstance, *Replacement))
            {
                return INDEX_NONE;
            }
            if (!bValidateOnly)
            {
                for (UBaseMovementModeTransition* DefaultTransition : Replacement->Transitions)
                {
                    if (DefaultTransition)
                    {
                        DefaultTransition->SetFlags(BaseFlags & (RF_Public | RF_ArchetypeObject));
                    }
                }
            }
            TArray<TObjectPtr<UBaseMovementModeTransition>> CopiedTransitions = ModeInstance->Transitions;
            if (!ConvertTransitionsArray(CopiedTransitions, Replacement, Replacement->Transitions,
                BaseFlags, bValidateOnly, Replacements))
            {
                return INDEX_NONE;
            }
            if (!bValidateOnly)
            {
                Replacement->Transitions = MoveTemp(CopiedTransitions);
            }
            UpdatedModes.Add(ModeName, Replacement);
        }
        else
        {
            int32 TransitionCount = 0;
            TArray<TObjectPtr<UBaseMovementModeTransition>> CopiedTransitions = ModeInstance->Transitions;
            if (!ConvertTransitionsArray(CopiedTransitions, ModeInstance, {}, BaseFlags, bValidateOnly, TransitionCount))
            {
                return INDEX_NONE;
            }
            if (!bValidateOnly && TransitionCount > 0)
            {
                ModeInstance->Modify();
                ModeInstance->Transitions = MoveTemp(CopiedTransitions);
            }
            Replacements += TransitionCount;
            UpdatedModes.Add(ModeName, ModeInstance);
        }
    }

    int32 GlobalTransitionCount = 0;
    TArray<TObjectPtr<UBaseMovementModeTransition>> GlobalTransitions = Mover->Transitions;
    if (!ConvertTransitionsArray(GlobalTransitions, Mover, {}, BaseFlags, bValidateOnly, GlobalTransitionCount))
    {
        return INDEX_NONE;
    }
    Replacements += GlobalTransitionCount;

    if (!bValidateOnly && Replacements > 0)
    {
        Mover->Modify();
        Mover->MovementModes = MoveTemp(UpdatedModes);
        if (GlobalTransitionCount > 0)
        {
            Mover->Transitions = MoveTemp(GlobalTransitions);
        }
        Mover->MarkPackageDirty();
    }
    return Replacements;
}

int32 USampleMoverModeAssetMigrationLibrary::ReplaceMoverModeInstancesInBlueprint(
    UBlueprint* Blueprint, bool bValidateOnly)
{
    if (!Blueprint)
    {
        return INDEX_NONE;
    }

    TArray<UCharacterMoverComponent*> Components;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (auto* Mover = Node ? Cast<UCharacterMoverComponent>(Node->ComponentTemplate) : nullptr)
            {
                Components.AddUnique(Mover);
            }
        }
    }
    if (UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false))
    {
        TArray<UActorComponent*> Overrides;
        Handler->GetAllTemplates(Overrides, false);
        for (UActorComponent* Component : Overrides)
        {
            if (auto* Mover = Cast<UCharacterMoverComponent>(Component))
            {
                Components.AddUnique(Mover);
            }
        }
    }

    int32 Total = 0;
    for (UCharacterMoverComponent* Mover : Components)
    {
        const int32 Replaced = ReplaceMoverModeInstancesOnComponent(Mover, bValidateOnly);
        if (Replaced < 0)
        {
            return INDEX_NONE;
        }
        Total += Replaced;
    }
    if (!bValidateOnly && Total > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    return Total;
}
