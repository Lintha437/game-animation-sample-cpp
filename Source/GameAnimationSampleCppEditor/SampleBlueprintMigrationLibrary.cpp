#include "SampleBlueprintMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_EnhancedInputAction.h"
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
