#include "SampleBlueprintMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
    bool CollectIsolatedChain(UEdGraphNode* Event, TSet<UEdGraphNode*>& Remove, const TSet<UEdGraphNode*>* AllowedEntries = nullptr)
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
            if (Current != Event && bHasExecOutput && !bHasExecInput && (!AllowedEntries || !AllowedEntries->Contains(Current)))
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

    TSet<UEdGraphNode*> CollectExecutionNodes(const UEdGraphPin& Start, const UEdGraphNode* Stop = nullptr)
    {
        TArray<UEdGraphPin*> Pending(Start.LinkedTo);
        TSet<UEdGraphNode*> Nodes;
        while (!Pending.IsEmpty())
        {
            UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No)->GetOwningNode();
            if (Node == Stop || Nodes.Contains(Node))
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
        TArray<UEdGraphNode*> AdditionalNodes = {};
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
            (!Then->LinkedTo.IsEmpty() && !Schema->MovePinLinks(*Then, *Call->GetThenPin()).CanSafeConnect()))
        {
            return false;
        }
        TSet<UEdGraphNode*> Remove{Replacement.First, Replacement.Last};
        for (UEdGraphNode* Node : Replacement.AdditionalNodes)
        {
            Remove.Add(Node);
        }
        CollectUnusedInputs(Remove, nullptr);
        RemoveNodes(Blueprint, Graph, Remove);
        return true;
    }

    UEdGraphNode* GetOnlyPeer(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.Num() == 1 ? Pin->LinkedTo[0]->GetOwningNode() : nullptr;
    }

    UK2Node_CallFunction* GetNextCall(const UK2Node_CallFunction& Call, FName Name)
    {
        auto* Next = Cast<UK2Node_CallFunction>(GetOnlyPeer(Call.GetThenPin()));
        return Next && Next->FunctionReference.GetMemberName() == Name ? Next : nullptr;
    }

    UK2Node_CallFunction* FindUniqueCall(UEdGraph& Graph, FName Name)
    {
        TArray<UK2Node_CallFunction*> Calls;
        Graph.GetNodesOfClass(Calls);
        UK2Node_CallFunction* Match = nullptr;
        for (UK2Node_CallFunction* Call : Calls)
        {
            if (Call->FunctionReference.GetMemberName() == Name)
            {
                if (Match)
                {
                    return nullptr;
                }
                Match = Call;
            }
        }
        return Match;
    }

    bool InstallRagdollTail(UBlueprint& Blueprint, UEdGraph& Graph, UK2Node_CallFunction& First,
        const UFunction& Function)
    {
        UEdGraphPin* Exec = First.GetExecPin();
        UEdGraphPin* Previous = Exec && Exec->LinkedTo.Num() == 1 ? Exec->LinkedTo[0] : nullptr;
        if (GetOnlyPeer(Previous) != &First)
        {
            return false;
        }

        TSet<UEdGraphNode*> Remove = CollectExecutionNodes(*Previous);
        for (const UEdGraphNode* Node : Remove)
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                if ((bExec && Pin->Direction == EGPD_Input && Pin != Exec) ||
                    (!bExec && Pin->Direction == EGPD_Output))
                {
                    for (const UEdGraphPin* Peer : Pin->LinkedTo)
                    {
                        if (!Remove.Contains(Peer->GetOwningNode()))
                        {
                            return false;
                        }
                    }
                }
            }
        }

        FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
        UK2Node_CallFunction* Call = Creator.CreateNode();
        Call->SetFromFunction(&Function);
        Call->NodePosX = First.NodePosX;
        Call->NodePosY = First.NodePosY;
        Creator.Finalize();
        if (!Graph.GetSchema()->MovePinLinks(*Exec, *Call->GetExecPin()).CanSafeConnect())
        {
            return false;
        }
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
    return RemoveCustomEvents(Blueprint, Graph, {EventName});
}

