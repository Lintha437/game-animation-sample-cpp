#include "Animation/SampleAnimationDrawContextMigration.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Self.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "ScopedTransaction.h"
#include "UObject/UObjectGlobals.h"

#include <algorithm>
#include <initializer_list>

namespace
{
    struct FDrawRotationGraphNodes
    {
        UK2Node_VariableSet* SetOrigin = nullptr;
        UK2Node_MacroInstance* ValidityMacro = nullptr;
        UK2Node_CallFunction* GetPlayerController = nullptr;
        UK2Node_CallFunction* GetControlRotation = nullptr;
        UK2Node_VariableSet* SetRotation = nullptr;
        UK2Node_VariableSet* InvalidRotation = nullptr;
        UK2Node_ExecutionSequence* Sequence = nullptr;
    };

    struct FPinLinkSnapshot
    {
        UEdGraphPin* Pin = nullptr;
        TArray<UEdGraphPin*> LinkedPins;
    };

    struct FGraphInstallState
    {
        UBlueprint* Blueprint = nullptr;
        UEdGraph* Graph = nullptr;
        FDrawRotationGraphNodes Nodes;
        bool bIsCmcGraph = false;
        bool bAlreadyInstalled = false;
        bool bPrepared = false;
        UK2Node_CallFunction* Resolver = nullptr;
        UK2Node_Self* SelfNode = nullptr;
        UEdGraphPin* SetOriginThen = nullptr;
        UEdGraphPin* SetRotationExec = nullptr;
        UEdGraphPin* SetRotationValue = nullptr;
        UEdGraphPin* InvalidRotationThen = nullptr;
        TArray<FPinLinkSnapshot> LinkSnapshots;
    };

