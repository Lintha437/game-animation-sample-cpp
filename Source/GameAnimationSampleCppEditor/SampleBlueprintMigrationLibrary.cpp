#include "SampleBlueprintMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
    bool CollectIsolatedChain(UEdGraphNode* Event, TSet<UEdGraphNode*>& Remove)
    {
        TArray<UEdGraphNode*> Pending{Event};
        TSet<UEdGraphNode*> Visited;
        while (!Pending.IsEmpty())
        {
            UEdGraphNode* Current = Pending.Pop(EAllowShrinking::No);
            if (Visited.Contains(Current))
            {
                continue;
            }
            Visited.Add(Current);
            bool bHasExecOutput = false;
            bool bHasExecInput = false;
            for (const UEdGraphPin* Pin : Current->Pins)
            {
                if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    bHasExecInput |= Pin->Direction == EGPD_Input;
                    bHasExecOutput |= Pin->Direction == EGPD_Output;
                }
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    Pending.Add(Peer->GetOwningNode());
                }
            }
            if (Current != Event && bHasExecOutput && !bHasExecInput)
            {
                return false;
            }
        }
        Remove.Append(Visited);
        return true;
    }

    void RemoveNodes(UBlueprint& Blueprint, UEdGraph& Graph, const TSet<UEdGraphNode*>& Nodes)
    {
        Blueprint.Modify();
        Graph.Modify();
        for (UEdGraphNode* Node : Nodes)
        {
            FBlueprintEditorUtils::RemoveNode(&Blueprint, Node, true);
        }
    }

    TSet<UEdGraphNode*> CollectExecutionNodes(const UEdGraphPin& Start)
    {
        TArray<UEdGraphPin*> Pending(Start.LinkedTo);
        TSet<UEdGraphNode*> Nodes;
        while (!Pending.IsEmpty())
        {
            UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No)->GetOwningNode();
            if (Nodes.Contains(Node))
            {
                continue;
            }
            Nodes.Add(Node);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    Pending.Append(Pin->LinkedTo);
                }
            }
        }
        return Nodes;
    }

    void CollectUnusedInputs(TSet<UEdGraphNode*>& Remove, const UEdGraphNode* Keep)
    {
        TSet<UEdGraphNode*> Candidates;
        TArray<UEdGraphNode*> Pending = Remove.Array();
        while (!Pending.IsEmpty())
        {
            for (const UEdGraphPin* Pin : Pending.Pop(EAllowShrinking::No)->Pins)
            {
                if (Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    continue;
                }
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    UEdGraphNode* Source = Peer->GetOwningNode();
                    if (Source != Keep && !Remove.Contains(Source) && !Candidates.Contains(Source) &&
                        !Source->Pins.ContainsByPredicate([](const UEdGraphPin* SourcePin)
                        {
                            return SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                        }))
                    {
                        Candidates.Add(Source);
                        Pending.Add(Source);
                    }
                }
            }
        }
        bool bChanged;
        do
        {
            bChanged = false;
            for (auto It = Candidates.CreateIterator(); It; ++It)
            {
                if ((*It)->Pins.ContainsByPredicate([&](const UEdGraphPin* Pin)
                {
                    return Pin->Direction == EGPD_Output && Pin->LinkedTo.ContainsByPredicate([&](const UEdGraphPin* Peer)
                    {
                        const UEdGraphNode* Consumer = Peer->GetOwningNode();
                        return !Remove.Contains(Consumer) && !Candidates.Contains(Consumer);
                    });
                }))
                {
                    It.RemoveCurrent();
                    bChanged = true;
                }
            }
        } while (bChanged);
        Remove.Append(Candidates);
    }

    struct FTraversalCallReplacement
    {
        UEdGraphNode* First;
        UEdGraphNode* Last;
        const UFunction* Function;
        TArray<TPair<FName, UEdGraphPin*>> Inputs;
    };

    bool InstallTraversalCall(UBlueprint& Blueprint, UEdGraph& Graph, const FTraversalCallReplacement& Replacement)
    {
        FGraphNodeCreator<UK2Node_CallFunction> CreateCall(Graph);
        UK2Node_CallFunction* Call = CreateCall.CreateNode();
        Call->SetFromFunction(Replacement.Function);
        Call->NodePosX = Replacement.First->NodePosX;
        Call->NodePosY = Replacement.First->NodePosY;
        CreateCall.Finalize();
        const UEdGraphSchema* Schema = Graph.GetSchema();
        for (const auto& [Name, Old] : Replacement.Inputs)
        {
            UEdGraphPin* New = Call->FindPin(Name, EGPD_Input);
            if (!New)
            {
                return false;
            }
            if (Old->LinkedTo.IsEmpty())
            {
                Schema->TrySetDefaultValue(*New, Old->DefaultValue);
            }
            else if (!Schema->TryCreateConnection(Old->LinkedTo[0], New))
            {
                return false;
            }
        }
        UEdGraphPin* Exec = Replacement.First->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        UEdGraphPin* Then = Replacement.Last->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
        if (!Exec || !Then || !Schema->MovePinLinks(*Exec, *Call->GetExecPin()).CanSafeConnect() ||
            !Schema->MovePinLinks(*Then, *Call->GetThenPin()).CanSafeConnect())
        {
            return false;
        }
        TSet<UEdGraphNode*> Remove{Replacement.First, Replacement.Last};
        CollectUnusedInputs(Remove, nullptr);
        RemoveNodes(Blueprint, Graph, Remove);
        return true;
    }
}