bool USampleBlueprintMigrationLibrary::RemoveCustomEvents(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FName>& EventNames)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || EventNames.IsEmpty() || EventNames.Contains(NAME_None))
    {
        return false;
    }
    TSet<FName> MatchedNames;
    TSet<UEdGraphNode*> Entries;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
        if (Event && EventNames.Contains(Event->CustomFunctionName))
        {
            if (MatchedNames.Contains(Event->CustomFunctionName))
            {
                return false;
            }
            MatchedNames.Add(Event->CustomFunctionName);
            Entries.Add(Event);
        }
    }

    if (MatchedNames.Num() != EventNames.Num())
    {
        return false;
    }
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Event : Entries)
    {
        if (!Remove.Contains(Event) && !CollectIsolatedChain(Event, Remove, &Entries))
        {
            return false;
        }
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::RemoveIsolatedEventOverride(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || EventName.IsNone())
    {
        return false;
    }
    UK2Node_Event* Match = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* Event = Cast<UK2Node_Event>(Node); Event && Event->EventReference.GetMemberName() == EventName)
        {
            if (Match)
            {
                return false;
            }
            Match = Event;
        }
    }
    if (!Match)
    {
        return true;
    }
    TSet<UEdGraphNode*> Remove;
    if (!CollectIsolatedChain(Match, Remove))
    {
        return false;
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::ForwardIsolatedEventOverride(UBlueprint* Blueprint, UEdGraph* Graph,
    FName EventName, FName FunctionName)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_Event*> Events;
    Graph->GetNodesOfClass(Events);
    const auto* Match = Events.FindByPredicate([&](const UK2Node_Event* Event)
    {
        if (const auto* CustomEvent = Cast<UK2Node_CustomEvent>(Event))
        {
            return CustomEvent->CustomFunctionName == EventName;
        }
        return Event->EventReference.GetMemberName() == EventName;
    });
    UK2Node_Event* Event = Match ? *Match : nullptr;
    UEdGraphPin* Then = Event ? Event->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    UEdGraphNode* First = GetOnlyPeer(Then);
    if (!Function || !First)
    {
        return false;
    }
    if (const auto* Existing = Cast<UK2Node_CallFunction>(First);
        Existing && Existing->FunctionReference.GetMemberName() == FunctionName)
    {
        return true;
    }
    TSet<UEdGraphNode*> Remove;
    if (!CollectIsolatedChain(Event, Remove))
    {
        return false;
    }
    Remove.Remove(Event);

    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Call = Creator.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = First->NodePosX;
    Call->NodePosY = First->NodePosY;
    Creator.Finalize();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    for (UEdGraphPin* Pin : Call->Pins)
    {
        if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Self &&
            Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            UEdGraphPin* Input = Event->FindPin(Pin->PinName, EGPD_Output);
            if (!Input || !Schema->TryCreateConnection(Input, Pin))
            {
                return false;
            }
        }
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return Schema->TryCreateConnection(Then, Call->GetExecPin());
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

bool USampleBlueprintMigrationLibrary::ReplaceTraversalModes(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const UClass* Parent = Blueprint->ParentClass;
    const UFunction* BeginCmc = Parent->FindFunctionByName(TEXT("BeginCmcTraversalMode"));
    const UFunction* FinishCmc = Parent->FindFunctionByName(TEXT("FinishCmcTraversalMode"));
    const UFunction* BeginMover = Parent->FindFunctionByName(TEXT("BeginMoverTraversalMode"));
    const UFunction* FinishMover = Parent->FindFunctionByName(TEXT("FinishMoverTraversalMode"));
    if (!BeginCmc || !FinishCmc || !BeginMover || !FinishMover)
    {
        return false;
    }

    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    const auto Count = [&](FName Name)
    {
        return Calls.FilterByPredicate([&](const UK2Node_CallFunction* Call)
        {
            return Call->FunctionReference.GetMemberName() == Name;
        }).Num();
    };
    if (Count(TEXT("BeginTraversalCollision")) != 2 || Count(TEXT("EndTraversalCollision")) != 2)
    {
        return false;
    }
    if (Count(BeginCmc->GetFName()) == 1 && Count(FinishCmc->GetFName()) == 1 &&
        Count(BeginMover->GetFName()) == 1 && Count(FinishMover->GetFName()) == 1)
    {
        return true;
    }
    if (Count(TEXT("SetMovementMode_CMC")) != 2 || Count(TEXT("QueueNextMode")) != 2 ||
        Count(TEXT("RetriggerableDelay")) != 1)
    {
        return false;
    }

    TArray<FTraversalCallReplacement> Replacements;
    for (UK2Node_CallFunction* Call : Calls)
    {
        const FName Name = Call->FunctionReference.GetMemberName();
        if (Name == TEXT("SetMovementMode_CMC"))
        {
            if (UK2Node_CallFunction* Delay = GetNextCall(*Call, TEXT("RetriggerableDelay")))
            {
                auto* Correction = GetNextCall(*Delay, TEXT("SetReplicationBehavior"));
                if (!Correction)
                {
                    return false;
                }
                Replacements.Add({Call, Correction, FinishCmc, {}, {Delay}});
            }
            else if (UK2Node_CallFunction* Correction = GetNextCall(*Call, TEXT("SetReplicationBehavior")))
            {
                Replacements.Add({Call, Correction, BeginCmc, {}});
            }
        }
        else if (Name == TEXT("QueueNextMode"))
        {
            const UEdGraphPin* Mode = Call->FindPin(TEXT("DesiredModeName"), EGPD_Input);
            UEdGraphPin* Mover = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
            if (!Mode || !Mode->LinkedTo.IsEmpty() || !Mover || Mover->LinkedTo.Num() != 1)
            {
                return false;
            }
            if (Mode->DefaultValue == TEXT("Traversing"))
            {
                Replacements.Add({Call, Call, BeginMover, {{TEXT("Mover"), Mover}}});
            }
            else if (Mode->DefaultValue == TEXT("Walking"))
            {
                auto* Gate = Cast<UK2Node_IfThenElse>(GetOnlyPeer(Call->GetExecPin()));
                UEdGraphPin* Then = Gate ? Gate->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
                if (!Gate || GetOnlyPeer(Then) != Call || !Gate->GetElsePin()->LinkedTo.IsEmpty())
                {
                    return false;
                }
                Replacements.Add({Gate, Call, FinishMover, {}});
            }
        }
    }
    for (const UFunction* Function : {BeginCmc, FinishCmc, BeginMover, FinishMover})
    {
        if (Replacements.FilterByPredicate([&](const FTraversalCallReplacement& Item)
        {
            return Item.Function == Function;
        }).Num() != 1)
        {
            return false;
        }
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

bool USampleBlueprintMigrationLibrary::ReplaceTraversalReplay(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    static const FName NativeName(TEXT("ApplyTraversalReplay"));
    static const FName ResultName(TEXT("TraversalResult"));
    const UFunction* NativeReplay = Blueprint->ParentClass->FindFunctionByName(NativeName);
    TArray<UK2Node_CustomEvent*> Events;
    Graph->GetNodesOfClass(Events);
    const auto* Match = Events.FindByPredicate([](const UK2Node_CustomEvent* Event)
    {
        return Event->CustomFunctionName == TEXT("PerformTraversalAction_Clients");
    });
    UEdGraphPin* EventExec = Match ? (*Match)->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    UEdGraphNode* Body = GetOnlyPeer(EventExec);
    if (!NativeReplay || !Body)
    {
        return false;
    }
    if (const auto* Call = Cast<UK2Node_CallFunction>(Body);
        Call && Call->FunctionReference.GetMemberName() == NativeName)
    {
        return true;
    }
    const auto* CastOwner = Cast<UK2Node_DynamicCast>(Body);
    UEdGraphPin* Then = CastOwner ? CastOwner->GetValidCastPin() : nullptr;
    auto* Store = Cast<UK2Node_VariableSet>(CastOwner ? GetOnlyPeer(Then) : Body);
    auto* Playback = Store ? Cast<UK2Node_CallFunction>(
        GetOnlyPeer(Store->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output))) : nullptr;
    UEdGraphPin* Payload = Store ? Store->FindPin(ResultName, EGPD_Input) : nullptr;
    if (!Store || Store->GetVarName() != ResultName || !Payload || Payload->LinkedTo.Num() != 1 ||
        !Playback || Playback->FunctionReference.GetMemberName() != TEXT("PerformTraversalAction") ||
        (CastOwner && !CastOwner->GetInvalidCastPin()->LinkedTo.IsEmpty()))
    {
        return false;
    }
    FTraversalCallReplacement Replacement{Body, Playback, NativeReplay, {{ResultName, Payload}}};
    Replacement.AdditionalNodes.Add(Store);
    return InstallTraversalCall(*Blueprint, *Graph, Replacement);
}

bool USampleBlueprintMigrationLibrary::ReplaceEnumQuery(UBlueprint* Blueprint, UEdGraph* Graph, FName FunctionName)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const UFunction* Native = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    if (!Native || !Native->HasAllFunctionFlags(FUNC_Native | FUNC_BlueprintPure) || Native->NumParms != 1 ||
        !CastField<FByteProperty>(Native->GetReturnProperty()) || Entries.Num() != 1 || Results.IsEmpty())
    {
        return false;
    }
    UK2Node_FunctionEntry* Entry = Entries[0];
    UK2Node_FunctionResult* Result = Results[0];
    UEdGraphPin* Output = Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Input);
    UEnum* Definition = Output ? Cast<UEnum>(Output->PinType.PinSubCategoryObject.Get()) : nullptr;
    if (!Definition || Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Byte)
    {
        return false;
    }

    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node != Entry && Node != Result)
        {
            OldBody.Add(Node);
        }
    }
    RemoveNodes(*Blueprint, *Graph, OldBody);

    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*Graph);
    UK2Node_CallFunction* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Native);
    Call->NodePosX = Entry->NodePosX + 240;
    Call->NodePosY = Entry->NodePosY + 100;
    CreateCall.Finalize();

    FGraphNodeCreator<UK2Node_CastByteToEnum> CreateConversion(*Graph);
    UK2Node_CastByteToEnum* Convert = CreateConversion.CreateNode();
    Convert->Enum = Definition;
    Convert->bSafe = true;
    Convert->NodePosX = Call->NodePosX + 310;
    Convert->NodePosY = Call->NodePosY;
    CreateConversion.Finalize();
    Result->NodePosX = Convert->NodePosX + 230;
    Result->NodePosY = Entry->NodePosY;

    const UEdGraphSchema* Schema = Graph->GetSchema();
    UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UEdGraphPin* Exec = Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    return (!Then || !Exec || Schema->TryCreateConnection(Then, Exec)) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Convert->FindPin(TEXT("Byte"), EGPD_Input)) &&
        Schema->TryCreateConnection(Convert->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Output);
}