    UEdGraphNode* FindNodeByName(const UEdGraph& Graph, const TCHAR* NodeName)
    {
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (Node && Node->GetName() == NodeName)
            {
                return Node;
            }
        }
        return nullptr;
    }

    UFunction* FindResolver()
    {
        UClass* Library = FindObject<UClass>(nullptr,
            TEXT("/Script/GameAnimationSampleCpp.SampleAnimationDrawContext"));
        return Library ? Library->FindFunctionByName(TEXT("ResolveDrawRotation")) : nullptr;
    }

    bool IsFunction(const UK2Node_CallFunction* Call, FName FunctionName)
    {
        return Call && Call->FunctionReference.GetMemberName() == FunctionName;
    }

    bool IsLinked(const UEdGraphPin* Output, const UEdGraphPin* Input)
    {
        return Output && Input &&
            !Output->bOrphanedPin && !Input->bOrphanedPin &&
            Output->Direction == EGPD_Output &&
            Input->Direction == EGPD_Input &&
            Output->LinkedTo.Contains(const_cast<UEdGraphPin*>(Input)) &&
            Input->LinkedTo.Contains(const_cast<UEdGraphPin*>(Output));
    }

    bool HasOnlyLink(const UEdGraphPin* Output, const UEdGraphPin* Input)
    {
        return IsLinked(Output, Input) && Output->LinkedTo.Num() == 1 && Input->LinkedTo.Num() == 1;
    }

    bool HasNoOtherLinks(const UEdGraphNode& Node, std::initializer_list<const UEdGraphPin*> Allowed)
    {
        for (const UEdGraphPin* Pin : Node.Pins)
        {
            if (Pin && !Pin->LinkedTo.IsEmpty() && std::find(Allowed.begin(), Allowed.end(), Pin) == Allowed.end())
            {
                return false;
            }
        }
        return true;
    }

    int32 CountVariableSets(const UEdGraph& Graph, FName VariableName)
    {
        int32 Count = 0;
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            const auto* VariableSet = Cast<UK2Node_VariableSet>(Node);
            if (VariableSet && VariableSet->GetVarName() == VariableName)
            {
                ++Count;
            }
        }
        return Count;
    }

    bool IsRotatorPin(const UEdGraphPin* Pin)
    {
        return Pin &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
            Pin->PinType.PinSubCategoryObject.Get() == TBaseStructure<FRotator>::Get();
    }

    bool IsZeroRotatorPin(const UEdGraphPin* Pin)
    {
        if (!IsRotatorPin(Pin) || !Pin->LinkedTo.IsEmpty())
        {
            return false;
        }

        FString Normalized = Pin->DefaultValue;
        Normalized.ReplaceInline(TEXT(" "), TEXT(""));
        TArray<FString> Components;
        Normalized.ParseIntoArray(Components, TEXT(","), false);
        if (Components.Num() != 3)
        {
            return false;
        }

        for (const FString& Component : Components)
        {
            if (!Component.IsNumeric() || FCString::Atod(*Component) != 0.0)
            {
                return false;
            }
        }
        return true;
    }

    bool HasRetainedLocalControlGate(const UEdGraph& Graph, UK2Node_VariableSet* SetOrigin, bool bIsCmcGraph)
    {
        auto* Branch = Cast<UK2Node_IfThenElse>(FindNodeByName(Graph, TEXT("K2Node_IfThenElse_2")));
        auto* LocalControl = Cast<UK2Node_CallFunction>(FindNodeByName(Graph, TEXT("K2Node_CallFunction_16")));
        auto* PawnOwner = Cast<UK2Node_CallFunction>(FindNodeByName(Graph, TEXT("K2Node_CallFunction_11")));
        const auto* Entry = Cast<UK2Node_FunctionEntry>(FindNodeByName(Graph, TEXT("K2Node_FunctionEntry_0")));
        if (!Branch || !Entry || !HasOnlyLink(
            Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output),
            Branch->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)))
        {
            return false;
        }

        const UEdGraphPin* OriginPredecessor = Branch->GetThenPin();
        if (bIsCmcGraph)
        {
            const auto* MeshLocation = Cast<UK2Node_VariableSet>(
                FindNodeByName(Graph, TEXT("K2Node_VariableSet_2")));
            if (!MeshLocation || MeshLocation->GetVarName() != TEXT("MeshLocation") ||
                !HasOnlyLink(OriginPredecessor, MeshLocation->GetExecPin()))
            {
                return false;
            }
            OriginPredecessor = MeshLocation->GetThenPin();
        }

        return Branch->GetElsePin() && Branch->GetElsePin()->LinkedTo.IsEmpty() &&
            SetOrigin->GetVarName() == TEXT("DrawOrigin") &&
            IsFunction(LocalControl, TEXT("IsLocallyControlled")) &&
            IsFunction(PawnOwner, TEXT("TryGetPawnOwner")) &&
            HasOnlyLink(PawnOwner->FindPin(TEXT("ReturnValue"), EGPD_Output),
                LocalControl->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)) &&
            HasOnlyLink(LocalControl->FindPin(TEXT("ReturnValue"), EGPD_Output), Branch->GetConditionPin()) &&
            HasOnlyLink(OriginPredecessor, SetOrigin->GetExecPin());
    }

    bool ValidateCVarSequence(const UK2Node_ExecutionSequence& Sequence, bool bIsCmcGraph)
    {
        const TCHAR* ExpectedNames[] = {
            TEXT("DDCvar.DrawCharacterDebugStates"),
            TEXT("DDCvar.DrawCharacterDebugGraphs"),
            TEXT("DDCvar.DrawCharacterDebugShapes")
        };
        const TCHAR* ExpectedBranches[] = {
            TEXT("K2Node_IfThenElse_1"),
            bIsCmcGraph ? TEXT("K2Node_IfThenElse_4") : TEXT("K2Node_IfThenElse_0"),
            TEXT("K2Node_IfThenElse_0")
        };
        const int32 ExpectedCount = bIsCmcGraph ? 3 : 2;
        if (Sequence.Pins.Num() != ExpectedCount + 1)
        {
            return false;
        }

        for (int32 Index = 0; Index < ExpectedCount; ++Index)
        {
            const UEdGraphPin* Then = Sequence.FindPin(
                FName(*FString::Printf(TEXT("then_%d"), Index)), EGPD_Output);
            if (!Then || Then->LinkedTo.Num() != 1)
            {
                return false;
            }
            const auto* Call = Cast<UK2Node_CallFunction>(Then->LinkedTo[0]->GetOwningNode());
            if (!IsFunction(Call, TEXT("GetConsoleVariableBoolValue")) ||
                !HasOnlyLink(Then, Call->GetExecPin()))
            {
                return false;
            }
            const UEdGraphPin* NamePin = Call->FindPin(TEXT("VariableName"), EGPD_Input);
            const UEdGraphPin* Result = Call->FindPin(TEXT("ReturnValue"), EGPD_Output);
            const UEdGraphPin* CallThen = Call->GetThenPin();
            if (!NamePin || !NamePin->LinkedTo.IsEmpty() || NamePin->DefaultValue != ExpectedNames[Index] ||
                !CallThen || CallThen->LinkedTo.Num() != 1)
            {
                return false;
            }
            const auto* Branch = Cast<UK2Node_IfThenElse>(CallThen->LinkedTo[0]->GetOwningNode());
            if (!Branch || Branch->GetFName() != FName(ExpectedBranches[Index]) ||
                !HasOnlyLink(CallThen, Branch->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)) ||
                !HasOnlyLink(Result, Branch->GetConditionPin()))
            {
                return false;
            }
        }
        return true;
    }

    bool ValidateOriginalPreamble(FGraphInstallState& State)
    {
        FDrawRotationGraphNodes& Nodes = State.Nodes;
        if (!Nodes.SetOrigin || !Nodes.ValidityMacro || !Nodes.GetPlayerController ||
            !Nodes.GetControlRotation || !Nodes.SetRotation || !Nodes.InvalidRotation ||
            !Nodes.Sequence ||
            Nodes.GetPlayerController->GetTargetFunction() !=
                UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController")) ||
            !Nodes.ValidityMacro->GetMacroGraph() ||
            Nodes.ValidityMacro->GetMacroGraph()->GetPathName() !=
                TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:IsValid") ||
            Nodes.GetControlRotation->GetTargetFunction() !=
                AController::StaticClass()->FindFunctionByName(TEXT("GetControlRotation")) ||
            Nodes.SetRotation->GetVarName() != TEXT("DrawRotation") ||
            Nodes.SetRotation->GetVarName() != Nodes.InvalidRotation->GetVarName() ||
            CountVariableSets(*State.Graph, Nodes.SetRotation->GetVarName()) != 2)
        {
            return false;
        }

        UEdGraphPin* MacroExec = Nodes.ValidityMacro->FindPin(
            FName(TEXT("exec")), EGPD_Input);
        const UEdGraphPin* WorldContext = Nodes.GetPlayerController->FindPin(
            TEXT("WorldContextObject"), EGPD_Input);
        UEdGraphPin* PlayerIndex = Nodes.GetPlayerController->FindPin(TEXT("PlayerIndex"), EGPD_Input);
        UEdGraphPin* PlayerController = Nodes.GetPlayerController->FindPin(TEXT("ReturnValue"), EGPD_Output);
        UEdGraphPin* ControlRotationTarget = Nodes.GetControlRotation->FindPin(TEXT("self"), EGPD_Input);
        UEdGraphPin* ControlRotation = Nodes.GetControlRotation->FindPin(TEXT("ReturnValue"), EGPD_Output);
        UEdGraphPin* ValidRotationValue =
            Nodes.SetRotation->FindPin(Nodes.SetRotation->GetVarName(), EGPD_Input);
        UEdGraphPin* InvalidRotationValue =
            Nodes.InvalidRotation->FindPin(Nodes.InvalidRotation->GetVarName(), EGPD_Input);
        UEdGraphPin* SequenceExec = Nodes.Sequence->FindPin(
            UEdGraphSchema_K2::PN_Execute, EGPD_Input);

        UEdGraphPin* MacroObject = Nodes.ValidityMacro->FindPin(TEXT("InputObject"), EGPD_Input);
        UEdGraphPin* MacroValid = Nodes.ValidityMacro->FindPin(TEXT("Is Valid"), EGPD_Output);
        UEdGraphPin* MacroInvalid = Nodes.ValidityMacro->FindPin(TEXT("Is Not Valid"), EGPD_Output);

        if (!MacroExec || !MacroObject || !MacroValid || !MacroInvalid || !PlayerIndex ||
            !WorldContext || !WorldContext->bHidden || !WorldContext->LinkedTo.IsEmpty() ||
            WorldContext->DefaultObject || !WorldContext->DefaultValue.IsEmpty() ||
            !Nodes.SetOrigin->GetThenPin() || !Nodes.SetRotation->GetExecPin() ||
            !Nodes.InvalidRotation->GetExecPin() || !Nodes.SetRotation->GetThenPin() ||
            !Nodes.InvalidRotation->GetThenPin() ||
            !PlayerController || !ControlRotationTarget || !ControlRotation ||
            !ValidRotationValue || !InvalidRotationValue || !SequenceExec ||
            !PlayerIndex->LinkedTo.IsEmpty() || PlayerIndex->DefaultValue != TEXT("0") ||
            !IsRotatorPin(ValidRotationValue) || !IsRotatorPin(ControlRotation) ||
            ValidRotationValue->PinType != ControlRotation->PinType ||
            !IsZeroRotatorPin(InvalidRotationValue) ||
            Nodes.SetOrigin->GetThenPin()->LinkedTo.Num() != 1 ||
            MacroExec->LinkedTo.Num() != 1 || MacroObject->LinkedTo.Num() != 1 ||
            PlayerController->LinkedTo.Num() != 2 || ControlRotationTarget->LinkedTo.Num() != 1 ||
            MacroValid->LinkedTo.Num() != 1 || MacroInvalid->LinkedTo.Num() != 1 ||
            Nodes.SetRotation->GetExecPin()->LinkedTo.Num() != 1 ||
            Nodes.InvalidRotation->GetExecPin()->LinkedTo.Num() != 1 ||
            ControlRotation->LinkedTo.Num() != 1 || ValidRotationValue->LinkedTo.Num() != 1 ||
            Nodes.SetRotation->GetThenPin()->LinkedTo.Num() != 1 ||
            Nodes.InvalidRotation->GetThenPin()->LinkedTo.Num() != 1 ||
            !HasNoOtherLinks(*Nodes.ValidityMacro, {MacroExec, MacroObject, MacroValid, MacroInvalid}) ||
            !HasNoOtherLinks(*Nodes.GetPlayerController, {PlayerController}) ||
            !HasNoOtherLinks(*Nodes.GetControlRotation, {ControlRotationTarget, ControlRotation}) ||
            !HasNoOtherLinks(*Nodes.InvalidRotation,
                {Nodes.InvalidRotation->GetExecPin(), Nodes.InvalidRotation->GetThenPin()}))
        {
            return false;
        }

        return IsLinked(Nodes.SetOrigin->GetThenPin(), MacroExec) &&
            IsLinked(PlayerController, MacroObject) &&
            IsLinked(PlayerController, ControlRotationTarget) &&
            IsLinked(MacroValid, Nodes.SetRotation->GetExecPin()) &&
            IsLinked(MacroInvalid, Nodes.InvalidRotation->GetExecPin()) &&
            IsLinked(ControlRotation, ValidRotationValue) &&
            SequenceExec->LinkedTo.Num() == 2 &&
            IsLinked(Nodes.SetRotation->GetThenPin(), SequenceExec) &&
            IsLinked(Nodes.InvalidRotation->GetThenPin(), SequenceExec);
    }

    bool ValidateInstalledPreamble(FGraphInstallState& State, UFunction* ResolverFunction)
    {
        UEdGraph* Graph = State.Graph;
        FDrawRotationGraphNodes& Nodes = State.Nodes;
        if (FindNodeByName(*Graph, TEXT("K2Node_MacroInstance_0")) ||
            FindNodeByName(*Graph, TEXT("K2Node_CallFunction_9")) ||
            FindNodeByName(*Graph, TEXT("K2Node_CallFunction_64")) ||
            FindNodeByName(*Graph, TEXT("K2Node_VariableSet_0")) ||
            Nodes.SetRotation->GetVarName() != TEXT("DrawRotation") ||
            CountVariableSets(*Graph, Nodes.SetRotation->GetVarName()) != 1)
        {
            return false;
        }

        const UEdGraphPin* ResolverValue = State.Resolver->FindPin(TEXT("ReturnValue"), EGPD_Output);
        const UEdGraphPin* ResolverInstance = State.Resolver->FindPin(TEXT("AnimInstance"), EGPD_Input);
        const UEdGraphPin* RotationValue =
            Nodes.SetRotation->FindPin(Nodes.SetRotation->GetVarName(), EGPD_Input);
        const UEdGraphPin* SequenceExec = Nodes.Sequence->FindPin(
            UEdGraphSchema_K2::PN_Execute, EGPD_Input);

        if (State.Resolver->GetTargetFunction() != ResolverFunction ||
            !ResolverValue || !ResolverInstance || !RotationValue || !SequenceExec ||
            !IsRotatorPin(ResolverValue) || !IsRotatorPin(RotationValue) ||
            ResolverValue->PinType != RotationValue->PinType ||
            ResolverInstance->LinkedTo.Num() != 1 ||
            ResolverInstance->LinkedTo[0]->PinName != UEdGraphSchema_K2::PN_Self ||
            !HasOnlyLink(ResolverInstance->LinkedTo[0], ResolverInstance) ||
            !Nodes.SetOrigin->GetThenPin() || !Nodes.SetRotation->GetExecPin() ||
            !Nodes.SetRotation->GetThenPin() ||
            !Cast<UK2Node_Self>(ResolverInstance->LinkedTo[0]->GetOwningNode()) ||
            ResolverValue->LinkedTo.Num() != 1 || RotationValue->LinkedTo.Num() != 1 ||
            !IsLinked(ResolverValue, RotationValue) ||
            !HasNoOtherLinks(*State.Resolver, {ResolverValue, ResolverInstance}) ||
            Nodes.SetOrigin->GetThenPin()->LinkedTo.Num() != 1 ||
            Nodes.SetRotation->GetExecPin()->LinkedTo.Num() != 1 ||
            Nodes.SetRotation->GetThenPin()->LinkedTo.Num() != 1 ||
            !IsLinked(Nodes.SetOrigin->GetThenPin(), Nodes.SetRotation->GetExecPin()) ||
            !IsLinked(Nodes.SetRotation->GetThenPin(), SequenceExec) ||
            SequenceExec->LinkedTo.Num() != 1)
        {
            return false;
        }

        return true;
    }

    bool InspectGraph(UBlueprint* Blueprint, UEdGraph* Graph, FGraphInstallState& State)
    {
        if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || !Blueprint->FunctionGraphs.Contains(Graph))
        {
            return false;
        }

        const bool bIsCmcGraph =
            Blueprint->GetPathName() == TEXT("/Game/Blueprints/SandboxCharacter_CMC_ABP.SandboxCharacter_CMC_ABP") &&
            Graph->GetFName() == TEXT("Debug_ExperimentalStateMachine");
        const bool bIsMoverGraph =
            Blueprint->GetPathName() == TEXT("/Game/Blueprints/SandboxCharacter_Mover_ABP.SandboxCharacter_Mover_ABP") &&
            Graph->GetFName() == TEXT("DebugDraws");
        if (!bIsCmcGraph && !bIsMoverGraph)
        {
            return false;
        }

        State.Blueprint = Blueprint;
        State.Graph = Graph;
        State.bIsCmcGraph = bIsCmcGraph;
        State.Nodes.SetOrigin =
            Cast<UK2Node_VariableSet>(FindNodeByName(*Graph, TEXT("K2Node_VariableSet_1")));
        State.Nodes.SetRotation =
            Cast<UK2Node_VariableSet>(FindNodeByName(*Graph, TEXT("K2Node_VariableSet_4")));
        State.Nodes.Sequence =
            Cast<UK2Node_ExecutionSequence>(FindNodeByName(*Graph, TEXT("K2Node_ExecutionSequence_0")));

        if (!State.Nodes.SetOrigin || !State.Nodes.SetRotation || !State.Nodes.Sequence ||
            !HasRetainedLocalControlGate(*Graph, State.Nodes.SetOrigin, bIsCmcGraph) ||
            !ValidateCVarSequence(*State.Nodes.Sequence, bIsCmcGraph))
        {
            return false;
        }

        UFunction* ResolverFunction = FindResolver();
        if (!ResolverFunction)
        {
            return false;
        }

        TArray<UK2Node_CallFunction*> ResolverCalls;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (Call && Call->GetTargetFunction() == ResolverFunction)
            {
                ResolverCalls.Add(Call);
            }
        }
        if (ResolverCalls.Num() > 1)
        {
            return false;
        }

        if (ResolverCalls.Num() == 1)
        {
            State.bAlreadyInstalled = true;
            State.Resolver = ResolverCalls[0];
            return ValidateInstalledPreamble(State, ResolverFunction);
        }

        State.Nodes.ValidityMacro =
            Cast<UK2Node_MacroInstance>(FindNodeByName(*Graph, TEXT("K2Node_MacroInstance_0")));
        State.Nodes.GetPlayerController =
            Cast<UK2Node_CallFunction>(FindNodeByName(*Graph, TEXT("K2Node_CallFunction_9")));
        State.Nodes.GetControlRotation =
            Cast<UK2Node_CallFunction>(FindNodeByName(*Graph, TEXT("K2Node_CallFunction_64")));
        State.Nodes.InvalidRotation =
            Cast<UK2Node_VariableSet>(FindNodeByName(*Graph, TEXT("K2Node_VariableSet_0")));

        return ValidateOriginalPreamble(State);
    }

    void CaptureLinks(UEdGraphPin* Pin, TArray<FPinLinkSnapshot>& OutSnapshots)
    {
        FPinLinkSnapshot Snapshot;
        Snapshot.Pin = Pin;
        if (Pin)
        {
            Snapshot.LinkedPins = Pin->LinkedTo;
        }
        OutSnapshots.Add(MoveTemp(Snapshot));
    }

    void RemoveNewNodes(FGraphInstallState& State)
    {
        if (State.Resolver)
        {
            FBlueprintEditorUtils::RemoveNode(State.Blueprint, State.Resolver, true);
            State.Resolver = nullptr;
        }
        if (State.SelfNode)
        {
            FBlueprintEditorUtils::RemoveNode(State.Blueprint, State.SelfNode, true);
            State.SelfNode = nullptr;
        }
    }

    void RollbackGraph(FGraphInstallState& State)
    {
        for (FPinLinkSnapshot& Snapshot : State.LinkSnapshots)
        {
            if (Snapshot.Pin)
            {
                Snapshot.Pin->BreakAllPinLinks();
            }
        }

        RemoveNewNodes(State);

        for (const FPinLinkSnapshot& Snapshot : State.LinkSnapshots)
        {
            if (!Snapshot.Pin)
            {
                continue;
            }
            for (UEdGraphPin* OriginalLink : Snapshot.LinkedPins)
            {
                if (OriginalLink && !Snapshot.Pin->LinkedTo.Contains(OriginalLink))
                {
                    Snapshot.Pin->MakeLinkTo(OriginalLink);
                }
            }
        }

        State.LinkSnapshots.Reset();
        State.bPrepared = false;
    }

    bool PrepareGraph(FGraphInstallState& State)
    {
        UFunction* ResolverFunction = FindResolver();
        const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(State.Graph->GetSchema());
        if (!ResolverFunction || !Schema)
        {
            return false;
        }

        State.Blueprint->Modify();
        State.Graph->Modify();

        FGraphNodeCreator<UK2Node_CallFunction> ResolverCreator(*State.Graph);
        State.Resolver = ResolverCreator.CreateNode();
        State.Resolver->SetFromFunction(ResolverFunction);
        State.Resolver->NodePosX = State.Nodes.SetRotation->NodePosX - 280;
        State.Resolver->NodePosY = State.Nodes.SetRotation->NodePosY + 140;
        ResolverCreator.Finalize();

        FGraphNodeCreator<UK2Node_Self> SelfCreator(*State.Graph);
        State.SelfNode = SelfCreator.CreateNode();
        State.SelfNode->NodePosX = State.Resolver->NodePosX - 220;
        State.SelfNode->NodePosY = State.Resolver->NodePosY + 40;
        SelfCreator.Finalize();

        State.SetOriginThen = State.Nodes.SetOrigin->GetThenPin();
        State.SetRotationExec = State.Nodes.SetRotation->GetExecPin();
        State.SetRotationValue =
            State.Nodes.SetRotation->FindPin(State.Nodes.SetRotation->GetVarName(), EGPD_Input);
        State.InvalidRotationThen = State.Nodes.InvalidRotation->GetThenPin();

        UEdGraphPin* SelfOutput =
            State.SelfNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Output);
        UEdGraphPin* ResolverInstance =
            State.Resolver->FindPin(TEXT("AnimInstance"), EGPD_Input);
        UEdGraphPin* ResolverValue =
            State.Resolver->FindPin(TEXT("ReturnValue"), EGPD_Output);

        if (!State.SetOriginThen || !State.SetRotationExec || !State.SetRotationValue ||
            !State.InvalidRotationThen || !SelfOutput || !ResolverInstance || !ResolverValue ||
            !IsRotatorPin(State.SetRotationValue) || !IsRotatorPin(ResolverValue) ||
            State.SetRotationValue->PinType != ResolverValue->PinType)
        {
            RemoveNewNodes(State);
            return false;
        }

        CaptureLinks(State.SetOriginThen, State.LinkSnapshots);
        CaptureLinks(State.SetRotationExec, State.LinkSnapshots);
        CaptureLinks(State.SetRotationValue, State.LinkSnapshots);
        CaptureLinks(State.InvalidRotationThen, State.LinkSnapshots);
        for (FPinLinkSnapshot& Snapshot : State.LinkSnapshots)
        {
            Snapshot.Pin->GetOwningNode()->Modify();
            for (UEdGraphPin* Peer : Snapshot.LinkedPins)
            {
                Peer->GetOwningNode()->Modify();
            }
            Snapshot.Pin->BreakAllPinLinks();
        }
        State.bPrepared = true;

        const bool bConnected =
            Schema->TryCreateConnection(SelfOutput, ResolverInstance) &&
            Schema->TryCreateConnection(ResolverValue, State.SetRotationValue) &&
            Schema->TryCreateConnection(State.SetOriginThen, State.SetRotationExec);

        if (!bConnected)
        {
            RollbackGraph(State);
            return false;
        }

        const UEdGraphPin* SequenceExec = State.Nodes.Sequence->FindPin(
            UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        if (!SequenceExec || SequenceExec->LinkedTo.Num() != 1 ||
            !SequenceExec->LinkedTo.Contains(State.Nodes.SetRotation->GetThenPin()))
        {
            RollbackGraph(State);
            return false;
        }

        return true;
    }

    void CommitGraph(FGraphInstallState& State)
    {
        if (!State.bPrepared)
        {
            return;
        }

        FBlueprintEditorUtils::RemoveNode(State.Blueprint, State.Nodes.ValidityMacro, true);
        FBlueprintEditorUtils::RemoveNode(State.Blueprint, State.Nodes.GetPlayerController, true);
        FBlueprintEditorUtils::RemoveNode(State.Blueprint, State.Nodes.GetControlRotation, true);
        FBlueprintEditorUtils::RemoveNode(State.Blueprint, State.Nodes.InvalidRotation, true);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(State.Blueprint);

        State.LinkSnapshots.Reset();
        State.bPrepared = false;
    }
}