bool USampleBlueprintMigrationLibrary::RetargetSelfCall(UEdGraph* Graph, FName OldFunction, FName NewFunction)
{
    if (!Graph || OldFunction.IsNone() || NewFunction.IsNone())
    {
        return false;
    }

    UK2Node_CallFunction* Match = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        if (Call && Call->FunctionReference.IsSelfContext() && Call->FunctionReference.GetMemberName() == OldFunction)
        {
            if (Match)
            {
                return false;
            }
            Match = Call;
        }
    }
    if (!Match)
    {
        return false;
    }

    Match->Modify();
    Match->FunctionReference.SetSelfMember(NewFunction, FGuid());
    return true;
}

TArray<FString> USampleBlueprintMigrationLibrary::ListEnhancedInputActions(const UEdGraph* Graph)
{
    TArray<FString> Paths;
    if (Graph)
    {
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_EnhancedInputAction* Event = Cast<UK2Node_EnhancedInputAction>(Node);
            if (Event && Event->InputAction)
            {
                Paths.Add(Event->InputAction->GetPathName());
            }
        }
    }
    return Paths;
}

bool USampleBlueprintMigrationLibrary::RemoveEnhancedInputActions(
    UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FString>& ActionPaths)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || ActionPaths.IsEmpty())
    {
        return false;
    }

    TSet<FString> Matched;
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_EnhancedInputAction* Event = Cast<UK2Node_EnhancedInputAction>(Node);
        if (!Event || !Event->InputAction || !ActionPaths.Contains(Event->InputAction->GetPathName()))
        {
            continue;
        }
        const FString Path = Event->InputAction->GetPathName();
        if (Matched.Contains(Path) || !CollectIsolatedChain(Event, Remove))
        {
            return false;
        }
        Matched.Add(Path);
    }
    if (Matched.Num() != ActionPaths.Num())
    {
        return false;
    }

    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

int64 USampleBlueprintMigrationLibrary::GetCompiledFunctionFlags(const UBlueprint* Blueprint, FName FunctionName)
{
    const UFunction* Function = Blueprint && Blueprint->GeneratedClass
        ? Blueprint->GeneratedClass->FindFunctionByName(FunctionName)
        : nullptr;
    return Function ? Function->FunctionFlags : 0;
}

bool USampleBlueprintMigrationLibrary::RemoveCustomEvent(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || EventName.IsNone())
    {
        return false;
    }
    UK2Node_CustomEvent* Match = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
        if (Event && Event->CustomFunctionName == EventName)
        {
            if (Match)
            {
                return false;
            }
            Match = Event;
        }
    }

    TSet<UEdGraphNode*> Remove;
    if (!Match || !CollectIsolatedChain(Match, Remove))
    {
        return false;
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::RemoveDeclaredVariable(UBlueprint* Blueprint, FName VariableName)
{
    if (!Blueprint || FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE)
    {
        return false;
    }
    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableName);
    return true;
}

bool USampleBlueprintMigrationLibrary::ReplaceVoidFunctionBody(
    UBlueprint* Blueprint, UEdGraph* Graph, UClass* Library, FName FunctionName)
{
    const UFunction* Function = Library ? Library->FindFunctionByName(FunctionName) : nullptr;
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || !Function ||
        !Function->HasAllFunctionFlags(FUNC_Static | FUNC_Native) || Function->GetReturnProperty())
    {
        return false;
    }

    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1)
    {
        return false;
    }
    UK2Node_FunctionEntry* Entry = Entries[0];
    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (const auto* Result = Cast<UK2Node_FunctionResult>(Node))
        {
            for (const UEdGraphPin* Pin : Result->Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    return false;
                }
            }
        }
        if (Node != Entry)
        {
            OldBody.Add(Node);
        }
    }

    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Call = Creator.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = Entry->NodePosX + 400;
    Call->NodePosY = Entry->NodePosY;
    Creator.Finalize();

    RemoveNodes(*Blueprint, *Graph, OldBody);
    Entry->Modify();
    Entry->LocalVariables.Reset();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    for (UEdGraphPin* Output : Entry->Pins)
    {
        if (Output->Direction == EGPD_Output)
        {
            const FName InputName = Output->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
                ? UEdGraphSchema_K2::PN_Execute
                : Output->PinName;
            UEdGraphPin* Input = Call->FindPin(InputName, EGPD_Input);
            if (!Input || !Schema->TryCreateConnection(Output, Input))
            {
                return false;
            }
        }
    }
    return true;
}