bool USampleBlueprintMigrationLibrary::ReplaceMoverInputFunction(
    UBlueprint* Blueprint, UEdGraph* Graph, FName FunctionName, FName OutputName)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const UFunction* Native = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    if (!Native || Entries.Num() != 1 || (!OutputName.IsNone() && Results.IsEmpty()))
    {
        return false;
    }
    auto* Entry = Entries[0];
    auto* Result = Results.IsEmpty() ? nullptr : Results[0];
    UEdGraphPin* Output = Result ? Result->FindPin(OutputName, EGPD_Input) : nullptr;
    UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UEdGraphPin* Exec = Result ? Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input) : nullptr;
    if ((!OutputName.IsNone() && !Output) || (Native->GetReturnProperty() != nullptr) != (Output != nullptr))
    {
        return false;
    }
    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node != Entry && Node != Result)
        {
            OldBody.Add(Node);
        }
    }
    if (OldBody.Num() == 1)
    {
        const auto* Forward = Cast<UK2Node_CallFunction>(*OldBody.CreateConstIterator());
        if (Forward && Forward->FunctionReference.GetMemberName() == FunctionName)
        {
            return true;
        }
    }

    RemoveNodes(*Blueprint, *Graph, OldBody);
    Entry->Modify();
    Entry->LocalVariables.Reset();
    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Call = Creator.CreateNode();
    Call->SetFromFunction(Native);
    Call->NodePosX = Entry->NodePosX + 350;
    Call->NodePosY = Entry->NodePosY;
    Creator.Finalize();
    if (Result)
    {
        Result->NodePosX = Call->NodePosX + 400;
        Result->NodePosY = Entry->NodePosY;
    }
    const UEdGraphSchema* Schema = Graph->GetSchema();
    for (UEdGraphPin* Pin : Entry->Pins)
    {
        if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            UEdGraphPin* Input = Call->FindPin(Pin->PinName, EGPD_Input);
            if (!Input)
            {
                const FString NativeName = Pin->PinName.ToString().Replace(TEXT(" "), TEXT(""));
                Input = Call->FindPin(FName(*NativeName), EGPD_Input);
            }
            if (!Input || !Input->LinkedTo.IsEmpty() || !Schema->TryCreateConnection(Pin, Input))
            {
                return false;
            }
        }
    }
    if (Output && !Schema->TryCreateConnection(Call->GetReturnValuePin(), Output))
    {
        return false;
    }
    if (Call->IsNodePure())
    {
        return !Then || !Exec || Schema->TryCreateConnection(Then, Exec);
    }
    return Then && Schema->TryCreateConnection(Then, Call->GetExecPin()) &&
        (!Exec || Schema->TryCreateConnection(Call->GetThenPin(), Exec));
}