ESampleDrawRotationGraphState USampleAnimationDrawContextMigration::ClassifySharedDrawRotation(
    UBlueprint* Blueprint, UEdGraph* Graph)
{
    FGraphInstallState State;
    if (!InspectGraph(Blueprint, Graph, State))
    {
        return ESampleDrawRotationGraphState::Rejected;
    }
    return State.bAlreadyInstalled ? ESampleDrawRotationGraphState::Installed : ESampleDrawRotationGraphState::Original;
}

bool USampleAnimationDrawContextMigration::InstallSharedDrawRotationPair(
    UBlueprint* CmcBlueprint,
    UEdGraph* CmcGraph,
    UBlueprint* MoverBlueprint,
    UEdGraph* MoverGraph)
{
    FGraphInstallState CmcState;
    FGraphInstallState MoverState;

    if (!InspectGraph(CmcBlueprint, CmcGraph, CmcState) ||
        !CmcState.bIsCmcGraph ||
        !InspectGraph(MoverBlueprint, MoverGraph, MoverState) ||
        MoverState.bIsCmcGraph)
    {
        return false;
    }

    if (CmcState.bAlreadyInstalled && MoverState.bAlreadyInstalled)
    {
        return true;
    }

    FScopedTransaction Transaction(
        NSLOCTEXT("GameAnimationSampleCpp", "InstallSharedDrawRotation", "Install shared animation draw rotation"));

    if (!CmcState.bAlreadyInstalled && !PrepareGraph(CmcState))
    {
        Transaction.Cancel();
        return false;
    }

    if (!MoverState.bAlreadyInstalled && !PrepareGraph(MoverState))
    {
        RollbackGraph(CmcState);
        Transaction.Cancel();
        return false;
    }

    CommitGraph(CmcState);
    CommitGraph(MoverState);
    return true;
}