bool USampleBlueprintMigrationLibrary::ReplaceTraversalGeometry(UBlueprint* Blueprint, UEdGraph* Graph)
{
    static const FName FunctionName(TEXT("EvaluateTraversalClearance"));
    static const FName ResultName(TEXT("TraversalCheckResult"));
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || Graph->GetFName() != TEXT("TryTraversalAction"))
    {
        return false;
    }
    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    if (Calls.ContainsByPredicate([](const UK2Node_CallFunction* Call)
    {
        return Call->FunctionReference.GetMemberName() == FunctionName;
    }))
    {
        return true;
    }
    const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (!Function || Entries.Num() != 1)
    {
        return false;
    }
    UK2Node_FunctionEntry* Entry = Entries[0];
    UEdGraphPin* Start = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UK2Node_ExecutionSequence* Sequence = Start && Start->LinkedTo.Num() == 1
        ? Cast<UK2Node_ExecutionSequence>(Start->LinkedTo[0]->GetOwningNode()) : nullptr;
    UEdGraphPin* Probe = Sequence ? Sequence->GetThenPinGivenIndex(1) : nullptr;
    UEdGraphPin* Clearance = Sequence ? Sequence->GetThenPinGivenIndex(2) : nullptr;
    UEdGraphPin* Inputs = Entry->FindPin(TEXT("Inputs"), EGPD_Output);
    UEdGraphPin* Debug = Entry->FindPin(TEXT("DebugType"), EGPD_Output);
    if (!Probe || !Clearance || !Inputs || !Debug || !Sequence->GetThenPinGivenIndex(4))
    {
        return false;
    }
    TSet<UEdGraphNode*> Remove = CollectExecutionNodes(*Probe);
    Remove.Append(CollectExecutionNodes(*Clearance));
    int32 ApproachTraces = 0;
    int32 ClearanceTraces = 0;
    UK2Node_FunctionResult* Rejection = nullptr;
    for (UEdGraphNode* Node : Remove)
    {
        if (const auto* Call = Cast<UK2Node_CallFunction>(Node);
            Call && Call->FunctionReference.GetMemberName() == TEXT("CapsuleTraceSingle"))
        {
            const UEdGraphPin* Channel = Call->FindPin(TEXT("TraceChannel"), EGPD_Input);
            ApproachTraces += Channel && Channel->DefaultValue == TEXT("TraceTypeQuery3");
            ClearanceTraces += Channel && Channel->DefaultValue == TEXT("TraceTypeQuery1");
        }
        if (auto* Result = Cast<UK2Node_FunctionResult>(Node))
        {
            const UEdGraphPin* TraversalFailed = Result->FindPin(TEXT("TraversalCheckFailed"), EGPD_Input);
            const UEdGraphPin* MontageFailed = Result->FindPin(TEXT("MontageSelectionFailed"), EGPD_Input);
            if (TraversalFailed && TraversalFailed->DefaultValue == TEXT("true") &&
                MontageFailed && MontageFailed->DefaultValue == TEXT("false"))
            {
                Rejection = Result;
            }
        }
    }
    TArray<UK2Node_VariableGet*> Gets;
    Graph->GetNodesOfClass(Gets);
    UEdGraphPin* LocalResult = nullptr;
    for (UK2Node_VariableGet* Get : Gets)
    {
        if (Get->GetVarName() == ResultName && Get->VariableReference.IsLocalScope())
        {
            UEdGraphPin* Candidate = Get->FindPin(ResultName, EGPD_Output);
            if (Candidate && Candidate->SubPins.IsEmpty())
            {
                LocalResult = Candidate;
                break;
            }
        }
    }
    if (ApproachTraces != 1 || ClearanceTraces != 3 || !LocalResult || !Rejection)
    {
        return false;
    }

    Remove.Remove(Rejection);
    CollectUnusedInputs(Remove, LocalResult->GetOwningNode());
    RemoveNodes(*Blueprint, *Graph, Remove);
    const UEdGraphSchema* Schema = Graph->GetSchema();
    if (!Inputs->SubPins.IsEmpty())
    {
        Schema->RecombinePin(Inputs->SubPins[0]);
    }
    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*Graph);
    UK2Node_CallFunction* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = Sequence->NodePosX + 350;
    Call->NodePosY = Sequence->NodePosY;
    CreateCall.Finalize();
    FGraphNodeCreator<UK2Node_IfThenElse> CreateBranch(*Graph);
    UK2Node_IfThenElse* Branch = CreateBranch.CreateNode();
    Branch->NodePosX = Call->NodePosX + 420;
    Branch->NodePosY = Call->NodePosY;
    CreateBranch.Finalize();
    return Schema->TryCreateConnection(Probe, Call->GetExecPin()) &&
        Schema->TryCreateConnection(Inputs, Call->FindPin(TEXT("Inputs"), EGPD_Input)) &&
        Schema->TryCreateConnection(Debug, Call->FindPin(TEXT("DebugType"), EGPD_Input)) &&
        Schema->TryCreateConnection(LocalResult, Call->FindPin(ResultName, EGPD_Input)) &&
        Schema->TryCreateConnection(Call->GetThenPin(), Branch->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Branch->GetConditionPin()) &&
        Schema->TryCreateConnection(Branch->GetElsePin(), Rejection->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input));
}