bool USampleBlueprintMigrationLibrary::ReplaceRagdollLifecycle(
    UBlueprint* Blueprint, UEdGraph* EventGraph, UEdGraph* ModeGraph)
{
    if (!Blueprint || !Blueprint->ParentClass || !EventGraph || !ModeGraph ||
        EventGraph->GetOuter() != Blueprint || ModeGraph->GetOuter() != Blueprint)
    {
        return false;
    }

    const auto InstallTail = [&](FName Native, UK2Node_CallFunction* First)
    {
        if (FindUniqueCall(*EventGraph, Native))
        {
            return true;
        }
        const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(Native);
        return First && Function && InstallRagdollTail(*Blueprint, *EventGraph, *First, *Function);
    };
    TArray<UK2Node_CustomEvent*> Events;
    EventGraph->GetNodesOfClass(Events);
    const auto* Exit = Events.FindByPredicate([](const UK2Node_CustomEvent* Event)
    {
        return Event->CustomFunctionName == TEXT("ExitRagdoll");
    });
    auto* ExitQueue = Cast<UK2Node_CallFunction>(Exit ? GetOnlyPeer(
        (*Exit)->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output)) : nullptr);
    if (!InstallTail(TEXT("BeginRagdollPhysics"), FindUniqueCall(*EventGraph, TEXT("SetSimulatePhysics"))) ||
        !InstallTail(TEXT("FinishRagdollPhysicsRecovery"), FindUniqueCall(*EventGraph, TEXT("SetControlDatasInSet"))) ||
        !InstallTail(TEXT("RequestRagdollRecovery"), ExitQueue && ExitQueue->FunctionReference.GetMemberName() == TEXT("QueueNextMode")
            ? ExitQueue : nullptr))
    {
        return false;
    }

    const auto InstallCollision = [&](UEdGraph& Graph, FName Native, bool bEnter)
    {
        if (FindUniqueCall(Graph, Native))
        {
            return true;
        }
        const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(Native);
        UK2Node_CallFunction* First = FindUniqueCall(Graph, TEXT("SetCollisionProfileName"));
        auto* Last = bEnter && First ? GetNextCall(*First, TEXT("SetCollisionResponseToChannel")) : First;
        return Function && First && Last && InstallTraversalCall(*Blueprint, Graph, {First, Last, Function, {}});
    };
    if (!InstallCollision(*ModeGraph, TEXT("ApplyRagdollCapsuleCollision"), true) ||
        !InstallCollision(*EventGraph, TEXT("RestoreRagdollCapsuleCollision"), false))
    {
        return false;
    }

    static const FName OldCheck(TEXT("Entered Ragdoll Successfully Check"));
    return !Events.ContainsByPredicate([](const UK2Node_CustomEvent* Event)
        {
            return Event->CustomFunctionName == OldCheck;
        }) || RemoveCustomEvent(Blueprint, EventGraph, OldCheck);
}