bool USampleBlueprintMigrationLibrary::ReplaceTraversalCollision(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const UFunction* Begin = Blueprint->ParentClass->FindFunctionByName(TEXT("BeginTraversalCollision"));
    const UFunction* End = Blueprint->ParentClass->FindFunctionByName(TEXT("EndTraversalCollision"));
    const UFunction* Activity = Blueprint->ParentClass->FindFunctionByName(TEXT("SetTraversalActionActive"));
    if (!Begin || !End || !Activity)
    {
        return false;
    }

    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    TArray<FTraversalCallReplacement> Replacements;
    int32 ExistingBegin = 0;
    int32 ExistingEnd = 0;
    int32 ExistingActivity = 0;
    int32 MeshChanges = 0;
    for (UK2Node_CallFunction* Call : Calls)
    {
        const FName Name = Call->FunctionReference.GetMemberName();
        ExistingBegin += Name == Begin->GetFName();
        ExistingEnd += Name == End->GetFName();
        ExistingActivity += Name == Activity->GetFName();
        if (Name != TEXT("IgnoreComponentWhenMoving"))
        {
            continue;
        }
        UEdGraphPin* Ignore = Call->FindPin(TEXT("bShouldIgnore"), EGPD_Input);
        UEdGraphPin* Capsule = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        UEdGraphPin* Obstacle = Call->FindPin(TEXT("Component"), EGPD_Input);
        if (!Ignore || !Ignore->LinkedTo.IsEmpty() || !Capsule || Capsule->LinkedTo.Num() != 1 ||
            !Obstacle || Obstacle->LinkedTo.Num() != 1 || Call->GetThenPin()->LinkedTo.Num() != 1)
        {
            return false;
        }
        const bool bStart = Ignore->DefaultValue == TEXT("true");
        auto* Next = Cast<UK2Node_CallFunction>(Call->GetThenPin()->LinkedTo[0]->GetOwningNode());
        const bool bChangesMesh = Next && Next->FunctionReference.GetMemberName() == TEXT("SetCollisionResponseToChannel");
        UEdGraphPin* Mesh = bChangesMesh ? Next->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input) : nullptr;
        if (bChangesMesh && (!Mesh || Mesh->LinkedTo.Num() != 1))
        {
            return false;
        }
        MeshChanges += bChangesMesh;
        FTraversalCallReplacement& Replacement = Replacements.Add_GetRef(
            {Call, bChangesMesh ? Next : Call, bStart ? Begin : End, {}});
        if (bStart)
        {
            Replacement.Inputs.Emplace(TEXT("Capsule"), Capsule);
            Replacement.Inputs.Emplace(TEXT("Obstacle"), Obstacle);
            if (Mesh)
            {
                Replacement.Inputs.Emplace(TEXT("Mesh"), Mesh);
            }
        }
    }

    TArray<UK2Node_VariableSet*> Setters;
    Graph->GetNodesOfClass(Setters);
    int32 ActivityChanges = 0;
    for (UK2Node_VariableSet* Set : Setters)
    {
        if (Set->GetVarName() == TEXT("DoingTraversalAction"))
        {
            UEdGraphPin* Active = Set->FindPin(TEXT("DoingTraversalAction"), EGPD_Input);
            if (!Active || !Active->LinkedTo.IsEmpty())
            {
                return false;
            }
            Replacements.Add({Set, Set, Activity, {{TEXT("bActive"), Active}}});
            ++ActivityChanges;
        }
    }
    if (Replacements.IsEmpty())
    {
        return ExistingBegin == 2 && ExistingEnd == 2 && ExistingActivity == 4;
    }
    if (Replacements.Num() != 8 || MeshChanges != 2 || ActivityChanges != 4 ||
        ExistingBegin || ExistingEnd || ExistingActivity)
    {
        return false;
    }
    for (const FTraversalCallReplacement& Replacement : Replacements)
    {
        if (!InstallTraversalCall(*Blueprint, *Graph, Replacement))
        {
            return false;
        }
    }
    return true;
}