bool USampleBlueprintMigrationLibrary::ReplacePlayerSmartObjectInput(UBlueprint* Blueprint, UEdGraph* EventGraph)
{
    if (!Blueprint || !Blueprint->ParentClass || !EventGraph || EventGraph->GetOuter() != Blueprint)
    {
        return false;
    }

    const FName FunctionName(TEXT("TryStartPlayerInteraction"));
    const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    if (FindUniqueCall(*EventGraph, FunctionName))
    {
        return true;
    }
    TArray<UK2Node_EnhancedInputAction*> Actions;
    EventGraph->GetNodesOfClass(Actions);
    auto* Action = Actions.FindByPredicate([](const UK2Node_EnhancedInputAction* Candidate)
    {
        return Candidate->InputAction && Candidate->InputAction->GetPathName() == TEXT("/Game/Input/IA_Interact.IA_Interact");
    });
    UEdGraphPin* Trigger = Action ? (*Action)->FindPin(TEXT("Triggered"), EGPD_Output) : nullptr;
    if (!Function || !GetOnlyPeer(Trigger))
    {
        UE_LOG(LogTemp, Warning, TEXT("Player interaction input or native request function is missing."));
        return false;
    }

    UEdGraphPin* GateOpen = nullptr;
    for (UEdGraphNode* Node : CollectExecutionNodes(*Trigger))
    {
        const auto* Flag = Cast<UK2Node_VariableSet>(Node);
        const UEdGraphPin* Then = Flag ? Flag->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
        if (Flag && Flag->GetVarName() == TEXT("PlayerIsInteracting") && Then && Then->LinkedTo.Num() == 1)
        {
            UEdGraphPin* GateInput = Then->LinkedTo[0];
            const UEdGraphPin* Endpoint = GateInput;
            TSet<const UEdGraphNode*> Reroutes;
            while (const auto* Reroute = Cast<UK2Node_Knot>(Endpoint->GetOwningNode()))
            {
                if (Reroutes.Contains(Reroute) || Reroute->GetOutputPin()->LinkedTo.Num() != 1)
                {
                    return false;
                }
                Reroutes.Add(Reroute);
                Endpoint = Reroute->GetOutputPin()->LinkedTo[0];
            }
            if (Endpoint->PinName != TEXT("Open"))
            {
                continue;
            }
            if (GateOpen)
            {
                return false;
            }
            GateOpen = GateInput;
        }
    }
    if (!GateOpen)
    {
        UE_LOG(LogTemp, Warning, TEXT("Player interaction did not lead to the playback cancellation gate."));
        return false;
    }

    TSet<UEdGraphNode*> Remove = CollectExecutionNodes(*Trigger, GateOpen->GetOwningNode());
    CollectUnusedInputs(Remove, *Action);
    for (const UEdGraphNode* Node : Remove)
    {
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
            if (bExec || Pin->Direction == EGPD_Output)
            {
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    if (Peer != Trigger && Peer != GateOpen && !Remove.Contains(Peer->GetOwningNode()))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Player interaction shares pin %s.%s with %s.%s."),
                            *Node->GetName(), *Pin->PinName.ToString(), *Peer->GetOwningNode()->GetName(), *Peer->PinName.ToString());
                        return false;
                    }
                }
            }
        }
    }

    const UEdGraphNode* First = GetOnlyPeer(Trigger);
    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*EventGraph);
    UK2Node_CallFunction* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = First->NodePosX;
    Call->NodePosY = First->NodePosY;
    CreateCall.Finalize();
    FGraphNodeCreator<UK2Node_IfThenElse> CreateBranch(*EventGraph);
    UK2Node_IfThenElse* Branch = CreateBranch.CreateNode();
    Branch->NodePosX = Call->NodePosX + 350;
    Branch->NodePosY = Call->NodePosY;
    CreateBranch.Finalize();

    RemoveNodes(*Blueprint, *EventGraph, Remove);
    const UEdGraphSchema* Schema = EventGraph->GetSchema();
    return Schema->TryCreateConnection(Trigger, Call->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetThenPin(), Branch->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Branch->FindPin(UEdGraphSchema_K2::PN_Condition)) &&
        Schema->TryCreateConnection(Branch->FindPin(UEdGraphSchema_K2::PN_Then), GateOpen);
}
